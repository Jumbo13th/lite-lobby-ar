package scan

import (
	"crypto/sha256"
	"encoding/hex"
	"os"
	"slices"
	"testing"
	"time"

	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/state"
)

func sha(content string) string {
	sum := sha256.Sum256([]byte(content))
	return hex.EncodeToString(sum[:])
}

func TestRetainDeletesOnlyConfirmedFilesPastRetention(t *testing.T) {
	profile := t.TempDir()
	now := time.Unix(1793563200, 0)
	keep := 14 * 24 * time.Hour
	expired := now.Add(-15 * 24 * time.Hour)
	st, err := state.Load(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	type file struct{ path, content string }
	add := func(rel, content string) file {
		return file{put(t, profile, rel, content, time.Time{}), content}
	}
	confirm := func(session string, at time.Time, segments []file, identity, stats *file) {
		g := state.GameRec{Over: OverEnded, Confirmed: at}
		for _, s := range segments {
			g.Segments = append(g.Segments, sha(s.content))
			st.Files[sha(s.content)] = state.FileRec{Kind: "segment", Session: session}
		}
		if identity != nil {
			g.Identity = sha(identity.content)
			st.Files[g.Identity] = state.FileRec{Kind: "identity", Session: session}
		}
		if stats != nil {
			g.Stats = sha(stats.content)
			st.Files[g.Stats] = state.FileRec{Kind: "stats", Session: session}
		}
		st.Games[session] = g
	}

	oldSeg := add("LL_Replays/old-1.llr", "LLR 1\nH session old\nEND debrief\n")
	oldIdent := add("LL_Replays/old-identities.txt", "LLI 1\nH session old\n")
	oldLive := add("LL_GameStats/old-live.json", `{"phase":"live"}`)
	oldFinal := add("LL_GameStats/old-final.json", `{"phase":"final"}`)
	confirm("old", expired, []file{oldSeg}, &oldIdent, &oldFinal)

	recentSeg := add("LL_Replays/recent-1.llr", "LLR 1\nH session recent\nEND debrief\n")
	recentStats := add("LL_GameStats/recent-final.json", `{"phase":"final","session":"recent"}`)
	confirm("recent", now.Add(-13*24*time.Hour), []file{recentSeg}, nil, &recentStats)

	openSeg := add("LL_Replays/open-1.llr", "LLR 1\nH session open\n")
	st.Games["open"] = state.GameRec{Failures: 4, LastError: "transient: HTTP 503"}

	pendingSeg := add("LL_Replays/pending-1.llr", "LLR 1\nH session pending\nEND debrief\n")
	pendingFinal := add("LL_GameStats/pending-final.json", `{"phase":"final","session":"pending"}`)
	pendingApproved := add("LL_GameStats/pending-approved.json", `{"phase":"approved","session":"pending"}`)
	confirm("pending", expired, []file{pendingSeg}, nil, &pendingFinal)

	lateSeg1 := add("LL_Replays/late-1.llr", "LLR 1\nH session late\n")
	lateIdent := add("LL_Replays/late-identities.txt", "LLI 1\nH session late\nI 1 x\n")
	confirm("late", expired, []file{lateSeg1}, &lateIdent, nil)
	lateSeg2 := add("LL_Replays/late-2.llr", "LLR 1\nH session late\nH segment 2\nEND debrief\n")

	grownSeg := add("LL_Replays/grown-1.llr", "LLR 1\nH session grown\nEND debrief\n")
	grownIdent := add("LL_Replays/grown-identities.txt", "LLI 1\nH session grown\nI 1 x\n")
	confirm("grown", expired, []file{grownSeg}, &grownIdent, nil)
	grownIdent = add("LL_Replays/grown-identities.txt", grownIdent.content+"I 2 y\n")

	status := add("LL_Replays/status.txt", "LLS 1 idle next - 1793563200\n")
	strayStats := add("LL_GameStats/stray-final.json", "{}")

	deleted, err := Retain(profile, st, now, keep)
	if err != nil {
		t.Fatal(err)
	}

	wantDeleted := []string{oldSeg.path, oldIdent.path, oldFinal.path, oldLive.path}
	slices.Sort(deleted)
	slices.Sort(wantDeleted)
	if !slices.Equal(deleted, wantDeleted) {
		t.Errorf("deleted %v\nwant %v", deleted, wantDeleted)
	}
	for _, f := range wantDeleted {
		if _, err := os.Stat(f); !os.IsNotExist(err) {
			t.Errorf("%s still exists", f)
		}
	}
	for _, f := range []file{recentSeg, recentStats, openSeg, pendingSeg, pendingFinal, pendingApproved, lateSeg1, lateSeg2, lateIdent, grownSeg, grownIdent, status, strayStats} {
		if _, err := os.Stat(f.path); err != nil {
			t.Errorf("%s was touched: %v", f.path, err)
		}
	}

	if _, ok := st.Games["old"]; ok {
		t.Error("game old not forgotten")
	}
	for _, kept := range []string{"recent", "open", "pending", "late", "grown"} {
		if _, ok := st.Games[kept]; !ok {
			t.Errorf("game %s forgotten", kept)
		}
	}
	for id, f := range st.Files {
		if f.Session == "old" {
			t.Errorf("file record %s of a forgotten game kept", id)
		}
	}
	if st.Files[sha(lateSeg1.content)].Session != "late" {
		t.Error("file records of a kept game dropped")
	}
}

func TestRetainWithoutRecordingsFolder(t *testing.T) {
	st, err := state.Load(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	deleted, err := Retain(t.TempDir(), st, time.Now(), time.Hour)
	if err != nil || len(deleted) != 0 {
		t.Fatalf("deleted = %v, err = %v", deleted, err)
	}
}
