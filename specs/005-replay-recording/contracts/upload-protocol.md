# Contract: upload protocol `ll-upload/1`

What the host service sends to the receiving website, and what the receiver answers.
Any website can implement the receiving side from this document alone. The service is
the only client; the receiver never calls the service and the service never listens.

## Transport

- HTTPS only, to the base address in the service configuration (for example
  `https://example.org/api/replays/v1`). Paths below are relative to it.
- The service sends one request at a time and never more bytes per second than its
  configured cap, counting request bodies.
- No request body is larger than **1,500,000 bytes**. Receivers commonly cap request
  bodies at 2 MB; staying under that keeps the receiver free to reject anything bigger.
- The service gives each request 60 seconds plus the time its rate cap needs to send
  the body, so a slow cap never makes a piece impossible to send.

## Authentication

Every request carries four headers:

| Header | Value |
|--------|-------|
| `X-LL-Server` | the server identity from the configuration: 1 to 64 characters of `a-z`, `0-9`, `-` |
| `X-LL-Timestamp` | the request time as Unix seconds, from the host clock |
| `X-LL-Content-SHA256` | lowercase hex SHA-256 of the request body; of the empty string when there is none |
| `X-LL-Signature` | lowercase hex HMAC-SHA256, keyed with the server's secret, of the canonical string |

The canonical string is the request method, the path relative to the base address with
its query, the timestamp and the content hash, joined with a single line feed and
without a final one.

Test vector: secret `test-secret`, body `hello`, method `PUT`, timestamp `1793563200`:

```text
PUT
/files/2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824/pieces/0
1793563200
2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
```

gives `X-LL-Signature: d2af137f20a18b26fe7ee727c7fa683ec1e570cf724f8bca9df6b975a211829a`.
The content hash of an empty body is
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

The receiver checks, in this order and from the headers alone, before it reads the
body: the server identity is known; the timestamp is within **300 seconds** of its own
clock; on a `PUT` or `POST`, `Content-Length` is present and within the limit for the
endpoint (a `GET` has no body and need not carry one); the signature matches, compared
in constant time. Any failure is answered `401` (or `413` for the
length) without reading the body. After reading the body, the receiver checks that its
SHA-256 equals `X-LL-Content-SHA256` and answers `400` if not.

The secret never appears in an address, a query string or a log line. A request
captured and replayed later than the window is refused by the timestamp check; a
replay inside the window is harmless because every operation below is idempotent.

## Files

A file is one recording segment, one identity file, or one statistics file. Its
identity is the lowercase hex SHA-256 of its **uncompressed** content. The service
compresses the file with gzip and splits the compressed stream into pieces of at most
1,500,000 bytes, numbered from 0.

### `GET /files/{sha256}`

Asks what the receiver holds. Empty body.

| Answer | Meaning |
|--------|---------|
| `200` `{"state":"complete"}` | the file is stored; nothing to send |
| `200` `{"state":"partial","pieces":[0,1,4]}` | these pieces are stored; send the rest |
| `404` | unknown; send every piece |

### `PUT /files/{sha256}/pieces/{index}`

Body: the piece bytes. `Content-Type: application/octet-stream`.

| Answer | Meaning |
|--------|---------|
| `201` | stored |
| `200` | already stored with the same bytes |
| `409` | already stored with different bytes; the receiver discards every piece it holds for the file, and the service starts the file over |

### `POST /files/{sha256}`

Commits the file once every piece is sent. Body (`application/json`, at most 64 KB):

```json
{
  "kind": "segment",
  "session": "20261031-201500-eden",
  "name": "20261031-201500-eden-1.llr",
  "size": 18734211,
  "encoding": "gzip",
  "encodedSize": 4120934,
  "encodedSha256": "5d0c…a1",
  "pieces": 3
}
```

`kind` is `segment`, `identity` or `stats`. `name` is the file name on the host, for the
receiver's display only. The receiver joins the pieces in order, checks the joined
length and `encodedSha256`, and may also decompress and check `size` and the file
identity.

| Answer | Meaning |
|--------|---------|
| `201` | complete and stored under its identity |
| `200` | was already complete |
| `409` `{"missing":[2]}` | these pieces are missing; the service sends them and commits again |
| `422` | the joined bytes do not match; the receiver discards the file's pieces, and the service sends every piece again |

A stored file never changes. A changed file on the host has a new identity and is a new
file.

## Games

### `POST /games/{session}`

Tells the receiver that a game is over and which files make it up. Sent after every
file it names is committed. Body (`application/json`, at most 64 KB):

```json
{
  "session": "20261031-201500-eden",
  "over": "ended",
  "segments": ["3f2a…e9", "a81c…40"],
  "identity": "c09d…11",
  "stats": "77e2…b8"
}
```

- `over` is `ended` (the recording has its end record), `stopped` (the server stopped
  in the game and later started a different game), or `abandoned` (the server stayed
  stopped longer than the configured wait).
- `segments` lists the segment files in segment order.
- `identity` is the session's identity file, or `null` when no player identity became
  known in the game.
- `stats` is the latest statistics file of the session, or `null` when statistics are
  off.
- The service posts the game again whenever its file set changes: a newer statistics
  file appears for the session, for example an approval the Game Master publishes after
  debriefing, or a game posted as `abandoned` is resumed later and writes another
  segment and a longer identity file. A repeated post replaces the game's record: its
  `over`, `segments`, `identity` and `stats` are those of the latest post.

| Answer | Meaning |
|--------|---------|
| `200` or `201` | recorded; this is the confirmation that starts local retention |
| `409` `{"missing":["a81c…40"]}` | these files are not complete; the service sends them first |

## Errors and retries

| Answer | What the service does |
|--------|-----------------------|
| `400`, `413`, `422` on a commit, `409` on a piece | logs it, sends the whole file again once; if it fails again, keeps the file and reports the failure until it changes |
| `401` | keeps everything and retries with growing waits; an expired timestamp means the host clock is wrong, a bad signature means the secret is wrong |
| `429` with `Retry-After` | waits at least that many seconds |
| `5xx`, timeout, connection error | retries with growing waits from 10 seconds up to 30 minutes, with jitter |

The service deletes nothing on the host that the receiver has not confirmed with a
game answer, and nothing before the retention time has passed after the latest such
answer for that game.

## What the receiver must guarantee to viewers

- Recording segments of an approved game are served as the stored, never-changing
  files, so any number of viewers can be served from a content delivery network
  without work on the website per viewer and without any traffic to the game host.
- Identity files are never served to viewers.
- Nothing about a game is shown before the game is posted as over and approved.
