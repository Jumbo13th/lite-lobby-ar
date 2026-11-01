package send

import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"
)

// PieceSize keeps every request body under the 2 MB limit receivers commonly run.
const PieceSize = 1_500_000

const (
	requestTimeout = 60 * time.Second
	maxAnswer      = 64 << 10
	maxRetryAfter  = 24 * time.Hour
)

// Kind classifies a failed request by what the uploader does about it.
type Kind string

const (
	KindAuth    Kind = "auth"
	KindMissing Kind = "missing"
	// KindInvalid is a 400, 413 or 422, or a piece held with other bytes: the file is sent
	// once more from the start, then held until its content changes.
	KindInvalid     Kind = "invalid"
	KindRateLimited Kind = "rate_limited"
	KindTransient   Kind = "transient"
	// KindLocal is a host file that cannot be read or a state that cannot be saved.
	KindLocal Kind = "local"
)

// Error is a request that did not succeed. Pieces and Files are what a 409 asked for.
type Error struct {
	Kind       Kind
	Status     int
	RetryAfter time.Duration
	Pieces     []int
	Files      []string
	Err        error
}

func (e *Error) Error() string {
	switch {
	case e.Err != nil && e.Status != 0:
		return fmt.Sprintf("%s: HTTP %d: %v", e.Kind, e.Status, e.Err)
	case e.Err != nil:
		return fmt.Sprintf("%s: %v", e.Kind, e.Err)
	default:
		return fmt.Sprintf("%s: HTTP %d", e.Kind, e.Status)
	}
}

func (e *Error) Unwrap() error { return e.Err }

type FileState struct {
	Complete bool
	Pieces   []int
}

type FileCommit struct {
	Kind          string `json:"kind"`
	Session       string `json:"session"`
	Name          string `json:"name"`
	Size          int64  `json:"size"`
	Encoding      string `json:"encoding"`
	EncodedSize   int64  `json:"encodedSize"`
	EncodedSHA256 string `json:"encodedSha256"`
	Pieces        int    `json:"pieces"`
}

// GamePost names files by identity; Identity and Stats are null when the game has none.
type GamePost struct {
	Session  string   `json:"session"`
	Over     string   `json:"over"`
	Segments []string `json:"segments"`
	Identity *string  `json:"identity"`
	Stats    *string  `json:"stats"`
}

// Encoded is a file ready to send: its identity is the SHA-256 of the uncompressed content.
type Encoded struct {
	Identity      string
	Size          int64
	EncodedSize   int64
	EncodedSHA256 string
	Pieces        [][]byte
}

// Encode leaves the time and the name out of the gzip header, so the same content always
// gives the same pieces and a resend matches the pieces the receiver already holds.
func Encode(raw []byte) (*Encoded, error) {
	var buf bytes.Buffer
	zw, err := gzip.NewWriterLevel(&buf, gzip.BestCompression)
	if err != nil {
		return nil, err
	}
	zw.ModTime = time.Time{}
	zw.Name = ""
	if _, err := zw.Write(raw); err != nil {
		return nil, err
	}
	if err := zw.Close(); err != nil {
		return nil, err
	}
	data := buf.Bytes()
	id := sha256.Sum256(raw)
	sum := sha256.Sum256(data)
	e := &Encoded{
		Identity:      hex.EncodeToString(id[:]),
		Size:          int64(len(raw)),
		EncodedSize:   int64(len(data)),
		EncodedSHA256: hex.EncodeToString(sum[:]),
	}
	for off := 0; off < len(data); off += PieceSize {
		e.Pieces = append(e.Pieces, data[off:min(off+PieceSize, len(data))])
	}
	return e, nil
}

// Client sends one signed request at a time; every body passes the limiter.
type Client struct {
	base    string
	server  string
	secret  []byte
	http    *http.Client
	limiter *Limiter
	clock   Clock
}

// NewClient never follows a redirect, so requests reach only the configured website.
func NewClient(base, serverID, secret string, limiter *Limiter, transport http.RoundTripper, clock Clock) *Client {
	return &Client{
		base:    strings.TrimRight(base, "/"),
		server:  serverID,
		secret:  []byte(secret),
		limiter: limiter,
		clock:   clock,
		http: &http.Client{
			Transport: transport,
			CheckRedirect: func(*http.Request, []*http.Request) error {
				return http.ErrUseLastResponse
			},
		},
	}
}

// Transport ignores proxy settings, so the service connects to nothing but the website.
func Transport() http.RoundTripper {
	t := http.DefaultTransport.(*http.Transport).Clone()
	t.Proxy = nil
	return t
}

func (c *Client) GetFile(id string) (FileState, error) {
	a, err := c.do(http.MethodGet, "/files/"+id, "", nil)
	if err != nil {
		return FileState{}, err
	}
	switch a.status {
	case http.StatusNotFound:
		return FileState{}, nil
	case http.StatusOK:
		var held struct {
			State  string `json:"state"`
			Pieces []int  `json:"pieces"`
		}
		if json.Unmarshal(a.body, &held) == nil {
			switch held.State {
			case "complete":
				return FileState{Complete: true}, nil
			case "partial":
				return FileState{Pieces: held.Pieces}, nil
			}
		}
		return FileState{}, &Error{Kind: KindTransient, Status: a.status, Err: errors.New("unreadable file state")}
	}
	return FileState{}, c.fail(a)
}

func (c *Client) PutPiece(id string, index int, piece []byte) error {
	a, err := c.do(http.MethodPut, "/files/"+id+"/pieces/"+strconv.Itoa(index), "application/octet-stream", piece)
	if err != nil {
		return err
	}
	switch a.status {
	case http.StatusOK, http.StatusCreated:
		return nil
	case http.StatusConflict:
		return &Error{Kind: KindInvalid, Status: a.status, Err: fmt.Errorf("piece %d held with other bytes", index)}
	}
	return c.fail(a)
}

func (c *Client) Commit(id string, fc FileCommit) error {
	body, err := json.Marshal(fc)
	if err != nil {
		return err
	}
	a, err := c.do(http.MethodPost, "/files/"+id, "application/json", body)
	if err != nil {
		return err
	}
	switch a.status {
	case http.StatusOK, http.StatusCreated:
		return nil
	case http.StatusConflict:
		var missing struct {
			Missing []int `json:"missing"`
		}
		if json.Unmarshal(a.body, &missing) != nil || len(missing.Missing) == 0 {
			return &Error{Kind: KindTransient, Status: a.status, Err: errors.New("409 without missing pieces")}
		}
		return &Error{Kind: KindMissing, Status: a.status, Pieces: missing.Missing}
	}
	return c.fail(a)
}

func (c *Client) PostGame(g GamePost) error {
	body, err := json.Marshal(g)
	if err != nil {
		return err
	}
	a, err := c.do(http.MethodPost, "/games/"+url.PathEscape(g.Session), "application/json", body)
	if err != nil {
		return err
	}
	switch a.status {
	case http.StatusOK, http.StatusCreated:
		return nil
	case http.StatusConflict:
		var missing struct {
			Missing []string `json:"missing"`
		}
		if json.Unmarshal(a.body, &missing) != nil || len(missing.Missing) == 0 {
			return &Error{Kind: KindTransient, Status: a.status, Err: errors.New("409 without missing files")}
		}
		return &Error{Kind: KindMissing, Status: a.status, Files: missing.Missing}
	}
	return c.fail(a)
}

type answer struct {
	status int
	header http.Header
	body   []byte
}

func (c *Client) do(method, path, contentType string, body []byte) (answer, error) {
	sum := sha256.Sum256(body)
	bodySHA := hex.EncodeToString(sum[:])
	ts := c.clock.Now().Unix()

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout+c.limiter.span(int64(len(body)), true))
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, method, c.base+path, nil)
	if err != nil {
		return answer{}, &Error{Kind: KindTransient, Err: err}
	}
	if len(body) > 0 {
		req.Body = io.NopCloser(c.limiter.Reader(bytes.NewReader(body)))
		req.GetBody = func() (io.ReadCloser, error) {
			return io.NopCloser(c.limiter.Reader(bytes.NewReader(body))), nil
		}
		req.ContentLength = int64(len(body))
		req.Header.Set("Content-Type", contentType)
	}
	req.Header.Set("X-LL-Server", c.server)
	req.Header.Set("X-LL-Timestamp", strconv.FormatInt(ts, 10))
	req.Header.Set("X-LL-Content-SHA256", bodySHA)
	req.Header.Set("X-LL-Signature", sign(c.secret, method, path, ts, bodySHA))

	resp, err := c.http.Do(req)
	if err != nil {
		return answer{}, &Error{Kind: KindTransient, Err: err}
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(io.LimitReader(resp.Body, maxAnswer))
	if err != nil {
		return answer{}, &Error{Kind: KindTransient, Status: resp.StatusCode, Err: err}
	}
	return answer{status: resp.StatusCode, header: resp.Header, body: data}, nil
}

func (c *Client) fail(a answer) error {
	switch a.status {
	case http.StatusUnauthorized:
		return &Error{Kind: KindAuth, Status: a.status}
	case http.StatusTooManyRequests:
		return &Error{Kind: KindRateLimited, Status: a.status, RetryAfter: retryAfter(a.header.Get("Retry-After"), c.clock.Now())}
	case http.StatusBadRequest, http.StatusRequestEntityTooLarge, http.StatusUnprocessableEntity:
		return &Error{Kind: KindInvalid, Status: a.status}
	}
	return &Error{Kind: KindTransient, Status: a.status}
}

func sign(secret []byte, method, path string, ts int64, bodySHA string) string {
	mac := hmac.New(sha256.New, secret)
	fmt.Fprintf(mac, "%s\n%s\n%d\n%s", method, path, ts, bodySHA)
	return hex.EncodeToString(mac.Sum(nil))
}

func retryAfter(v string, now time.Time) time.Duration {
	v = strings.TrimSpace(v)
	if s, err := strconv.ParseInt(v, 10, 64); err == nil && s >= 0 {
		return time.Duration(min(s, int64(maxRetryAfter/time.Second))) * time.Second
	}
	if t, err := http.ParseTime(v); err == nil && t.After(now) {
		return min(t.Sub(now), maxRetryAfter)
	}
	return 0
}
