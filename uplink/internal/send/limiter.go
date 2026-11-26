package send

import (
	"io"
	"sync"
	"time"
)

// Clock is the time source of the limiter, the client and the uploader, so that tests can
// pass hours of waiting at once.
type Clock interface {
	Now() time.Time
	Sleep(d time.Duration)
}

type SystemClock struct{}

func (SystemClock) Now() time.Time        { return time.Now() }
func (SystemClock) Sleep(d time.Duration) { time.Sleep(d) }

// Limiter is a token bucket on request body bytes. It refills at nine tenths of the cap and
// holds a tenth of a second, so no window of a second or longer carries more than the cap.
type Limiter struct {
	clock Clock
	rate  int64
	burst int64

	mu     sync.Mutex
	fullAt time.Time
}

func NewLimiter(capBytesPerSecond int64, clock Clock) *Limiter {
	return &Limiter{
		clock: clock,
		rate:  max(capBytesPerSecond*9/10, 1),
		burst: max(capBytesPerSecond/10, 1),
	}
}

func (l *Limiter) Reader(r io.Reader) io.Reader {
	return &limitedReader{r: r, l: l}
}

type limitedReader struct {
	r io.Reader
	l *Limiter
}

func (lr *limitedReader) Read(p []byte) (int, error) {
	if int64(len(p)) > lr.l.burst {
		p = p[:lr.l.burst]
	}
	n, err := lr.r.Read(p)
	if n > 0 {
		lr.l.wait(int64(n))
	}
	return n, err
}

func (l *Limiter) wait(n int64) {
	l.mu.Lock()
	now := l.clock.Now()
	if l.fullAt.Before(now) {
		l.fullAt = now
	}
	at := l.fullAt.Add(-l.span(l.burst-n, false))
	l.fullAt = l.fullAt.Add(l.span(n, true))
	l.mu.Unlock()
	if d := at.Sub(now); d > 0 {
		l.clock.Sleep(d)
	}
}

func (l *Limiter) span(n int64, roundUp bool) time.Duration {
	ns := n * int64(time.Second)
	d := ns / l.rate
	if roundUp && ns%l.rate != 0 {
		d++
	}
	return time.Duration(d)
}
