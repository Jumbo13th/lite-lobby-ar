// Package config loads the service configuration. It refuses a file that anyone but its
// owner can read, because the file holds the secret.
package config

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/url"
	"os"
	"regexp"
	"runtime"
	"strings"
	"unicode/utf8"
)

// Config serves one game server; the fields and their rules are in the config-files contract.
type Config struct {
	Website            string
	ServerID           string `json:"ServerId"`
	Secret             string
	Profile            string
	StateDir           string
	RateCapKbit        int
	StoppedWaitMinutes int
	RetentionDays      int
	MetricsFile        string
}

var serverIDPattern = regexp.MustCompile(`^[a-z0-9-]{1,64}$`)

// Load skips the permission check on Windows, whose ACLs have no group and other bits.
func Load(path string) (Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return Config{}, err
	}
	defer f.Close()

	if runtime.GOOS != "windows" {
		info, err := f.Stat()
		if err != nil {
			return Config{}, err
		}
		if perm := info.Mode().Perm(); perm&0o077 != 0 {
			return Config{}, fmt.Errorf("%s: mode %04o allows group or other access; allow the owner only (chmod 600)", path, perm)
		}
	}

	c := Config{RateCapKbit: 8000, StoppedWaitMinutes: 30, RetentionDays: 14}
	dec := json.NewDecoder(f)
	dec.DisallowUnknownFields()
	if err := dec.Decode(&c); err != nil {
		return Config{}, fmt.Errorf("%s: %w", path, err)
	}
	if _, err := dec.Token(); err != io.EOF {
		return Config{}, fmt.Errorf("%s: content after the JSON object", path)
	}
	if err := c.validate(); err != nil {
		return Config{}, fmt.Errorf("%s: %w", path, err)
	}
	c.Website = strings.TrimRight(c.Website, "/")
	return c, nil
}

func (c *Config) validate() error {
	if err := checkWebsite(c.Website); err != nil {
		return err
	}
	if !serverIDPattern.MatchString(c.ServerID) {
		return errors.New("ServerId must be 1 to 64 characters of a-z, 0-9 and -")
	}
	if utf8.RuneCountInString(c.Secret) < 32 {
		return errors.New("Secret must be at least 32 characters")
	}
	if info, err := os.Stat(c.Profile); err != nil || !info.IsDir() {
		return fmt.Errorf("Profile must be an existing folder: %q", c.Profile)
	}
	if err := checkWritable(c.StateDir); err != nil {
		return err
	}
	if err := checkRange("RateCapKbit", c.RateCapKbit, 100, 1_000_000); err != nil {
		return err
	}
	if err := checkRange("StoppedWaitMinutes", c.StoppedWaitMinutes, 5, 1440); err != nil {
		return err
	}
	return checkRange("RetentionDays", c.RetentionDays, 1, 365)
}

func checkWebsite(s string) error {
	u, err := url.Parse(s)
	if !strings.HasPrefix(s, "https://") || err != nil || u.Host == "" {
		return errors.New("Website must be an https:// address")
	}
	if u.User != nil || strings.ContainsAny(s, "?#") {
		return errors.New("Website must not carry a user, a query or a fragment")
	}
	return nil
}

func checkWritable(dir string) error {
	if dir == "" {
		return errors.New("StateDir is required")
	}
	if err := os.MkdirAll(dir, 0o700); err != nil {
		return fmt.Errorf("StateDir: %w", err)
	}
	f, err := os.CreateTemp(dir, ".write-check-*")
	if err != nil {
		return fmt.Errorf("StateDir is not writable: %w", err)
	}
	f.Close()
	if err := os.Remove(f.Name()); err != nil {
		return fmt.Errorf("StateDir: %w", err)
	}
	return nil
}

func checkRange(name string, v, lo, hi int) error {
	if v < lo || v > hi {
		return fmt.Errorf("%s must be %d to %d, not %d", name, lo, hi, v)
	}
	return nil
}
