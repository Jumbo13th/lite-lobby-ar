package scan

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"io"
	"io/fs"
	"maps"
	"os"
	"slices"
	"time"

	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/state"
)

// Retain deletes the files of games last confirmed more than keep ago and forgets them. A game
// holding any file whose content the website has not confirmed keeps them all until its next post.
func Retain(profile string, st *state.State, now time.Time, keep time.Duration) ([]string, error) {
	found, err := replayFiles(profile)
	if err != nil {
		return nil, err
	}
	var deleted []string
	for _, session := range slices.Sorted(maps.Keys(st.Games)) {
		g := st.Games[session]
		if g.Confirmed.IsZero() || now.Sub(g.Confirmed) <= keep {
			continue
		}
		doomed, settled, err := confirmedFiles(profile, session, g, found[session])
		if err != nil {
			return deleted, err
		}
		if !settled {
			continue
		}
		for _, p := range doomed {
			err := os.Remove(p)
			if err == nil {
				deleted = append(deleted, p)
				continue
			}
			if !errors.Is(err, fs.ErrNotExist) {
				return deleted, err
			}
		}
		st.Forget(session)
	}
	return deleted, nil
}

func confirmedFiles(profile, session string, g state.GameRec, r *Recording) ([]string, bool, error) {
	var stats []string
	for _, p := range statsPaths(profile, session) {
		_, err := os.Stat(p)
		if err == nil {
			stats = append(stats, p)
			continue
		}
		if !errors.Is(err, fs.ErrNotExist) {
			return nil, false, err
		}
	}
	if len(stats) > 0 {
		id, err := fileSHA256(stats[0])
		if err != nil || id != g.Stats {
			return nil, false, err
		}
	}
	var doomed []string
	if r != nil {
		for _, s := range r.Segments {
			id, err := fileSHA256(s.Path)
			if err != nil || !slices.Contains(g.Segments, id) {
				return nil, false, err
			}
			doomed = append(doomed, s.Path)
		}
		if r.Identity != "" {
			id, err := fileSHA256(r.Identity)
			if err != nil || id != g.Identity {
				return nil, false, err
			}
			doomed = append(doomed, r.Identity)
		}
	}
	return append(doomed, stats...), true, nil
}

func fileSHA256(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil)), nil
}
