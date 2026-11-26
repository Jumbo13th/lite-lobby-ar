// Package state keeps what the website confirmed, so that a restart neither loses nor
// repeats work.
package state

import (
	"encoding/json"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"time"
)

const fileName = "state.json"

// FileRec is a file the website holds complete, keyed in State.Files by its identity.
type FileRec struct {
	Kind      string    `json:"kind"`
	Session   string    `json:"session"`
	Name      string    `json:"name"`
	Committed time.Time `json:"committed"`
}

// GameRec is a game that was over at least once. Over and the file identities are the set the
// website confirmed last, at Confirmed; every repeated post replaces that set.
type GameRec struct {
	Over      string    `json:"over,omitempty"`
	Segments  []string  `json:"segments,omitempty"`
	Identity  string    `json:"identity,omitempty"`
	Stats     string    `json:"stats,omitempty"`
	Confirmed time.Time `json:"confirmed,omitzero"`
	Failures  int       `json:"failures,omitempty"`
	LastError string    `json:"lastError,omitempty"`
}

type State struct {
	Files map[string]FileRec `json:"files"`
	Games map[string]GameRec `json:"games"`

	dir string
}

// Load returns an empty state when the folder holds none yet.
func Load(dir string) (*State, error) {
	s := &State{dir: dir}
	data, err := os.ReadFile(filepath.Join(dir, fileName))
	switch {
	case errors.Is(err, fs.ErrNotExist):
	case err != nil:
		return nil, err
	default:
		if err := json.Unmarshal(data, s); err != nil {
			return nil, fmt.Errorf("%s: %w", filepath.Join(dir, fileName), err)
		}
	}
	if s.Files == nil {
		s.Files = map[string]FileRec{}
	}
	if s.Games == nil {
		s.Games = map[string]GameRec{}
	}
	return s, nil
}

// Save writes a temporary file and renames it over the state, so a crash leaves either the
// old or the new state, never a torn one.
func (s *State) Save() error {
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	tmp := filepath.Join(s.dir, fileName+".tmp")
	f, err := os.OpenFile(tmp, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0o600)
	if err != nil {
		return err
	}
	_, err = f.Write(append(data, '\n'))
	if err == nil {
		err = f.Sync()
	}
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		os.Remove(tmp)
		return err
	}
	return os.Rename(tmp, filepath.Join(s.dir, fileName))
}

// Forget drops a game and the records of its files, once its local files are deleted.
func (s *State) Forget(session string) {
	delete(s.Games, session)
	for id, f := range s.Files {
		if f.Session == session {
			delete(s.Files, id)
		}
	}
}
