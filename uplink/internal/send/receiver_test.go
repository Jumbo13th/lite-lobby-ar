package send

import (
	"bytes"
	"compress/gzip"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"io"
	"maps"
	"net/http"
	"net/http/httptest"
	"slices"
	"strconv"
	"sync"
	"testing"
	"time"
)

const (
	testServer = "server-1"
	testSecret = "a-test-secret-of-at-least-32-characters"
)

type fakeClock struct {
	mu sync.Mutex
	t  time.Time
}

func newFakeClock() *fakeClock { return &fakeClock{t: time.Unix(1793563200, 0)} }

func (c *fakeClock) Now() time.Time {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.t
}

func (c *fakeClock) Sleep(d time.Duration) { c.Advance(d) }

func (c *fakeClock) Advance(d time.Duration) {
	c.mu.Lock()
	c.t = c.t.Add(d)
	c.mu.Unlock()
}

func hexSHA(b []byte) string {
	sum := sha256.Sum256(b)
	return hex.EncodeToString(sum[:])
}

type receiver struct {
	clock   Clock
	secrets map[string]string
	mux     *http.ServeMux

	mu         sync.Mutex
	pieces     map[string]map[int][]byte
	complete   map[string]bool
	commits    map[string]int
	puts       map[string]int
	games      map[string]GamePost
	posts      int
	requests   int
	signatures []string

	down       bool
	outageIn   int
	limited    int
	retryAfter string
	corrupt    int
}

func newReceiver(clock Clock) *receiver {
	rc := &receiver{
		clock:    clock,
		secrets:  map[string]string{testServer: testSecret},
		mux:      http.NewServeMux(),
		pieces:   map[string]map[int][]byte{},
		complete: map[string]bool{},
		commits:  map[string]int{},
		puts:     map[string]int{},
		games:    map[string]GamePost{},
	}
	rc.mux.HandleFunc("GET /files/{id}", rc.getFile)
	rc.mux.HandleFunc("PUT /files/{id}/pieces/{index}", rc.putPiece)
	rc.mux.HandleFunc("POST /files/{id}", rc.commit)
	rc.mux.HandleFunc("POST /games/{session}", rc.postGame)
	return rc
}

func (rc *receiver) with(f func()) {
	rc.mu.Lock()
	defer rc.mu.Unlock()
	f()
}

func newTestClient(t *testing.T, h http.Handler, clock Clock, secret string) *Client {
	t.Helper()
	srv := httptest.NewTLSServer(h)
	t.Cleanup(srv.Close)
	return NewClient(srv.URL, testServer, secret, NewLimiter(1_000_000, clock), srv.Client().Transport, clock)
}

type countingReader struct {
	r io.Reader
	n int64
}

func (c *countingReader) Read(p []byte) (int, error) {
	n, err := c.r.Read(p)
	c.n += int64(n)
	return n, err
}

func (rc *receiver) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	rc.mu.Lock()
	defer rc.mu.Unlock()
	rc.requests++
	if rc.down {
		w.WriteHeader(http.StatusServiceUnavailable)
		return
	}
	if rc.outageIn > 0 {
		rc.outageIn--
		rc.down = rc.outageIn == 0
	}
	if rc.limited > 0 {
		rc.limited--
		w.Header().Set("Retry-After", rc.retryAfter)
		w.WriteHeader(http.StatusTooManyRequests)
		return
	}
	if status := rc.authenticate(r); status != 0 {
		w.WriteHeader(status)
		return
	}
	body, err := io.ReadAll(r.Body)
	if err != nil || hexSHA(body) != r.Header.Get("X-LL-Content-SHA256") {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	r.Body = io.NopCloser(bytes.NewReader(body))
	rc.mux.ServeHTTP(w, r)
}

func (rc *receiver) authenticate(r *http.Request) int {
	signature := r.Header.Get("X-LL-Signature")
	rc.signatures = append(rc.signatures, signature)
	secret, ok := rc.secrets[r.Header.Get("X-LL-Server")]
	if !ok {
		return http.StatusUnauthorized
	}
	ts, err := strconv.ParseInt(r.Header.Get("X-LL-Timestamp"), 10, 64)
	if err != nil {
		return http.StatusUnauthorized
	}
	if skew := rc.clock.Now().Unix() - ts; skew > 300 || skew < -300 {
		return http.StatusUnauthorized
	}
	limit := int64(0)
	switch r.Method {
	case http.MethodPut:
		limit = PieceSize
	case http.MethodPost:
		limit = 64 << 10
	}
	if r.ContentLength > limit || (r.Method != http.MethodGet && r.ContentLength < 0) {
		return http.StatusRequestEntityTooLarge
	}
	path := r.URL.EscapedPath()
	if r.URL.RawQuery != "" {
		path += "?" + r.URL.RawQuery
	}
	want := sign([]byte(secret), r.Method, path, ts, r.Header.Get("X-LL-Content-SHA256"))
	if !hmac.Equal([]byte(want), []byte(signature)) {
		return http.StatusUnauthorized
	}
	return 0
}

func (rc *receiver) getFile(w http.ResponseWriter, r *http.Request) {
	id := r.PathValue("id")
	switch {
	case rc.complete[id]:
		writeJSON(w, http.StatusOK, map[string]any{"state": "complete"})
	case len(rc.pieces[id]) > 0:
		held := slices.Sorted(maps.Keys(rc.pieces[id]))
		writeJSON(w, http.StatusOK, map[string]any{"state": "partial", "pieces": held})
	default:
		w.WriteHeader(http.StatusNotFound)
	}
}

func (rc *receiver) putPiece(w http.ResponseWriter, r *http.Request) {
	id := r.PathValue("id")
	index, err := strconv.Atoi(r.PathValue("index"))
	if err != nil || index < 0 {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	body, _ := io.ReadAll(r.Body)
	rc.puts[id]++
	if rc.pieces[id] == nil {
		rc.pieces[id] = map[int][]byte{}
	}
	if held, ok := rc.pieces[id][index]; ok {
		if bytes.Equal(held, body) {
			w.WriteHeader(http.StatusOK)
		} else {
			w.WriteHeader(http.StatusConflict)
		}
		return
	}
	rc.pieces[id][index] = body
	w.WriteHeader(http.StatusCreated)
}

func (rc *receiver) commit(w http.ResponseWriter, r *http.Request) {
	id := r.PathValue("id")
	var c FileCommit
	if json.NewDecoder(r.Body).Decode(&c) != nil || c.Pieces < 1 {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	if rc.complete[id] {
		w.WriteHeader(http.StatusOK)
		return
	}
	var missing []int
	var joined []byte
	for i := range c.Pieces {
		piece, ok := rc.pieces[id][i]
		if !ok {
			missing = append(missing, i)
		}
		joined = append(joined, piece...)
	}
	if len(missing) > 0 {
		writeJSON(w, http.StatusConflict, map[string]any{"missing": missing})
		return
	}
	if rc.corrupt > 0 {
		rc.corrupt--
		joined[len(joined)/2] ^= 0xff
	}
	if int64(len(joined)) != c.EncodedSize || hexSHA(joined) != c.EncodedSHA256 || !inflatesTo(joined, id, c.Size) {
		delete(rc.pieces, id)
		w.WriteHeader(http.StatusUnprocessableEntity)
		return
	}
	rc.complete[id] = true
	rc.commits[id]++
	w.WriteHeader(http.StatusCreated)
}

func (rc *receiver) postGame(w http.ResponseWriter, r *http.Request) {
	session := r.PathValue("session")
	var g GamePost
	if json.NewDecoder(r.Body).Decode(&g) != nil || g.Session != session {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	named := slices.Clone(g.Segments)
	for _, id := range []*string{g.Identity, g.Stats} {
		if id != nil {
			named = append(named, *id)
		}
	}
	var missing []string
	for _, id := range named {
		if !rc.complete[id] {
			missing = append(missing, id)
		}
	}
	if len(missing) > 0 {
		writeJSON(w, http.StatusConflict, map[string]any{"missing": missing})
		return
	}
	_, seen := rc.games[session]
	rc.games[session] = g
	rc.posts++
	if seen {
		w.WriteHeader(http.StatusOK)
	} else {
		w.WriteHeader(http.StatusCreated)
	}
}

func inflatesTo(encoded []byte, id string, size int64) bool {
	zr, err := gzip.NewReader(bytes.NewReader(encoded))
	if err != nil {
		return false
	}
	raw, err := io.ReadAll(zr)
	return err == nil && int64(len(raw)) == size && hexSHA(raw) == id
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}
