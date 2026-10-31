package send

import (
	"bytes"
	"compress/gzip"
	"errors"
	"io"
	"math/rand/v2"
	"net/http"
	"net/http/httptest"
	"slices"
	"strconv"
	"sync/atomic"
	"testing"
	"time"
)

const helloSHA = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"

func TestSignatureTestVector(t *testing.T) {
	if hexSHA([]byte("hello")) != helloSHA {
		t.Fatal("content hash of hello differs from the contract")
	}
	if hexSHA(nil) != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" {
		t.Fatal("content hash of the empty body differs from the contract")
	}
	got := sign([]byte("test-secret"), "PUT", "/files/"+helloSHA+"/pieces/0", 1793563200, helloSHA)
	if got != "d2af137f20a18b26fe7ee727c7fa683ec1e570cf724f8bca9df6b975a211829a" {
		t.Fatalf("signature %s differs from the contract's test vector", got)
	}
}

type captured struct {
	method, path string
	header       http.Header
	length       int64
	body         []byte
}

func capture(status int) (http.Handler, chan captured) {
	got := make(chan captured, 1)
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		got <- captured{r.Method, r.URL.EscapedPath(), r.Header.Clone(), r.ContentLength, body}
		w.WriteHeader(status)
	}), got
}

func TestRequestCarriesTheSignedHeaders(t *testing.T) {
	h, got := capture(http.StatusCreated)
	c := newTestClient(t, h, newFakeClock(), "test-secret")
	if err := c.PutPiece(helloSHA, 0, []byte("hello")); err != nil {
		t.Fatal(err)
	}
	r := <-got
	want := map[string]string{
		"X-LL-Server":         testServer,
		"X-LL-Timestamp":      "1793563200",
		"X-LL-Content-SHA256": helloSHA,
		"X-LL-Signature":      "d2af137f20a18b26fe7ee727c7fa683ec1e570cf724f8bca9df6b975a211829a",
		"Content-Type":        "application/octet-stream",
	}
	for k, v := range want {
		if r.header.Get(k) != v {
			t.Errorf("%s = %q, want %q", k, r.header.Get(k), v)
		}
	}
	if r.method != http.MethodPut || r.path != "/files/"+helloSHA+"/pieces/0" || r.length != 5 || string(r.body) != "hello" {
		t.Errorf("request = %s %s length %d body %q", r.method, r.path, r.length, r.body)
	}
}

func TestEmptyRequestSignsTheEmptyHash(t *testing.T) {
	h, got := capture(http.StatusNotFound)
	c := newTestClient(t, h, newFakeClock(), testSecret)
	if _, err := c.GetFile(helloSHA); err != nil {
		t.Fatal(err)
	}
	r := <-got
	empty := hexSHA(nil)
	if r.header.Get("X-LL-Content-SHA256") != empty || r.header.Get("Content-Type") != "" {
		t.Errorf("headers = %v", r.header)
	}
	if want := sign([]byte(testSecret), "GET", "/files/"+helloSHA, 1793563200, empty); r.header.Get("X-LL-Signature") != want {
		t.Errorf("signature = %s, want %s", r.header.Get("X-LL-Signature"), want)
	}
}

func TestReceiverRefusesBeforeReadingTheBody(t *testing.T) {
	clock := newFakeClock()
	now := clock.Now().Unix()
	path := "/files/" + helloSHA + "/pieces/0"
	cases := []struct {
		name, server, secret string
		ts, length           int64
		noSignature          bool
		want                 int
	}{
		{"accepted", testServer, testSecret, now, 5, false, http.StatusCreated},
		{"unknown server", "server-2", testSecret, now, 5, false, http.StatusUnauthorized},
		{"timestamp too old", testServer, testSecret, now - 301, 5, false, http.StatusUnauthorized},
		{"timestamp too new", testServer, testSecret, now + 301, 5, false, http.StatusUnauthorized},
		{"wrong secret", testServer, "another-secret-of-at-least-32-chars", now, 5, false, http.StatusUnauthorized},
		{"no signature", testServer, testSecret, now, 5, true, http.StatusUnauthorized},
		{"no length", testServer, testSecret, now, -1, false, http.StatusRequestEntityTooLarge},
		{"over the piece limit", testServer, testSecret, now, PieceSize + 1, false, http.StatusRequestEntityTooLarge},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			rc := newReceiver(clock)
			body := &countingReader{r: bytes.NewReader([]byte("hello"))}
			r := httptest.NewRequest(http.MethodPut, "https://example.org"+path, body)
			r.ContentLength = c.length
			r.Header.Set("X-LL-Server", c.server)
			r.Header.Set("X-LL-Timestamp", strconv.FormatInt(c.ts, 10))
			r.Header.Set("X-LL-Content-SHA256", helloSHA)
			if !c.noSignature {
				r.Header.Set("X-LL-Signature", sign([]byte(c.secret), http.MethodPut, path, c.ts, helloSHA))
			}
			w := httptest.NewRecorder()
			rc.ServeHTTP(w, r)
			if w.Code != c.want {
				t.Fatalf("answer %d, want %d", w.Code, c.want)
			}
			if c.want != http.StatusCreated && body.n != 0 {
				t.Fatalf("read %d body bytes before refusing", body.n)
			}
		})
	}
}

func TestReceiverRefusesABodyThatDoesNotMatchItsHash(t *testing.T) {
	clock := newFakeClock()
	rc := newReceiver(clock)
	path := "/files/" + helloSHA + "/pieces/0"
	ts := clock.Now().Unix()
	claimed := hexSHA([]byte("other"))
	r := httptest.NewRequest(http.MethodPut, "https://example.org"+path, bytes.NewReader([]byte("hello")))
	r.Header.Set("X-LL-Server", testServer)
	r.Header.Set("X-LL-Timestamp", strconv.FormatInt(ts, 10))
	r.Header.Set("X-LL-Content-SHA256", claimed)
	r.Header.Set("X-LL-Signature", sign([]byte(testSecret), http.MethodPut, path, ts, claimed))
	w := httptest.NewRecorder()
	rc.ServeHTTP(w, r)
	if w.Code != http.StatusBadRequest {
		t.Fatalf("answer %d, want 400", w.Code)
	}
}

func randomBytes(n int, seed uint64) []byte {
	rnd := rand.New(rand.NewPCG(seed, seed+1))
	b := make([]byte, n)
	for i := range b {
		b[i] = byte(rnd.Uint32())
	}
	return b
}

func TestFileLifecycle(t *testing.T) {
	clock := newFakeClock()
	rc := newReceiver(clock)
	c := newTestClient(t, rc, clock, testSecret)
	enc, err := Encode(randomBytes(3_200_000, 7))
	if err != nil {
		t.Fatal(err)
	}
	if len(enc.Pieces) != 3 {
		t.Fatalf("%d pieces, want 3", len(enc.Pieces))
	}
	commit := FileCommit{Kind: "segment", Session: "s", Name: "s-1.llr", Size: enc.Size, Encoding: "gzip", EncodedSize: enc.EncodedSize, EncodedSHA256: enc.EncodedSHA256, Pieces: 3}

	held, err := c.GetFile(enc.Identity)
	if err != nil || held.Complete || len(held.Pieces) != 0 {
		t.Fatalf("unknown file: %+v, %v", held, err)
	}
	for _, i := range []int{0, 2} {
		if err := c.PutPiece(enc.Identity, i, enc.Pieces[i]); err != nil {
			t.Fatal(err)
		}
	}
	if err := c.PutPiece(enc.Identity, 0, enc.Pieces[0]); err != nil {
		t.Fatalf("same piece again: %v", err)
	}
	var e *Error
	if err := c.PutPiece(enc.Identity, 0, enc.Pieces[1]); !errors.As(err, &e) || e.Kind != KindInvalid || e.Status != http.StatusConflict {
		t.Fatalf("other bytes for a held piece: %v", err)
	}
	if held, err = c.GetFile(enc.Identity); err != nil || held.Complete || !slices.Equal(held.Pieces, []int{0, 2}) {
		t.Fatalf("partial file: %+v, %v", held, err)
	}
	if err := c.Commit(enc.Identity, commit); !errors.As(err, &e) || e.Kind != KindMissing || !slices.Equal(e.Pieces, []int{1}) {
		t.Fatalf("commit with a missing piece: %v", err)
	}
	if err := c.PutPiece(enc.Identity, 1, enc.Pieces[1]); err != nil {
		t.Fatal(err)
	}
	wrong := commit
	wrong.EncodedSHA256 = hexSHA(nil)
	if err := c.Commit(enc.Identity, wrong); !errors.As(err, &e) || e.Kind != KindInvalid || e.Status != http.StatusUnprocessableEntity {
		t.Fatalf("commit that does not match: %v", err)
	}
	for i, p := range enc.Pieces {
		if err := c.PutPiece(enc.Identity, i, p); err != nil {
			t.Fatal(err)
		}
	}
	if err := c.Commit(enc.Identity, commit); err != nil {
		t.Fatal(err)
	}
	if err := c.Commit(enc.Identity, commit); err != nil {
		t.Fatalf("second commit: %v", err)
	}
	if held, err = c.GetFile(enc.Identity); err != nil || !held.Complete {
		t.Fatalf("complete file: %+v, %v", held, err)
	}
	rc.with(func() {
		if rc.commits[enc.Identity] != 1 {
			t.Fatalf("stored %d times", rc.commits[enc.Identity])
		}
	})
}

func TestPostGameNamesTheMissingFiles(t *testing.T) {
	clock := newFakeClock()
	rc := newReceiver(clock)
	c := newTestClient(t, rc, clock, testSecret)
	stats := hexSHA([]byte("stats"))
	g := GamePost{Session: "20261031-201500-eden", Over: "stopped", Segments: []string{helloSHA}, Stats: &stats}
	var e *Error
	if err := c.PostGame(g); !errors.As(err, &e) || e.Kind != KindMissing || !slices.Equal(e.Files, []string{helloSHA, stats}) {
		t.Fatalf("post naming uncommitted files: %v", err)
	}
	rc.with(func() { rc.complete[helloSHA], rc.complete[stats] = true, true })
	if err := c.PostGame(g); err != nil {
		t.Fatal(err)
	}
	rc.with(func() {
		if got := rc.games[g.Session]; got.Identity != nil || *got.Stats != stats || !slices.Equal(got.Segments, g.Segments) {
			t.Fatalf("receiver got %+v", got)
		}
	})
}

func TestAnswersAreClassified(t *testing.T) {
	clock := newFakeClock()
	when := clock.Now().Add(90 * time.Second).UTC().Format(http.TimeFormat)
	cases := []struct {
		status     int
		retryAfter string
		kind       Kind
		wait       time.Duration
	}{
		{http.StatusUnauthorized, "", KindAuth, 0},
		{http.StatusBadRequest, "", KindInvalid, 0},
		{http.StatusRequestEntityTooLarge, "", KindInvalid, 0},
		{http.StatusUnprocessableEntity, "", KindInvalid, 0},
		{http.StatusTooManyRequests, "7", KindRateLimited, 7 * time.Second},
		{http.StatusTooManyRequests, when, KindRateLimited, 90 * time.Second},
		{http.StatusTooManyRequests, "", KindRateLimited, 0},
		{http.StatusTooManyRequests, "99999999999", KindRateLimited, maxRetryAfter},
		{http.StatusInternalServerError, "", KindTransient, 0},
		{http.StatusServiceUnavailable, "", KindTransient, 0},
		{http.StatusNotFound, "", KindTransient, 0},
	}
	for _, c := range cases {
		h := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			if c.retryAfter != "" {
				w.Header().Set("Retry-After", c.retryAfter)
			}
			w.WriteHeader(c.status)
		})
		client := newTestClient(t, h, clock, testSecret)
		err := client.PutPiece(helloSHA, 0, []byte("hello"))
		var e *Error
		if !errors.As(err, &e) {
			t.Errorf("%d %q: got %v", c.status, c.retryAfter, err)
			continue
		}
		if e.Kind != c.kind || e.Status != c.status || e.RetryAfter != c.wait {
			t.Errorf("%d %q: got %v (wait %v), want %s (wait %v)", c.status, c.retryAfter, err, e.RetryAfter, c.kind, c.wait)
		}
	}
}

func TestUnreachableReceiverIsTransient(t *testing.T) {
	srv := httptest.NewTLSServer(http.NotFoundHandler())
	clock := newFakeClock()
	c := NewClient(srv.URL, testServer, testSecret, NewLimiter(1_000_000, clock), srv.Client().Transport, clock)
	srv.Close()
	var e *Error
	if _, err := c.GetFile(helloSHA); !errors.As(err, &e) || e.Kind != KindTransient || e.Status != 0 {
		t.Fatalf("got %v", err)
	}
}

func TestRedirectIsNotFollowed(t *testing.T) {
	var elsewhere atomic.Int32
	other := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		elsewhere.Add(1)
		w.WriteHeader(http.StatusCreated)
	}))
	defer other.Close()
	h := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, other.URL+r.URL.Path, http.StatusTemporaryRedirect)
	})
	c := newTestClient(t, h, newFakeClock(), testSecret)
	var e *Error
	if err := c.PutPiece(helloSHA, 0, []byte("hello")); !errors.As(err, &e) || e.Kind != KindTransient || e.Status != http.StatusTemporaryRedirect {
		t.Fatalf("got %v", err)
	}
	if elsewhere.Load() != 0 {
		t.Fatal("the redirect was followed")
	}
}

func TestEncodeIsDeterministic(t *testing.T) {
	raw := randomBytes(3_200_000, 3)
	a, err := Encode(raw)
	if err != nil {
		t.Fatal(err)
	}
	b, err := Encode(slices.Clone(raw))
	if err != nil {
		t.Fatal(err)
	}
	if len(a.Pieces) != len(b.Pieces) || a.EncodedSHA256 != b.EncodedSHA256 {
		t.Fatal("the same content encoded differently")
	}
	joined := bytes.Join(a.Pieces, nil)
	if joined[3]&0x08 != 0 || !bytes.Equal(joined[4:8], []byte{0, 0, 0, 0}) {
		t.Errorf("gzip header carries a name or a time: % x", joined[:10])
	}
	for i, p := range a.Pieces {
		if len(p) > PieceSize || (i < len(a.Pieces)-1 && len(p) != PieceSize) {
			t.Errorf("piece %d is %d bytes", i, len(p))
		}
	}
	if a.Identity != hexSHA(raw) || a.Size != int64(len(raw)) || a.EncodedSize != int64(len(joined)) || a.EncodedSHA256 != hexSHA(joined) {
		t.Errorf("encoded = identity %s size %d encoded %d %s", a.Identity, a.Size, a.EncodedSize, a.EncodedSHA256)
	}
	zr, err := gzip.NewReader(bytes.NewReader(joined))
	if err != nil {
		t.Fatal(err)
	}
	back, err := io.ReadAll(zr)
	if err != nil || !bytes.Equal(back, raw) {
		t.Fatal("pieces do not decompress to the file")
	}

	small, err := Encode([]byte("LLR 1\nH session s\nSTOP shutdown\n"))
	if err != nil || len(small.Pieces) != 1 {
		t.Fatalf("small file: %+v, %v", small, err)
	}
	empty, err := Encode(nil)
	if err != nil || len(empty.Pieces) != 1 || empty.Size != 0 {
		t.Fatalf("empty file: %+v, %v", empty, err)
	}
}
