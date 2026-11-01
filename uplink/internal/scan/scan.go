// Package scan reads what the recorder and the statistics wrote under a server profile and
// decides which games are over. It deletes nothing but files the website confirmed (Retain).
package scan

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"time"
)

const (
	StateIdle   = "idle"
	StateResume = "resume"
	StateRec    = "rec"
)

const (
	OverStopped   = "stopped"
	OverAbandoned = "abandoned"
)

const (
	minStatusUnix  = 1_000_000_000
	identitySuffix = "-identities.txt"
)

var (
	segmentName = regexp.MustCompile(`^(.+)-([1-9][0-9]{0,8})\.llr$`)
	statsPhases = [...]string{"approved", "final", "live"}
)

// Status is the recorder's status line. The zero Status stands for a file that could not be
// read, and GamesOver holds every game for it.
type Status struct {
	State   string
	Session string
	Segment int
	Unix    int64
}

// ReadStatus fails on a line that does not parse, including one cut inside its time field
// (a number before 2001): the recorder is rewriting the file, and the next pass retries.
func ReadStatus(profile string) (Status, error) {
	data, err := os.ReadFile(filepath.Join(profile, "LL_Replays", "status.txt"))
	if err != nil {
		return Status{}, err
	}
	st, ok := parseStatus(string(data))
	if !ok {
		return Status{}, fmt.Errorf("status.txt: cannot parse %.100q", data)
	}
	return st, nil
}

func parseStatus(s string) (Status, bool) {
	line, rest, _ := strings.Cut(s, "\n")
	if rest != "" {
		return Status{}, false
	}
	f := strings.Split(strings.TrimSuffix(line, "\r"), " ")
	if len(f) != 6 || f[0] != "LLS" || f[1] != "1" {
		return Status{}, false
	}
	st := Status{State: f[2]}
	switch st.State {
	case StateIdle, StateResume, StateRec:
	default:
		return Status{}, false
	}
	if f[3] != "-" {
		if f[3] == "" || strings.ContainsRune(f[3], '"') {
			return Status{}, false
		}
		st.Session = f[3]
	}
	if f[4] != "-" {
		n, err := strconv.Atoi(f[4])
		if err != nil || n < 1 {
			return Status{}, false
		}
		st.Segment = n
	}
	unix, err := strconv.ParseInt(f[5], 10, 64)
	if err != nil || unix < minStatusUnix {
		return Status{}, false
	}
	st.Unix = unix
	if st.State == StateRec && (st.Session == "" || st.Segment == 0) {
		return Status{}, false
	}
	return st, true
}

type Segment struct {
	Number  int
	Path    string
	ModTime time.Time
}

type Recording struct {
	Session  string
	Segments []Segment
	// Identity is the path of the identity file, "" when no player appeared.
	Identity string
	// LastWrite also counts the session's statistics files, whose autosaves keep a running
	// game from looking abandoned when its recording stopped writing.
	LastWrite time.Time
}

// ListRecordings returns the sessions with at least one segment file, oldest first.
func ListRecordings(profile string) ([]Recording, error) {
	found, err := replayFiles(profile)
	if err != nil {
		return nil, err
	}
	var recs []Recording
	for _, r := range found {
		if len(r.Segments) == 0 {
			continue
		}
		for _, p := range statsPaths(profile, r.Session) {
			if info, err := os.Stat(p); err == nil && info.ModTime().After(r.LastWrite) {
				r.LastWrite = info.ModTime()
			}
		}
		recs = append(recs, *r)
	}
	slices.SortFunc(recs, func(a, b Recording) int { return strings.Compare(a.Session, b.Session) })
	return recs, nil
}

// LatestStats returns the session's statistics file of the latest phase (approved, final,
// live), or "" when there is none.
func LatestStats(profile, session string) (string, error) {
	for _, p := range statsPaths(profile, session) {
		_, err := os.Stat(p)
		if err == nil {
			return p, nil
		}
		if !errors.Is(err, fs.ErrNotExist) {
			return "", err
		}
	}
	return "", nil
}

// GamesOver maps each recording that is over to stopped or abandoned. Nothing is over while the
// status is unreadable or a resume is pending, and a rec status holds its session until stale.
func GamesOver(now time.Time, st Status, recs []Recording, wait time.Duration) map[string]string {
	over := map[string]string{}
	if st.State == "" || st.State == StateResume {
		return over
	}
	written := time.Unix(st.Unix, 0)
	for _, r := range recs {
		stale := now.Sub(written) > wait && now.Sub(r.LastWrite) > wait
		if st.State == StateRec && st.Session == r.Session && !stale {
			continue
		}
		switch {
		case st.Session != r.Session && st.Unix > r.LastWrite.Unix():
			over[r.Session] = OverStopped
		case stale:
			over[r.Session] = OverAbandoned
		}
	}
	return over
}

func replayFiles(profile string) (map[string]*Recording, error) {
	dir := filepath.Join(profile, "LL_Replays")
	entries, err := os.ReadDir(dir)
	if errors.Is(err, fs.ErrNotExist) {
		return map[string]*Recording{}, nil
	}
	if err != nil {
		return nil, err
	}
	found := map[string]*Recording{}
	at := func(session string) *Recording {
		r := found[session]
		if r == nil {
			r = &Recording{Session: session}
			found[session] = r
		}
		return r
	}
	for _, e := range entries {
		if !e.Type().IsRegular() {
			continue
		}
		info, err := e.Info()
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, err
		}
		path := filepath.Join(dir, e.Name())
		if m := segmentName.FindStringSubmatch(e.Name()); m != nil {
			n, _ := strconv.Atoi(m[2])
			r := at(m[1])
			r.Segments = append(r.Segments, Segment{Number: n, Path: path, ModTime: info.ModTime()})
			r.LastWrite = later(r.LastWrite, info.ModTime())
		} else if session, ok := strings.CutSuffix(e.Name(), identitySuffix); ok && session != "" {
			r := at(session)
			r.Identity = path
			r.LastWrite = later(r.LastWrite, info.ModTime())
		}
	}
	for _, r := range found {
		slices.SortFunc(r.Segments, func(a, b Segment) int { return a.Number - b.Number })
	}
	return found, nil
}

func statsPaths(profile, session string) []string {
	paths := make([]string, len(statsPhases))
	for i, phase := range statsPhases {
		paths[i] = filepath.Join(profile, "LL_GameStats", session+"-"+phase+".json")
	}
	return paths
}

func later(a, b time.Time) time.Time {
	if b.After(a) {
		return b
	}
	return a
}
