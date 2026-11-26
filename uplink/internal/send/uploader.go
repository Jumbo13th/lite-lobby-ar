package send

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"maps"
	"math/rand/v2"
	"os"
	"path/filepath"
	"slices"
	"time"

	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/state"
)

const (
	fileSegment  = "segment"
	fileIdentity = "identity"
	fileStats    = "stats"

	minWait = 10 * time.Second
	maxWait = 30 * time.Minute
)

// Game is a game that is over, by file path: segments in order, "" for an absent file.
type Game struct {
	Session  string
	Over     string
	Segments []string
	Identity string
	Stats    string
}

// Uploader sends games that are over and keeps in the state the set the website last
// confirmed for each. Every failure is logged and retried with growing waits per game.
type Uploader struct {
	client *Client
	state  *state.State
	clock  Clock
	log    *slog.Logger

	retry       map[string]*retry
	refused     map[string]bool
	ids         map[string]cachedID
	failures    map[string]int
	lastSuccess time.Time
	confirmed   int
}

type retry struct {
	failures int
	next     time.Time
}

type cachedID struct {
	size int64
	mod  time.Time
	id   string
}

type fileRef struct {
	kind    string
	session string
	path    string
	id      string
	size    int64
}

func NewUploader(client *Client, st *state.State, clock Clock, log *slog.Logger) *Uploader {
	u := &Uploader{
		client:   client,
		state:    st,
		clock:    clock,
		log:      log,
		retry:    map[string]*retry{},
		refused:  map[string]bool{},
		ids:      map[string]cachedID{},
		failures: map[string]int{},
	}
	for _, k := range []Kind{KindAuth, KindInvalid, KindRateLimited, KindTransient, KindLocal} {
		u.failures[string(k)] = 0
	}
	for _, f := range st.Files {
		u.lastSuccess = later(u.lastSuccess, f.Committed)
	}
	for _, g := range st.Games {
		u.lastSuccess = later(u.lastSuccess, g.Confirmed)
	}
	return u
}

// UploadGame reposts a confirmed game only when its set changed, never without a confirmed
// segment (a repost replaces the website's record); stop never cuts a request in flight.
func (u *Uploader) UploadGame(stop context.Context, g Game) error {
	if !u.due(g.Session) {
		return nil
	}
	err := u.uploadGame(stop, g)
	u.settle(g.Session, err)
	return err
}

func (u *Uploader) Pending(paths []string) (files int, bytes int64) {
	for _, p := range paths {
		id, size, err := u.identity(p)
		if err != nil {
			continue
		}
		if _, ok := u.state.Files[id]; !ok {
			files++
			bytes += size
		}
	}
	return files, bytes
}

// Forget drops deleted files from the identity cache, which would otherwise grow with every game.
func (u *Uploader) Forget(paths ...string) {
	for _, p := range paths {
		delete(u.ids, p)
	}
}

func (u *Uploader) LastSuccess() time.Time   { return u.lastSuccess }
func (u *Uploader) Failures() map[string]int { return maps.Clone(u.failures) }
func (u *Uploader) ConfirmedGames() int      { return u.confirmed }

func (u *Uploader) uploadGame(stop context.Context, g Game) error {
	refs, err := u.refs(g)
	if err != nil {
		return err
	}
	post := GamePost{Session: g.Session, Over: g.Over, Segments: []string{}}
	for _, f := range refs {
		switch f.kind {
		case fileSegment:
			post.Segments = append(post.Segments, f.id)
		case fileIdentity:
			post.Identity = ptr(f.id)
		case fileStats:
			post.Stats = ptr(f.id)
		}
	}
	rec := u.state.Games[g.Session]
	again := !rec.Confirmed.IsZero()
	if again {
		if !containsAll(post.Segments, rec.Segments) {
			return nil
		}
		if post.Identity == nil && rec.Identity != "" {
			post.Identity = ptr(rec.Identity)
		}
		if post.Stats == nil && rec.Stats != "" {
			post.Stats = ptr(rec.Stats)
		}
		if post.Over == rec.Over && slices.Equal(post.Segments, rec.Segments) && deref(post.Identity) == rec.Identity && deref(post.Stats) == rec.Stats {
			return nil
		}
	}
	for _, f := range refs {
		if err := u.ensure(stop, f); err != nil {
			return err
		}
	}
	if err := u.post(stop, post, refs); err != nil {
		return err
	}
	rec.Over, rec.Segments, rec.Identity, rec.Stats = g.Over, post.Segments, deref(post.Identity), deref(post.Stats)
	rec.Confirmed = u.clock.Now()
	u.state.Games[g.Session] = rec
	u.lastSuccess = rec.Confirmed
	msg := "game posted again"
	if !again {
		msg = "game confirmed"
		u.confirmed++
	}
	u.log.Info(msg, "session", g.Session, "over", g.Over, "segments", len(post.Segments))
	return u.save()
}

func (u *Uploader) refs(g Game) ([]fileRef, error) {
	var refs []fileRef
	add := func(kind, path string) error {
		id, size, err := u.identity(path)
		if err != nil {
			return &Error{Kind: KindLocal, Err: err}
		}
		refs = append(refs, fileRef{kind: kind, session: g.Session, path: path, id: id, size: size})
		return nil
	}
	for _, p := range g.Segments {
		if err := add(fileSegment, p); err != nil {
			return nil, err
		}
	}
	if g.Identity != "" {
		if err := add(fileIdentity, g.Identity); err != nil {
			return nil, err
		}
	}
	if g.Stats != "" {
		if err := add(fileStats, g.Stats); err != nil {
			return nil, err
		}
	}
	return refs, nil
}

func (u *Uploader) ensure(stop context.Context, f fileRef) error {
	if _, ok := u.state.Files[f.id]; ok {
		return nil
	}
	return u.send(stop, f)
}

func (u *Uploader) send(stop context.Context, f fileRef) error {
	name := filepath.Base(f.path)
	if u.refused[f.id] {
		return &Error{Kind: KindInvalid, Err: fmt.Errorf("%s was refused twice and is held until it changes", name)}
	}
	raw, err := os.ReadFile(f.path)
	if err != nil {
		return &Error{Kind: KindLocal, Err: err}
	}
	enc, err := Encode(raw)
	if err != nil {
		return &Error{Kind: KindLocal, Err: err}
	}
	if enc.Identity != f.id {
		return &Error{Kind: KindTransient, Err: fmt.Errorf("%s changed while it was being sent", name)}
	}
	err = u.sendPieces(stop, f, enc, false)
	if isKind(err, KindInvalid) {
		u.log.Warn("file refused, sending it again", "session", f.session, "file", name, "error", err)
		err = u.sendPieces(stop, f, enc, true)
		if isKind(err, KindInvalid) {
			u.refused[f.id] = true
			u.log.Error("file refused twice; it stays on the host until it changes", "session", f.session, "file", name, "error", err)
		}
	}
	if err != nil {
		return err
	}
	now := u.clock.Now()
	u.state.Files[f.id] = state.FileRec{Kind: f.kind, Session: f.session, Name: name, Committed: now}
	u.lastSuccess = now
	u.log.Info("file sent", "session", f.session, "file", name, "bytes", enc.Size, "encoded", enc.EncodedSize)
	return u.save()
}

func (u *Uploader) sendPieces(stop context.Context, f fileRef, enc *Encoded, all bool) error {
	stored := map[int]bool{}
	if !all {
		if err := stop.Err(); err != nil {
			return err
		}
		held, err := u.client.GetFile(f.id)
		if err != nil {
			return err
		}
		if held.Complete {
			return nil
		}
		for _, i := range held.Pieces {
			stored[i] = true
		}
	}
	for i, piece := range enc.Pieces {
		if stored[i] {
			continue
		}
		if err := u.put(stop, f.id, i, piece); err != nil {
			return err
		}
	}
	commit := FileCommit{
		Kind:          f.kind,
		Session:       f.session,
		Name:          filepath.Base(f.path),
		Size:          enc.Size,
		Encoding:      "gzip",
		EncodedSize:   enc.EncodedSize,
		EncodedSHA256: enc.EncodedSHA256,
		Pieces:        len(enc.Pieces),
	}
	for range 3 {
		if err := stop.Err(); err != nil {
			return err
		}
		err := u.client.Commit(f.id, commit)
		var e *Error
		if !errors.As(err, &e) || e.Kind != KindMissing {
			return err
		}
		for _, i := range e.Pieces {
			if i < 0 || i >= len(enc.Pieces) {
				return &Error{Kind: KindInvalid, Status: e.Status, Err: fmt.Errorf("receiver asked for piece %d of %d", i, len(enc.Pieces))}
			}
			if err := u.put(stop, f.id, i, enc.Pieces[i]); err != nil {
				return err
			}
		}
	}
	return &Error{Kind: KindTransient, Err: errors.New("receiver still reports missing pieces")}
}

func (u *Uploader) put(stop context.Context, id string, index int, piece []byte) error {
	if err := stop.Err(); err != nil {
		return err
	}
	return u.client.PutPiece(id, index, piece)
}

func (u *Uploader) post(stop context.Context, g GamePost, refs []fileRef) error {
	for range 3 {
		if err := stop.Err(); err != nil {
			return err
		}
		err := u.client.PostGame(g)
		var e *Error
		if !errors.As(err, &e) || e.Kind != KindMissing {
			return err
		}
		for _, id := range e.Files {
			f, ok := findRef(refs, id)
			if !ok {
				return &Error{Kind: KindTransient, Status: e.Status, Err: fmt.Errorf("receiver asked for %.64q, which is not part of the game", id)}
			}
			if err := u.send(stop, f); err != nil {
				return err
			}
		}
	}
	return &Error{Kind: KindTransient, Err: errors.New("receiver still reports missing files")}
}

func (u *Uploader) due(session string) bool {
	r := u.retry[session]
	return r == nil || !u.clock.Now().Before(r.next)
}

func (u *Uploader) settle(session string, err error) {
	rec := u.state.Games[session]
	if err == nil {
		delete(u.retry, session)
		if rec.Failures != 0 || rec.LastError != "" {
			rec.Failures, rec.LastError = 0, ""
			u.state.Games[session] = rec
			u.saveOrLog()
		}
		return
	}
	if errors.Is(err, context.Canceled) {
		return
	}
	r := u.retry[session]
	if r == nil {
		r = &retry{failures: rec.Failures}
		u.retry[session] = r
	}
	r.failures++
	wait := backoff(r.failures, rand.Float64())
	kind := KindTransient
	var e *Error
	if errors.As(err, &e) {
		kind = e.Kind
		wait = max(wait, e.RetryAfter)
	}
	r.next = u.clock.Now().Add(wait)
	rec.Failures, rec.LastError = r.failures, err.Error()
	u.state.Games[session] = rec
	u.failures[string(kind)]++
	msg := "upload failed"
	if kind == KindAuth {
		msg = "upload refused: check the host clock and the secret"
	}
	u.log.Warn(msg, "session", session, "kind", string(kind), "retry_in", wait.Round(time.Second).String(), "error", err)
	u.saveOrLog()
}

func (u *Uploader) save() error {
	if err := u.state.Save(); err != nil {
		return &Error{Kind: KindLocal, Err: err}
	}
	return nil
}

func (u *Uploader) saveOrLog() {
	if err := u.state.Save(); err != nil {
		u.log.Error("cannot save the state", "error", err)
	}
}

func (u *Uploader) identity(path string) (string, int64, error) {
	info, err := os.Stat(path)
	if err != nil {
		return "", 0, err
	}
	if c, ok := u.ids[path]; ok && c.size == info.Size() && c.mod.Equal(info.ModTime()) {
		return c.id, c.size, nil
	}
	f, err := os.Open(path)
	if err != nil {
		return "", 0, err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", 0, err
	}
	id := hex.EncodeToString(h.Sum(nil))
	u.ids[path] = cachedID{size: info.Size(), mod: info.ModTime(), id: id}
	return id, info.Size(), nil
}

func backoff(failures int, jitter float64) time.Duration {
	d := minWait
	for i := 1; i < failures && d < maxWait; i++ {
		d *= 2
	}
	return min(d+time.Duration(jitter*float64(d/2)), maxWait)
}

func isKind(err error, k Kind) bool {
	var e *Error
	return errors.As(err, &e) && e.Kind == k
}

func findRef(refs []fileRef, id string) (fileRef, bool) {
	for _, f := range refs {
		if f.id == id {
			return f, true
		}
	}
	return fileRef{}, false
}

func containsAll(have, want []string) bool {
	for _, id := range want {
		if !slices.Contains(have, id) {
			return false
		}
	}
	return true
}

func ptr(s string) *string { return &s }

func deref(s *string) string {
	if s == nil {
		return ""
	}
	return *s
}

func later(a, b time.Time) time.Time {
	if b.After(a) {
		return b
	}
	return a
}
