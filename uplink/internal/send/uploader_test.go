package send

import (
	"bytes"
	"context"
	"errors"
	"log/slog"
	"maps"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/state"
)

const session = "20261031-201500-eden"

type syncBuffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (b *syncBuffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Write(p)
}

func (b *syncBuffer) String() string {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.String()
}

type fixture struct {
	t        *testing.T
	clock    *fakeClock
	rc       *receiver
	st       *state.State
	stateDir string
	up       *Uploader
	profile  string
	logs     *syncBuffer
	game     Game
}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	f := &fixture{t: t, clock: newFakeClock(), profile: t.TempDir(), stateDir: t.TempDir(), logs: &syncBuffer{}}
	f.rc = newReceiver(f.clock)
	st, err := state.Load(f.stateDir)
	if err != nil {
		t.Fatal(err)
	}
	f.st = st
	f.up = f.uploader(testSecret)
	f.game = Game{
		Session: session,
		Over:    "ended",
		Segments: []string{
			f.write("LL_Replays/"+session+"-1.llr", randomBytes(3_200_000, 11)),
			f.write("LL_Replays/"+session+"-2.llr", []byte("LLR 1\nH session "+session+"\nH segment 2\nEND debrief\n")),
		},
		Identity: f.write("LL_Replays/"+session+"-identities.txt", []byte("LLI 1\nH session "+session+"\nI 1 6f1a2b3c-4d5e-6f70-8192-a3b4c5d6e7f8\n")),
		Stats:    f.write("LL_GameStats/"+session+"-final.json", []byte(`{"session":"`+session+`","phase":"final"}`)),
	}
	return f
}

func (f *fixture) uploader(secret string) *Uploader {
	client := newTestClient(f.t, f.rc, f.clock, secret)
	return NewUploader(client, f.st, f.clock, slog.New(slog.NewTextHandler(f.logs, nil)))
}

func (f *fixture) write(rel string, content []byte) string {
	f.t.Helper()
	p := filepath.Join(f.profile, filepath.FromSlash(rel))
	if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
		f.t.Fatal(err)
	}
	if err := os.WriteFile(p, content, 0o644); err != nil {
		f.t.Fatal(err)
	}
	return p
}

func (f *fixture) id(path string) string {
	f.t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		f.t.Fatal(err)
	}
	return hexSHA(data)
}

func (f *fixture) ids() []string {
	ids := []string{f.id(f.game.Segments[0]), f.id(f.game.Segments[1]), f.id(f.game.Identity), f.id(f.game.Stats)}
	return ids
}

func (f *fixture) requests() int {
	var n int
	f.rc.with(func() { n = f.rc.requests })
	return n
}

func (f *fixture) committedOnce(ids ...string) {
	f.t.Helper()
	f.rc.with(func() {
		for _, id := range ids {
			if f.rc.commits[id] != 1 {
				f.t.Errorf("file %.12s stored %d times", id, f.rc.commits[id])
			}
		}
	})
}

func kindOf(err error) Kind {
	var e *Error
	if errors.As(err, &e) {
		return e.Kind
	}
	return ""
}

func TestUploadGameSendsEveryFileThenPosts(t *testing.T) {
	f := newFixture(t)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	ids := f.ids()
	f.committedOnce(ids...)
	f.rc.with(func() {
		g := f.rc.games[session]
		if f.rc.posts != 1 || g.Over != "ended" || !slices.Equal(g.Segments, ids[:2]) || *g.Identity != ids[2] || *g.Stats != ids[3] {
			t.Fatalf("receiver got %d posts, last %+v", f.rc.posts, g)
		}
	})
	rec := f.st.Games[session]
	if rec.Confirmed.IsZero() || rec.Over != "ended" || !slices.Equal(rec.Segments, ids[:2]) || rec.Identity != ids[2] || rec.Stats != ids[3] {
		t.Fatalf("state = %+v", rec)
	}
	kinds := map[string]string{ids[0]: "segment", ids[1]: "segment", ids[2]: "identity", ids[3]: "stats"}
	for id, kind := range kinds {
		if got := f.st.Files[id]; got.Kind != kind || got.Session != session || got.Committed.IsZero() {
			t.Errorf("file record %.12s = %+v", id, got)
		}
	}
	if f.st.Files[ids[0]].Name != session+"-1.llr" {
		t.Errorf("name = %q", f.st.Files[ids[0]].Name)
	}
	if f.up.ConfirmedGames() != 1 || f.up.LastSuccess().IsZero() {
		t.Error("counters not updated")
	}
	back, err := state.Load(f.stateDir)
	if err != nil || !back.Games[session].Confirmed.Equal(rec.Confirmed) || len(back.Files) != 4 {
		t.Fatalf("confirmation not saved: %v", err)
	}
}

func TestUploadGameSurvivesAnOutageWithoutDuplicates(t *testing.T) {
	f := newFixture(t)
	f.rc.with(func() { f.rc.outageIn = 5 })
	err := f.up.UploadGame(context.Background(), f.game)
	if kindOf(err) != KindTransient {
		t.Fatalf("during the outage: %v", err)
	}
	ids := f.ids()
	if _, ok := f.st.Files[ids[0]]; !ok || len(f.st.Files) != 1 {
		t.Fatalf("committed before the outage: %v", slices.Collect(maps.Keys(f.st.Files)))
	}
	if rec := f.st.Games[session]; rec.Failures != 1 || rec.LastError == "" || !rec.Confirmed.IsZero() {
		t.Fatalf("failure not recorded: %+v", rec)
	}

	before := f.requests()
	f.clock.Advance(5 * time.Second)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil || f.requests() != before {
		t.Fatalf("retried before the wait: %v, %d requests", err, f.requests()-before)
	}

	f.rc.with(func() { f.rc.down = false })
	f.clock.Advance(maxWait)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	f.committedOnce(ids...)
	f.rc.with(func() {
		if f.rc.posts != 1 {
			t.Errorf("%d posts", f.rc.posts)
		}
	})
	if rec := f.st.Games[session]; rec.Failures != 0 || rec.LastError != "" || rec.Confirmed.IsZero() {
		t.Fatalf("after recovery: %+v", rec)
	}
	if f.up.Failures()["transient"] != 1 {
		t.Errorf("failures = %v", f.up.Failures())
	}
}

func TestUploadGameHonoursRetryAfter(t *testing.T) {
	f := newFixture(t)
	f.rc.with(func() { f.rc.limited, f.rc.retryAfter = 1, "120" })
	err := f.up.UploadGame(context.Background(), f.game)
	var e *Error
	if !errors.As(err, &e) || e.Kind != KindRateLimited || e.RetryAfter != 120*time.Second {
		t.Fatalf("got %v", err)
	}
	before := f.requests()
	f.clock.Advance(119 * time.Second)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil || f.requests() != before {
		t.Fatalf("sent before Retry-After: %v, %d requests", err, f.requests()-before)
	}
	f.clock.Advance(2 * time.Second)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	f.committedOnce(f.ids()...)
}

func TestUploadGameResendsAfterAMismatch(t *testing.T) {
	f := newFixture(t)
	f.rc.with(func() { f.rc.corrupt = 1 })
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	ids := f.ids()
	f.committedOnce(ids...)
	f.rc.with(func() {
		if f.rc.puts[ids[0]] != 6 || f.rc.posts != 1 {
			t.Errorf("first file: %d pieces sent, %d posts; want 6 and 1", f.rc.puts[ids[0]], f.rc.posts)
		}
	})
	if !strings.Contains(f.logs.String(), "sending it again") {
		t.Error("the resend was not logged")
	}
}

func TestUploadGameHoldsAFileRefusedTwiceUntilItChanges(t *testing.T) {
	f := newFixture(t)
	f.rc.with(func() { f.rc.corrupt = 2 })
	if err := f.up.UploadGame(context.Background(), f.game); kindOf(err) != KindInvalid {
		t.Fatalf("got %v", err)
	}
	f.rc.with(func() {
		if f.rc.posts != 0 {
			t.Fatal("game posted with a refused file")
		}
	})

	f.clock.Advance(maxWait)
	before := f.requests()
	if err := f.up.UploadGame(context.Background(), f.game); kindOf(err) != KindInvalid || f.requests() != before {
		t.Fatalf("refused file sent again unchanged: %v, %d requests", err, f.requests()-before)
	}

	f.write("LL_Replays/"+session+"-1.llr", randomBytes(3_200_000, 12))
	f.clock.Advance(maxWait)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	f.rc.with(func() {
		if g := f.rc.games[session]; g.Segments[0] != f.id(f.game.Segments[0]) {
			t.Fatal("game posted without the changed file")
		}
	})
}

func TestUploadGameResendsFilesTheReceiverLost(t *testing.T) {
	f := newFixture(t)
	ids := f.ids()
	for i, id := range ids {
		f.st.Files[id] = state.FileRec{Kind: []string{"segment", "segment", "identity", "stats"}[i], Session: session, Committed: f.clock.Now()}
	}
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	f.committedOnce(ids...)
	f.rc.with(func() {
		if f.rc.posts != 1 {
			t.Errorf("%d posts", f.rc.posts)
		}
	})
}

func (f *fixture) appendTo(path, more string) {
	f.t.Helper()
	file, err := os.OpenFile(path, os.O_APPEND|os.O_WRONLY, 0)
	if err != nil {
		f.t.Fatal(err)
	}
	if _, err := file.WriteString(more); err != nil {
		f.t.Fatal(err)
	}
	if err := file.Close(); err != nil {
		f.t.Fatal(err)
	}
}

func TestStatisticsAloneArePostedAgain(t *testing.T) {
	f := newFixture(t)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	ids := f.ids()
	first := f.st.Games[session].Confirmed

	before := f.requests()
	if err := f.up.UploadGame(context.Background(), f.game); err != nil || f.requests() != before {
		t.Fatalf("unchanged game sent again: %v, %d requests", err, f.requests()-before)
	}

	f.clock.Advance(time.Hour)
	approved := f.write("LL_GameStats/"+session+"-approved.json", []byte(`{"session":"`+session+`","phase":"approved"}`))
	g := f.game
	g.Stats = approved
	if err := f.up.UploadGame(context.Background(), g); err != nil {
		t.Fatal(err)
	}
	approvedID := f.id(approved)
	f.committedOnce(append(ids, approvedID)...)
	f.rc.with(func() {
		got := f.rc.games[session]
		if f.rc.posts != 2 || *got.Stats != approvedID || !slices.Equal(got.Segments, ids[:2]) || *got.Identity != ids[2] || got.Over != "ended" {
			t.Fatalf("%d posts, last %+v", f.rc.posts, got)
		}
	})
	if rec := f.st.Games[session]; rec.Stats != approvedID || !rec.Confirmed.After(first) {
		t.Fatalf("new set or confirmation time not recorded: %+v", rec)
	}
	if f.up.ConfirmedGames() != 1 {
		t.Errorf("a repeated post counted as another game: %d", f.up.ConfirmedGames())
	}

	f.write("LL_GameStats/"+session+"-approved.json", []byte(`{"session":"`+session+`","phase":"approved"}`))
	before = f.requests()
	if err := f.up.UploadGame(context.Background(), g); err != nil || f.requests() != before {
		t.Fatalf("rewritten but unchanged statistics sent: %v, %d requests", err, f.requests()-before)
	}
}

func TestResumedGameIsPostedAgainWithAllItsFiles(t *testing.T) {
	f := newFixture(t)
	seg1, identity := f.game.Segments[0], f.game.Identity
	abandoned := Game{Session: session, Over: "abandoned", Segments: []string{seg1}, Identity: identity}
	if err := f.up.UploadGame(context.Background(), abandoned); err != nil {
		t.Fatal(err)
	}
	first := f.st.Games[session].Confirmed
	seg1ID, oldIdentity := f.id(seg1), f.id(identity)

	f.clock.Advance(time.Hour)
	seg2 := f.write("LL_Replays/"+session+"-2.llr", []byte("LLR 1\nH session "+session+"\nH segment 2\nH resumed 9000\nEND debrief\n"))
	f.appendTo(identity, "I 2 0b9c8d7e-6f5a-4b3c-2d1e-0f9a8b7c6d5e\n")
	ended := Game{Session: session, Over: "ended", Segments: []string{seg1, seg2}, Identity: identity}
	if err := f.up.UploadGame(context.Background(), ended); err != nil {
		t.Fatal(err)
	}

	seg2ID, newIdentity := f.id(seg2), f.id(identity)
	f.committedOnce(seg1ID, seg2ID, oldIdentity, newIdentity)
	f.rc.with(func() {
		got := f.rc.games[session]
		if f.rc.posts != 2 || got.Over != "ended" || !slices.Equal(got.Segments, []string{seg1ID, seg2ID}) || *got.Identity != newIdentity || got.Stats != nil {
			t.Fatalf("%d posts, last %+v", f.rc.posts, got)
		}
		if f.rc.puts[seg1ID] != 3 {
			t.Errorf("segment 1 pieces sent %d times, want once each (3)", f.rc.puts[seg1ID])
		}
	})
	rec := f.st.Games[session]
	if rec.Over != "ended" || !slices.Equal(rec.Segments, []string{seg1ID, seg2ID}) || rec.Identity != newIdentity || !rec.Confirmed.After(first) {
		t.Fatalf("state = %+v", rec)
	}

	before := f.requests()
	if err := f.up.UploadGame(context.Background(), ended); err != nil || f.requests() != before {
		t.Fatalf("unchanged game sent again: %v, %d requests", err, f.requests()-before)
	}
}

func TestRepeatedPostKeepsWhatWasConfirmed(t *testing.T) {
	f := newFixture(t)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	ids := f.ids()
	approved := f.write("LL_GameStats/"+session+"-approved.json", []byte(`{"phase":"approved"}`))

	without := Game{Session: session, Over: "ended", Segments: f.game.Segments[1:], Identity: f.game.Identity, Stats: approved}
	before := f.requests()
	if err := f.up.UploadGame(context.Background(), without); err != nil || f.requests() != before {
		t.Fatalf("posted without a confirmed segment: %v, %d requests", err, f.requests()-before)
	}

	noIdentity := Game{Session: session, Over: "ended", Segments: f.game.Segments, Stats: approved}
	if err := f.up.UploadGame(context.Background(), noIdentity); err != nil {
		t.Fatal(err)
	}
	f.rc.with(func() {
		if got := f.rc.games[session]; got.Identity == nil || *got.Identity != ids[2] || *got.Stats != f.id(approved) {
			t.Fatalf("repeated post dropped the confirmed identity file: %+v", got)
		}
	})
}

func TestUploadGameStopsBetweenRequests(t *testing.T) {
	f := newFixture(t)
	stop, cancel := context.WithCancel(context.Background())
	cancel()
	if err := f.up.UploadGame(stop, f.game); !errors.Is(err, context.Canceled) {
		t.Fatalf("got %v", err)
	}
	if f.requests() != 0 || f.st.Games[session].Failures != 0 {
		t.Fatal("a stop counted as a failure or sent a request")
	}
}

func TestPendingCountsFilesNotHeld(t *testing.T) {
	f := newFixture(t)
	paths := append(slices.Clone(f.game.Segments), f.game.Identity, f.game.Stats)
	files, size := f.up.Pending(paths)
	if files != 4 || size < 3_200_000 {
		t.Fatalf("before: %d files, %d bytes", files, size)
	}
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	if files, size = f.up.Pending(paths); files != 0 || size != 0 {
		t.Fatalf("after: %d files, %d bytes", files, size)
	}
}

func TestLogsCarryNoSecretOrSignature(t *testing.T) {
	f := newFixture(t)
	wrong := "a-wrong-secret-of-at-least-32-characters"
	if err := f.uploader(wrong).UploadGame(context.Background(), f.game); kindOf(err) != KindAuth {
		t.Fatalf("wrong secret: %v", err)
	}
	f.rc.with(func() { f.rc.limited, f.rc.retryAfter = 1, "1" })
	if err := f.up.UploadGame(context.Background(), f.game); kindOf(err) != KindRateLimited {
		t.Fatalf("rate limit: %v", err)
	}
	f.clock.Advance(maxWait)
	if err := f.up.UploadGame(context.Background(), f.game); err != nil {
		t.Fatal(err)
	}
	logs := f.logs.String()
	if !strings.Contains(logs, "check the host clock and the secret") {
		t.Error("the refused signature was not explained")
	}
	for _, secret := range []string{testSecret, wrong, "X-LL-"} {
		if strings.Contains(logs, secret) {
			t.Errorf("log carries %q", secret)
		}
	}
	f.rc.with(func() {
		for _, sig := range f.rc.signatures {
			if strings.Contains(logs, sig) {
				t.Fatalf("log carries the signature %s", sig)
			}
		}
	})
}

func TestBackoffGrowsFromTenSecondsToThirtyMinutes(t *testing.T) {
	cases := []struct {
		failures int
		jitter   float64
		want     time.Duration
	}{
		{1, 0, 10 * time.Second},
		{1, 1, 15 * time.Second},
		{2, 0, 20 * time.Second},
		{3, 0.5, 50 * time.Second},
		{8, 0, 1280 * time.Second},
		{8, 1, 30 * time.Minute},
		{9, 0, 30 * time.Minute},
		{1000, 1, 30 * time.Minute},
	}
	for _, c := range cases {
		if got := backoff(c.failures, c.jitter); got != c.want {
			t.Errorf("backoff(%d, %v) = %v, want %v", c.failures, c.jitter, got, c.want)
		}
	}
}
