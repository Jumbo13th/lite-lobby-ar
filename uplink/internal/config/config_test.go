package config

import (
	"encoding/json"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func validFields(t *testing.T) map[string]any {
	t.Helper()
	return map[string]any{
		"Website":  "https://example.org/api/replays/v1",
		"ServerId": "server-1",
		"Secret":   strings.Repeat("s", 32),
		"Profile":  t.TempDir(),
		"StateDir": filepath.Join(t.TempDir(), "state"),
	}
}

func writeFields(t *testing.T, fields map[string]any) string {
	t.Helper()
	data, err := json.Marshal(fields)
	if err != nil {
		t.Fatal(err)
	}
	return writeRaw(t, data)
}

func writeRaw(t *testing.T, data []byte) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "uplink.json")
	if err := os.WriteFile(path, data, 0o600); err != nil {
		t.Fatal(err)
	}
	return path
}

func loadWith(t *testing.T, key string, value any) error {
	t.Helper()
	fields := validFields(t)
	if value == nil {
		delete(fields, key)
	} else {
		fields[key] = value
	}
	_, err := Load(writeFields(t, fields))
	return err
}

func TestLoadAppliesDefaults(t *testing.T) {
	c, err := Load(writeFields(t, validFields(t)))
	if err != nil {
		t.Fatal(err)
	}
	if c.RateCapKbit != 8000 || c.StoppedWaitMinutes != 30 || c.RetentionDays != 14 || c.MetricsFile != "" {
		t.Fatalf("defaults not applied: %+v", c)
	}
}

func TestLoadKeepsGivenValues(t *testing.T) {
	fields := validFields(t)
	fields["Website"] = "https://example.org/api/replays/v1/"
	fields["RateCapKbit"] = 100
	fields["StoppedWaitMinutes"] = 1440
	fields["RetentionDays"] = 365
	fields["MetricsFile"] = "/var/lib/metrics/ll_uplink.prom"
	c, err := Load(writeFields(t, fields))
	if err != nil {
		t.Fatal(err)
	}
	if c.Website != "https://example.org/api/replays/v1" {
		t.Errorf("Website = %q, want the trailing slash trimmed", c.Website)
	}
	if c.ServerID != "server-1" || c.Secret != fields["Secret"] || c.Profile != fields["Profile"] || c.StateDir != fields["StateDir"] {
		t.Errorf("fields not loaded: %+v", c)
	}
	if c.RateCapKbit != 100 || c.StoppedWaitMinutes != 1440 || c.RetentionDays != 365 || c.MetricsFile != fields["MetricsFile"] {
		t.Errorf("optional fields not loaded: %+v", c)
	}
}

func TestLoadRefusesUnknownField(t *testing.T) {
	if err := loadWith(t, "Webiste", "https://example.org"); err == nil {
		t.Fatal("unknown field accepted")
	}
}

func TestLoadRefusesTrailingContent(t *testing.T) {
	data, err := json.Marshal(validFields(t))
	if err != nil {
		t.Fatal(err)
	}
	if _, err := Load(writeRaw(t, append(data, []byte(" {}")...))); err == nil {
		t.Fatal("content after the object accepted")
	}
}

func TestWebsiteMustBeHTTPS(t *testing.T) {
	for _, website := range []any{nil, "", "http://example.org/api", "ftp://example.org", "https://", "example.org", "HTTPS://example.org", "https://user:pass@example.org/api", "https://example.org/api?key=1", "https://example.org/api#top"} {
		if err := loadWith(t, "Website", website); err == nil {
			t.Errorf("Website %v accepted", website)
		}
	}
}

func TestServerIDPattern(t *testing.T) {
	for _, id := range []string{"a", "server-1", "0-9", strings.Repeat("a", 64)} {
		if err := loadWith(t, "ServerId", id); err != nil {
			t.Errorf("ServerId %q refused: %v", id, err)
		}
	}
	for _, id := range []any{nil, "", "Server-1", "server_1", "server 1", "сервер", strings.Repeat("a", 65)} {
		if err := loadWith(t, "ServerId", id); err == nil {
			t.Errorf("ServerId %v accepted", id)
		}
	}
}

func TestSecretLength(t *testing.T) {
	if err := loadWith(t, "Secret", strings.Repeat("s", 31)); err == nil {
		t.Error("31-character secret accepted")
	}
	if err := loadWith(t, "Secret", nil); err == nil {
		t.Error("missing secret accepted")
	}
	if err := loadWith(t, "Secret", strings.Repeat("ж", 32)); err != nil {
		t.Errorf("32-character secret refused: %v", err)
	}
}

func TestProfileMustBeFolder(t *testing.T) {
	file := filepath.Join(t.TempDir(), "profile.txt")
	if err := os.WriteFile(file, nil, 0o600); err != nil {
		t.Fatal(err)
	}
	for _, profile := range []any{nil, filepath.Join(t.TempDir(), "absent"), file} {
		if err := loadWith(t, "Profile", profile); err == nil {
			t.Errorf("Profile %v accepted", profile)
		}
	}
}

func TestStateDirMustBeWritable(t *testing.T) {
	if err := loadWith(t, "StateDir", nil); err == nil {
		t.Error("missing StateDir accepted")
	}
	file := filepath.Join(t.TempDir(), "file")
	if err := os.WriteFile(file, nil, 0o600); err != nil {
		t.Fatal(err)
	}
	if err := loadWith(t, "StateDir", filepath.Join(file, "state")); err == nil {
		t.Error("StateDir below a file accepted")
	}
	dir := filepath.Join(t.TempDir(), "new", "state")
	if err := loadWith(t, "StateDir", dir); err != nil {
		t.Fatalf("absent StateDir refused: %v", err)
	}
	if info, err := os.Stat(dir); err != nil || !info.IsDir() {
		t.Error("absent StateDir not created")
	}
}

func TestNumericRanges(t *testing.T) {
	cases := []struct {
		field  string
		lo, hi int
	}{
		{"RateCapKbit", 100, 1_000_000},
		{"StoppedWaitMinutes", 5, 1440},
		{"RetentionDays", 1, 365},
	}
	for _, c := range cases {
		for _, v := range []int{c.lo, c.hi} {
			if err := loadWith(t, c.field, v); err != nil {
				t.Errorf("%s = %d refused: %v", c.field, v, err)
			}
		}
		for _, v := range []int{0, -1, c.lo - 1, c.hi + 1} {
			if err := loadWith(t, c.field, v); err == nil {
				t.Errorf("%s = %d accepted", c.field, v)
			}
		}
	}
}

func TestPermissionCheck(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("the permission check is skipped on Windows")
	}
	path := writeFields(t, validFields(t))
	for _, mode := range []os.FileMode{0o640, 0o604, 0o644, 0o660, 0o666} {
		if err := os.Chmod(path, mode); err != nil {
			t.Fatal(err)
		}
		if _, err := Load(path); err == nil {
			t.Errorf("mode %04o accepted", mode)
		}
	}
	for _, mode := range []os.FileMode{0o600, 0o400} {
		if err := os.Chmod(path, mode); err != nil {
			t.Fatal(err)
		}
		if _, err := Load(path); err != nil {
			t.Errorf("mode %04o refused: %v", mode, err)
		}
	}
}
