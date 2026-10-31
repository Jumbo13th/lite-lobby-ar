package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/config"
	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/send"
	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/state"
)

type testClock struct {
	mu sync.Mutex
	t  time.Time
}

func (c *testClock) Now() time.Time {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.t
}

func (c *testClock) Sleep(d time.Duration) {
	c.mu.Lock()
	c.t = c.t.Add(d)
	c.mu.Unlock()
}

type sink struct {
	mu       sync.Mutex
	commits  []string
	sessions []string
	games    map[string]send.GamePost
}

func (k *sink) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	k.mu.Lock()
	defer k.mu.Unlock()
	body, _ := io.ReadAll(r.Body)
	switch {
	case r.Method == http.MethodGet:
		w.WriteHeader(http.StatusNotFound)
	case r.Method == http.MethodPost && strings.HasPrefix(r.URL.Path, "/files/"):
		var c send.FileCommit
		json.Unmarshal(body, &c)
		k.commits = append(k.commits, strings.TrimPrefix(r.URL.Path, "/files/"))
		k.sessions = append(k.sessions, c.Session)
		w.WriteHeader(http.StatusCreated)
	case r.Method == http.MethodPost && strings.HasPrefix(r.URL.Path, "/games/"):
		var g send.GamePost
		json.Unmarshal(body, &g)
		k.games[g.Session] = g
		w.WriteHeader(http.StatusCreated)
	default:
		w.WriteHeader(http.StatusCreated)
	}
}

func (k *sink) game(session string) (send.GamePost, bool) {
	k.mu.Lock()
	defer k.mu.Unlock()
	g, ok := k.games[session]
	return g, ok
}

func (k *sink) commitCount() int {
	k.mu.Lock()
	defer k.mu.Unlock()
	return len(k.commits)
}

func put(t *testing.T, profile, rel, content string, mod time.Time) string {
	t.Helper()
	p := filepath.Join(profile, filepath.FromSlash(rel))
	if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Chtimes(p, mod, mod); err != nil {
		t.Fatal(err)
	}
	return p
}

func exists(p string) bool {
	_, err := os.Stat(p)
	return err == nil
}

func sha(content string) string {
	sum := sha256.Sum256([]byte(content))
	return hex.EncodeToString(sum[:])
}

func TestPassSendsOnlyGamesThatAreOver(t *testing.T) {
	profile := t.TempDir()
	clock := &testClock{t: time.Unix(1793563200, 0)}
	now := clock.Now()
	const finished, running, next = "20261031-180000-eden", "20261031-200000-eden", "20261031-230000-eden"
	finishedFiles := []string{
		put(t, profile, "LL_Replays/"+finished+"-1.llr", "LLR 1\nH session "+finished+"\nP 1 2 3 4 5\nSTOP shutdown\n", now.Add(-time.Hour)),
		put(t, profile, "LL_Replays/"+finished+"-identities.txt", "LLI 1\nH session "+finished+"\nI 1 x\n", now.Add(-time.Hour)),
		put(t, profile, "LL_GameStats/"+finished+"-live.json", `{"phase":"live"}`, now.Add(-2*time.Hour)),
		put(t, profile, "LL_GameStats/"+finished+"-final.json", `{"phase":"final"}`, now.Add(-time.Hour)),
	}
	runningSegment := put(t, profile, "LL_Replays/"+running+"-1.llr", "LLR 1\nH session "+running+"\nP 1 2 3 4 5\n", now)
	put(t, profile, "LL_Replays/status.txt", fmt.Sprintf("LLS 1 rec %s 1 %d\n", running, now.Unix()), now)

	rx := &sink{games: map[string]send.GamePost{}}
	srv := httptest.NewTLSServer(rx)
	defer srv.Close()
	st, err := state.Load(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	client := send.NewClient(srv.URL, "server-1", strings.Repeat("s", 32), send.NewLimiter(1_000_000, clock), srv.Client().Transport, clock)
	metricsFile := filepath.Join(t.TempDir(), "ll_uplink_server-1.prom")
	log := slog.New(slog.DiscardHandler)
	s := &service{
		cfg:      config.Config{ServerID: "server-1", Profile: profile, StoppedWaitMinutes: 30, RetentionDays: 14, MetricsFile: metricsFile},
		state:    st,
		up:       send.NewUploader(client, st, clock, log),
		log:      log,
		now:      clock.Now,
		reported: map[string]string{},
	}

	s.pass(context.Background())
	g, posted := rx.game(finished)
	if !posted || g.Over != "stopped" || len(g.Segments) != 1 || g.Identity == nil || *g.Stats != sha(`{"phase":"final"}`) {
		t.Fatalf("game followed by the next session posted as %+v (posted %v)", g, posted)
	}
	rx.mu.Lock()
	early := slices.Contains(rx.sessions, running)
	rx.mu.Unlock()
	if _, ok := rx.game(running); ok || early {
		t.Fatal("something of the running game was sent")
	}
	m, err := os.ReadFile(metricsFile)
	if err != nil || !strings.Contains(string(m), `ll_uplink_confirmed_games_total{server="server-1"} 1`) || !strings.Contains(string(m), `ll_uplink_pending_files{server="server-1"} 0`) {
		t.Fatalf("metrics: %s, %v", m, err)
	}

	clock.Sleep(time.Minute)
	put(t, profile, "LL_Replays/status.txt", fmt.Sprintf("LLS 1 resume - - %d\n", clock.Now().Unix()), clock.Now())
	approved := `{"phase":"approved"}`
	put(t, profile, "LL_GameStats/"+finished+"-approved.json", approved, clock.Now())
	commits := rx.commitCount()
	s.pass(context.Background())
	if rx.commitCount() != commits {
		t.Fatal("sent while a resume is pending")
	}

	clock.Sleep(time.Minute)
	put(t, profile, "LL_Replays/status.txt", fmt.Sprintf("LLS 1 idle %s - %d\n", next, clock.Now().Unix()), clock.Now())
	s.pass(context.Background())
	if g, _ := rx.game(finished); *g.Stats != sha(approved) || g.Over != "stopped" {
		t.Fatalf("newer statistics not posted: %+v", g)
	}
	if g, ok := rx.game(running); !ok || g.Over != "stopped" {
		t.Fatalf("game cut off by another game's start posted as %+v", g)
	}
	finishedFiles = append(finishedFiles, filepath.Join(profile, "LL_GameStats", finished+"-approved.json"))
	for _, p := range append(finishedFiles, runningSegment) {
		if !exists(p) {
			t.Fatalf("%s deleted before the retention time", p)
		}
	}

	clock.Sleep(15 * 24 * time.Hour)
	s.pass(context.Background())
	for _, p := range append(finishedFiles, runningSegment) {
		if exists(p) {
			t.Errorf("%s kept past the retention time", p)
		}
	}
	if !exists(filepath.Join(profile, "LL_Replays", "status.txt")) {
		t.Error("status file deleted")
	}
	if len(st.Games) != 0 || len(st.Files) != 0 {
		t.Errorf("deleted games not forgotten: %+v", st)
	}
}
