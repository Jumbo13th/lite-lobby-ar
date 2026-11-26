// Package metrics writes the service's state in the Prometheus text format. The file is
// replaced through a rename, so a collector never reads half of it.
package metrics

import (
	"fmt"
	"maps"
	"os"
	"slices"
	"strings"
	"time"
)

// Snapshot is one pass's report. Server labels every series, so that the files of several
// instances on one host can sit in the same collector folder.
type Snapshot struct {
	Server       string
	LastSuccess  time.Time
	PendingFiles int
	PendingBytes int64
	Failures     map[string]int
	Confirmed    int
}

// Write does nothing when path is empty.
func Write(path string, s Snapshot) error {
	if path == "" {
		return nil
	}
	var b strings.Builder
	server := fmt.Sprintf("server=%q", s.Server)
	last := int64(0)
	if !s.LastSuccess.IsZero() {
		last = s.LastSuccess.Unix()
	}
	metric := func(name, kind, help string) {
		fmt.Fprintf(&b, "# HELP %s %s\n# TYPE %s %s\n", name, help, name, kind)
	}
	metric("ll_uplink_last_success_timestamp_seconds", "gauge", "Unix time the website last accepted a file or a game.")
	fmt.Fprintf(&b, "ll_uplink_last_success_timestamp_seconds{%s} %d\n", server, last)
	metric("ll_uplink_pending_files", "gauge", "Files of finished games the website does not hold yet.")
	fmt.Fprintf(&b, "ll_uplink_pending_files{%s} %d\n", server, s.PendingFiles)
	metric("ll_uplink_pending_bytes", "gauge", "Uncompressed size of the pending files.")
	fmt.Fprintf(&b, "ll_uplink_pending_bytes{%s} %d\n", server, s.PendingBytes)
	metric("ll_uplink_failures_total", "counter", "Failed upload attempts by kind since the service started.")
	for _, kind := range slices.Sorted(maps.Keys(s.Failures)) {
		fmt.Fprintf(&b, "ll_uplink_failures_total{%s,kind=%q} %d\n", server, kind, s.Failures[kind])
	}
	metric("ll_uplink_confirmed_games_total", "counter", "Games the website confirmed since the service started.")
	fmt.Fprintf(&b, "ll_uplink_confirmed_games_total{%s} %d\n", server, s.Confirmed)

	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, []byte(b.String()), 0o644); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}
