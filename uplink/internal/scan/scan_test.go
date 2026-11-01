package scan

import (
	"maps"
	"os"
	"path/filepath"
	"slices"
	"testing"
	"time"
)

func put(t *testing.T, profile, rel, content string, mod time.Time) string {
	t.Helper()
	p := filepath.Join(profile, filepath.FromSlash(rel))
	if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
	if !mod.IsZero() {
		if err := os.Chtimes(p, mod, mod); err != nil {
			t.Fatal(err)
		}
	}
	return p
}

func TestReadStatus(t *testing.T) {
	const s = "20261031-201500-eden"
	cases := []struct {
		name, content string
		want          Status
		ok            bool
	}{
		{"rec", "LLS 1 rec " + s + " 1 1793566800\n", Status{StateRec, s, 1, 1793566800}, true},
		{"crlf", "LLS 1 rec " + s + " 2 1793566800\r\n", Status{StateRec, s, 2, 1793566800}, true},
		{"no final newline", "LLS 1 idle " + s + " - 1793566800", Status{StateIdle, s, 0, 1793566800}, true},
		{"resume without session", "LLS 1 resume - - 1793566800\n", Status{StateResume, "", 0, 1793566800}, true},
		{"empty", "", Status{}, false},
		{"cut in the session", "LLS 1 rec 20261031-20", Status{}, false},
		{"cut in the time", "LLS 1 rec " + s + " 1 179356680", Status{}, false},
		{"newer version", "LLS 2 rec " + s + " 1 1793566800\n", Status{}, false},
		{"unknown state", "LLS 1 paused " + s + " 1 1793566800\n", Status{}, false},
		{"rec without segment", "LLS 1 rec " + s + " - 1793566800\n", Status{}, false},
		{"rec without session", "LLS 1 rec - 1 1793566800\n", Status{}, false},
		{"done is not a state", "LLS 1 done " + s + " 1 1793566800\n", Status{}, false},
		{"segment zero", "LLS 1 rec " + s + " 0 1793566800\n", Status{}, false},
		{"time not a number", "LLS 1 rec " + s + " 1 soon\n", Status{}, false},
		{"double space", "LLS 1 rec  " + s + " 1 1793566800\n", Status{}, false},
		{"two lines", "LLS 1 rec " + s + " 1 1793566800\nLLS 1 rec " + s + " 1 1793566809\n", Status{}, false},
		{"quote in session", "LLS 1 idle a\"b - 1793566800\n", Status{}, false},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			profile := t.TempDir()
			put(t, profile, "LL_Replays/status.txt", c.content, time.Time{})
			got, err := ReadStatus(profile)
			if c.ok != (err == nil) {
				t.Fatalf("err = %v, want ok = %v", err, c.ok)
			}
			if got != c.want {
				t.Fatalf("got %+v, want %+v", got, c.want)
			}
		})
	}
}

func TestReadStatusMissingFile(t *testing.T) {
	if _, err := ReadStatus(t.TempDir()); err == nil {
		t.Fatal("missing status file read without error")
	}
}

func TestListRecordings(t *testing.T) {
	profile := t.TempDir()
	t0 := time.Unix(1793563200, 0)
	const a, b = "20261030-180000-eden", "20261031-090000-eden-2"
	put(t, profile, "LL_Replays/"+a+"-1.llr", "LLR 1\nT 10\nP 1 2 3 4 5\n", t0)
	put(t, profile, "LL_Replays/"+a+"-2.llr", "LLR 1\nSTOP shutdown\n", t0.Add(time.Hour))
	put(t, profile, "LL_Replays/"+a+"-10.llr", "LLR 1\nP 1 2 3 4 5\nSTOP shutdown\n", t0.Add(2*time.Hour))
	ident := put(t, profile, "LL_Replays/"+a+"-identities.txt", "LLI 1\nH session "+a+"\n", t0.Add(2*time.Hour))
	put(t, profile, "LL_GameStats/"+a+"-final.json", "{}", t0.Add(3*time.Hour))
	put(t, profile, "LL_Replays/"+b+"-1.llr", "LLR 1\nP 1 2 3 4 5\n", t0.Add(4*time.Hour))
	put(t, profile, "LL_Replays/status.txt", "LLS 1 rec "+b+" 1 1793577600\n", time.Time{})
	put(t, profile, "LL_Replays/notes.txt", "", time.Time{})
	put(t, profile, "LL_Replays/20261031-120000-eden-identities.txt", "LLI 1\n", time.Time{})
	put(t, profile, "LL_Replays/20261031-130000-eden-01.llr", "", time.Time{})
	if err := os.MkdirAll(filepath.Join(profile, "LL_Replays", "folder-1.llr"), 0o755); err != nil {
		t.Fatal(err)
	}

	recs, err := ListRecordings(profile)
	if err != nil {
		t.Fatal(err)
	}
	if len(recs) != 2 || recs[0].Session != a || recs[1].Session != b {
		t.Fatalf("sessions = %+v, want %s and %s", recs, a, b)
	}
	var numbers []int
	for _, s := range recs[0].Segments {
		numbers = append(numbers, s.Number)
	}
	if !slices.Equal(numbers, []int{1, 2, 10}) {
		t.Errorf("segments = %v, want 1 2 10", numbers)
	}
	if recs[0].Identity != ident || !recs[0].LastWrite.Equal(t0.Add(3*time.Hour)) {
		t.Errorf("first recording = %+v", recs[0])
	}
	if recs[1].Identity != "" || !recs[1].LastWrite.Equal(t0.Add(4*time.Hour)) {
		t.Errorf("second recording = %+v", recs[1])
	}
}

func TestListRecordingsWithoutFolder(t *testing.T) {
	recs, err := ListRecordings(t.TempDir())
	if err != nil || len(recs) != 0 {
		t.Fatalf("recs = %v, err = %v", recs, err)
	}
}

func TestLatestStats(t *testing.T) {
	profile := t.TempDir()
	const s = "20261031-201500-eden"
	steps := []struct{ phase, want string }{
		{"", ""},
		{"live", "live"},
		{"final", "final"},
		{"approved", "approved"},
	}
	for _, step := range steps {
		if step.phase != "" {
			put(t, profile, "LL_GameStats/"+s+"-"+step.phase+".json", "{}", time.Time{})
		}
		got, err := LatestStats(profile, s)
		if err != nil {
			t.Fatal(err)
		}
		want := ""
		if step.want != "" {
			want = filepath.Join(profile, "LL_GameStats", s+"-"+step.want+".json")
		}
		if got != want {
			t.Errorf("after %q: got %q, want %q", step.phase, got, want)
		}
	}
	if got, _ := LatestStats(profile, "20261031-230000-eden"); got != "" {
		t.Errorf("another session's statistics returned: %q", got)
	}
}

func TestGamesOver(t *testing.T) {
	now := time.Unix(1793563200, 0)
	wait := 30 * time.Minute
	ago := func(d time.Duration) time.Time { return now.Add(-d) }
	status := func(state, session string, written time.Time) Status {
		st := Status{State: state, Session: session, Unix: written.Unix()}
		if state == StateRec {
			st.Segment = 1
		}
		return st
	}
	rec := func(session string, lastWrite time.Time) Recording {
		return Recording{Session: session, LastWrite: lastWrite}
	}
	const a, b, c = "20261031-180000-eden", "20261031-200000-eden", "20261031-220000-eden"
	cases := []struct {
		name string
		st   Status
		recs []Recording
		want map[string]string
	}{
		{"unreadable status holds every game", Status{},
			[]Recording{rec(a, ago(2*time.Hour)), rec(b, ago(10*time.Hour))}, map[string]string{}},
		{"resume holds every game", status(StateResume, "", ago(time.Minute)),
			[]Recording{rec(a, ago(2*time.Hour)), rec(b, ago(10*time.Hour))}, map[string]string{}},
		{"resume holds even when everything is stale", status(StateResume, b, ago(10*time.Hour)),
			[]Recording{rec(a, ago(10*time.Hour))}, map[string]string{}},
		{"rec with the same session is running", status(StateRec, a, ago(5*time.Second)),
			[]Recording{rec(a, ago(5*time.Second))}, map[string]string{}},
		{"fresh rec status holds stale files", status(StateRec, a, ago(time.Minute)),
			[]Recording{rec(a, ago(time.Hour))}, map[string]string{}},
		{"another session written after the last write means stopped", status(StateIdle, b, ago(time.Minute)),
			[]Recording{rec(a, ago(10*time.Minute))}, map[string]string{a: OverStopped}},
		{"a game cut off while the next one records is stopped", status(StateRec, b, now),
			[]Recording{rec(a, ago(time.Hour)), rec(b, now)}, map[string]string{a: OverStopped}},
		{"another session written before the last write proves nothing", status(StateIdle, b, ago(10*time.Minute)),
			[]Recording{rec(a, ago(5*time.Minute))}, map[string]string{}},
		{"status and files older than the wait mean abandoned", status(StateRec, a, ago(31*time.Minute)),
			[]Recording{rec(a, ago(31*time.Minute))}, map[string]string{a: OverAbandoned}},
		{"another session's start counts before staleness", status(StateIdle, b, ago(2*time.Hour)),
			[]Recording{rec(a, ago(3*time.Hour))}, map[string]string{a: OverStopped}},
		{"fresh files keep a stale status from abandoning", status(StateRec, a, ago(31*time.Minute)),
			[]Recording{rec(a, ago(time.Minute))}, map[string]string{}},
		{"within the wait nothing is abandoned", status(StateIdle, a, ago(29*time.Minute)),
			[]Recording{rec(a, ago(29*time.Minute))}, map[string]string{}},
		{"each game on its own", status(StateIdle, c, ago(40*time.Minute)),
			[]Recording{rec(a, ago(3*time.Hour)), rec(b, ago(10*time.Minute)), rec(c, ago(40*time.Minute))},
			map[string]string{a: OverStopped, c: OverAbandoned}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got := GamesOver(now, tc.st, tc.recs, wait)
			if !maps.Equal(got, tc.want) {
				t.Fatalf("got %v, want %v", got, tc.want)
			}
		})
	}
}
