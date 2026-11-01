// Command ll-uplink sends the finished games of one game server to the receiving website.
// One process serves one game server; it never listens and connects only to the website.
package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"slices"
	"syscall"
	"time"

	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/config"
	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/metrics"
	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/scan"
	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/send"
	"github.com/Jumbo13th/lite-lobby-ar/uplink/internal/state"
)

var version = "dev"

const passInterval = 15 * time.Second

func main() {
	configPath := flag.String("config", "", "path to the configuration file")
	showVersion := flag.Bool("version", false, "print the version and exit")
	flag.Parse()
	if *showVersion {
		fmt.Println(version)
		return
	}
	log := slog.New(slog.NewTextHandler(os.Stdout, nil))
	if *configPath == "" {
		log.Error("-config is required")
		os.Exit(2)
	}
	if err := run(*configPath, log); err != nil {
		log.Error("stopped", "error", err)
		os.Exit(1)
	}
}

func run(configPath string, log *slog.Logger) error {
	cfg, err := config.Load(configPath)
	if err != nil {
		return err
	}
	st, err := state.Load(cfg.StateDir)
	if err != nil {
		return err
	}
	clock := send.SystemClock{}
	limiter := send.NewLimiter(int64(cfg.RateCapKbit)*1000/8, clock)
	client := send.NewClient(cfg.Website, cfg.ServerID, cfg.Secret, limiter, send.Transport(), clock)
	s := &service{
		cfg:      cfg,
		state:    st,
		up:       send.NewUploader(client, st, clock, log),
		log:      log,
		now:      time.Now,
		reported: map[string]string{},
	}

	stop, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer cancel()
	log.Info("started", "version", version, "website", cfg.Website, "profile", cfg.Profile)
	ticker := time.NewTicker(passInterval)
	defer ticker.Stop()
	for {
		s.pass(stop)
		select {
		case <-stop.Done():
			log.Info("stopping")
			return st.Save()
		case <-ticker.C:
		}
	}
}

type service struct {
	cfg      config.Config
	state    *state.State
	up       *send.Uploader
	log      *slog.Logger
	now      func() time.Time
	reported map[string]string
}

func (s *service) pass(stop context.Context) {
	now := s.now()
	status, err := scan.ReadStatus(s.cfg.Profile)
	s.report("status file unreadable", err)
	recs, err := scan.ListRecordings(s.cfg.Profile)
	s.report("recordings unreadable", err)
	over := scan.GamesOver(now, status, recs, time.Duration(s.cfg.StoppedWaitMinutes)*time.Minute)

	var pending []string
	for _, r := range recs {
		how := over[r.Session]
		if how == "" {
			continue
		}
		g, err := s.game(r, how)
		s.report("statistics unreadable for "+r.Session, err)
		if err != nil {
			continue
		}
		if stop.Err() == nil {
			s.up.UploadGame(stop, g)
		}
		pending = append(pending, g.Segments...)
		pending = append(pending, g.Identity, g.Stats)
	}

	if stop.Err() == nil {
		s.retain(now)
	}

	files, size := s.up.Pending(slices.DeleteFunc(pending, func(p string) bool { return p == "" }))
	err = metrics.Write(s.cfg.MetricsFile, metrics.Snapshot{
		Server:       s.cfg.ServerID,
		LastSuccess:  s.up.LastSuccess(),
		PendingFiles: files,
		PendingBytes: size,
		Failures:     s.up.Failures(),
		Confirmed:    s.up.ConfirmedGames(),
	})
	s.report("metrics not written", err)
}

func (s *service) game(r scan.Recording, how string) (send.Game, error) {
	stats, err := scan.LatestStats(s.cfg.Profile, r.Session)
	if err != nil {
		return send.Game{}, err
	}
	g := send.Game{Session: r.Session, Over: how, Identity: r.Identity, Stats: stats}
	for _, seg := range r.Segments {
		g.Segments = append(g.Segments, seg.Path)
	}
	return g, nil
}

func (s *service) retain(now time.Time) {
	games := len(s.state.Games)
	deleted, err := scan.Retain(s.cfg.Profile, s.state, now, time.Duration(s.cfg.RetentionDays)*24*time.Hour)
	for _, p := range deleted {
		s.log.Info("deleted", "file", p)
	}
	s.up.Forget(deleted...)
	s.report("retention failed", err)
	if len(deleted) > 0 || len(s.state.Games) != games {
		if err := s.state.Save(); err != nil {
			s.log.Error("cannot save the state", "error", err)
		}
	}
}

func (s *service) report(problem string, err error) {
	text := ""
	if err != nil {
		text = err.Error()
	}
	if s.reported[problem] == text {
		return
	}
	s.reported[problem] = text
	if err != nil {
		s.log.Warn(problem, "error", err)
		return
	}
	s.log.Info(problem + ": cleared")
}
