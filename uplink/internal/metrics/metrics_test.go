package metrics

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestWriteInPrometheusTextFormat(t *testing.T) {
	path := filepath.Join(t.TempDir(), "ll_uplink_server-1.prom")
	if err := os.WriteFile(path, []byte("stale"), 0o644); err != nil {
		t.Fatal(err)
	}
	err := Write(path, Snapshot{
		Server:       "server-1",
		LastSuccess:  time.Unix(1793563200, 0),
		PendingFiles: 3,
		PendingBytes: 18734211,
		Failures:     map[string]int{"transient": 2, "auth": 0, "invalid": 1},
		Confirmed:    5,
	})
	if err != nil {
		t.Fatal(err)
	}
	want := `# HELP ll_uplink_last_success_timestamp_seconds Unix time the website last accepted a file or a game.
# TYPE ll_uplink_last_success_timestamp_seconds gauge
ll_uplink_last_success_timestamp_seconds{server="server-1"} 1793563200
# HELP ll_uplink_pending_files Files of finished games the website does not hold yet.
# TYPE ll_uplink_pending_files gauge
ll_uplink_pending_files{server="server-1"} 3
# HELP ll_uplink_pending_bytes Uncompressed size of the pending files.
# TYPE ll_uplink_pending_bytes gauge
ll_uplink_pending_bytes{server="server-1"} 18734211
# HELP ll_uplink_failures_total Failed upload attempts by kind since the service started.
# TYPE ll_uplink_failures_total counter
ll_uplink_failures_total{server="server-1",kind="auth"} 0
ll_uplink_failures_total{server="server-1",kind="invalid"} 1
ll_uplink_failures_total{server="server-1",kind="transient"} 2
# HELP ll_uplink_confirmed_games_total Games the website confirmed since the service started.
# TYPE ll_uplink_confirmed_games_total counter
ll_uplink_confirmed_games_total{server="server-1"} 5
`
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != want {
		t.Fatalf("got\n%s\nwant\n%s", got, want)
	}
	if _, err := os.Stat(path + ".tmp"); !os.IsNotExist(err) {
		t.Errorf("temporary file left behind: %v", err)
	}
}

func TestWriteBeforeAnySuccessReportsZero(t *testing.T) {
	path := filepath.Join(t.TempDir(), "m.prom")
	if err := Write(path, Snapshot{Server: "server-1"}); err != nil {
		t.Fatal(err)
	}
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(got), `ll_uplink_last_success_timestamp_seconds{server="server-1"} 0`+"\n") {
		t.Fatalf("got\n%s", got)
	}
}

func TestWriteWithoutPathDoesNothing(t *testing.T) {
	dir := t.TempDir()
	t.Chdir(dir)
	if err := Write("", Snapshot{Server: "server-1", Confirmed: 1}); err != nil {
		t.Fatal(err)
	}
	entries, err := os.ReadDir(dir)
	if err != nil || len(entries) != 0 {
		t.Fatalf("wrote %v, err %v", entries, err)
	}
}
