package send

import (
	"bytes"
	"io"
	"testing"
	"time"
)

func TestLimiterNeverExceedsTheCap(t *testing.T) {
	const capBytes = 125_000
	clock := newFakeClock()
	l := NewLimiter(capBytes, clock)
	start := clock.Now()

	type sent struct {
		at time.Time
		n  int
	}
	var log []sent
	send := func(total, chunk int) {
		r := l.Reader(bytes.NewReader(make([]byte, total)))
		buf := make([]byte, chunk)
		for {
			n, err := r.Read(buf)
			if n > 0 {
				log = append(log, sent{clock.Now(), n})
			}
			if err == io.EOF {
				return
			}
			if err != nil {
				t.Fatal(err)
			}
		}
	}
	idle := func(d time.Duration) time.Duration {
		clock.Advance(d)
		return d
	}
	send(12*capBytes, 64<<10)
	paused := idle(3 * time.Second)
	send(5*capBytes, 1000)
	paused += idle(700 * time.Millisecond)
	send(8*capBytes, 32<<10)
	paused += idle(20 * time.Second)
	send(capBytes/2, 64<<10)

	total := 0
	for _, s := range log {
		total += s.n
	}
	if total != 25*capBytes+capBytes/2 {
		t.Fatalf("passed %d bytes, want all of them", total)
	}
	for _, window := range []time.Duration{time.Second, 10 * time.Second} {
		limit := capBytes * int(window/time.Second)
		for i := range log {
			sum := 0
			for j := i; j < len(log) && log[j].at.Sub(log[i].at) <= window; j++ {
				sum += log[j].n
			}
			if sum > limit {
				t.Fatalf("%v from %v carried %d bytes, cap %d", window, log[i].at.Sub(start), sum, limit)
			}
		}
	}

	busy := log[len(log)-1].at.Sub(start) - paused
	if slowest := time.Duration(float64(total) / (0.9 * capBytes) * float64(time.Second)); busy > slowest+time.Millisecond {
		t.Errorf("sending took %v, longer than %v at nine tenths of the cap", busy, slowest)
	}
}

func TestLimiterBurstIsATenthOfTheCap(t *testing.T) {
	const capBytes = 1_000_000
	clock := newFakeClock()
	l := NewLimiter(capBytes, clock)
	r := l.Reader(bytes.NewReader(make([]byte, capBytes)))
	buf := make([]byte, capBytes)
	n, err := r.Read(buf)
	if err != nil {
		t.Fatal(err)
	}
	if n != capBytes/10 || !clock.Now().Equal(time.Unix(1793563200, 0)) {
		t.Fatalf("first read passed %d bytes after %v, want a tenth of the cap at once", n, clock.Now().Sub(time.Unix(1793563200, 0)))
	}
}
