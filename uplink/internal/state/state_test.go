package state

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"
	"time"
)

func TestLoadMissingFileGivesEmptyState(t *testing.T) {
	s, err := Load(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	if s.Files == nil || s.Games == nil || len(s.Files) != 0 || len(s.Games) != 0 {
		t.Fatalf("not an empty state: %+v", s)
	}
}

func TestSaveAndLoadRoundTrip(t *testing.T) {
	dir := t.TempDir()
	s, err := Load(dir)
	if err != nil {
		t.Fatal(err)
	}
	at := time.Date(2026, 10, 31, 20, 15, 0, 0, time.UTC)
	s.Files["3f2a"] = FileRec{Kind: "segment", Session: "20261031-201500-eden", Name: "20261031-201500-eden-1.llr", Committed: at}
	s.Files["c09d"] = FileRec{Kind: "identity", Session: "20261031-201500-eden", Name: "20261031-201500-eden-identities.txt", Committed: at}
	s.Games["20261031-201500-eden"] = GameRec{Over: "ended", Segments: []string{"3f2a"}, Identity: "c09d", Confirmed: at.Add(time.Minute)}
	s.Games["20261031-231000-eden"] = GameRec{Failures: 3, LastError: "transient: HTTP 503"}
	if err := s.Save(); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(dir, fileName+".tmp")); !os.IsNotExist(err) {
		t.Errorf("temporary file left behind: %v", err)
	}

	back, err := Load(dir)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(back.Files, s.Files) || !reflect.DeepEqual(back.Games, s.Games) {
		t.Fatalf("round trip changed the state:\n got %+v\nwant %+v", back, s)
	}

	back.Games["20261031-231000-eden"] = GameRec{}
	if err := back.Save(); err != nil {
		t.Fatalf("second save over an existing file: %v", err)
	}
}

func TestLoadRefusesCorruptFile(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, fileName), []byte(`{"files":`), 0o600); err != nil {
		t.Fatal(err)
	}
	if _, err := Load(dir); err == nil {
		t.Fatal("corrupt state accepted")
	}
}

func TestForgetDropsTheGameAndItsFiles(t *testing.T) {
	s, err := Load(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	s.Files["a"] = FileRec{Session: "one"}
	s.Files["b"] = FileRec{Session: "two"}
	s.Games["one"] = GameRec{Over: "ended"}
	s.Games["two"] = GameRec{Over: "stopped"}
	s.Forget("one")
	if _, ok := s.Games["one"]; ok {
		t.Error("game kept")
	}
	if _, ok := s.Files["a"]; ok {
		t.Error("file record kept")
	}
	if len(s.Games) != 1 || len(s.Files) != 1 {
		t.Errorf("other game touched: %+v", s)
	}
}
