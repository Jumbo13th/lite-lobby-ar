// Package send speaks ll-upload/1, the protocol that hands finished games to the receiving
// website. The service is the only client, sends one request at a time, never listens, and
// counts request bodies against its rate cap.
//
// Every request carries four headers:
//
//	X-LL-Server          the server identity: 1 to 64 of a-z, 0-9 and -
//	X-LL-Timestamp       the request time in Unix seconds
//	X-LL-Content-SHA256  lowercase hex SHA-256 of the body, of the empty string for none
//	X-LL-Signature       lowercase hex HMAC-SHA256, keyed with the server's secret, of
//	                     method LF path LF timestamp LF content hash (no final LF)
//
// The signed path is relative to the configured base address and includes any query, for
// example /files/{sha256}/pieces/0. From the headers alone, before reading the body, the
// receiver checks that the server is known, that the timestamp is within 300 seconds of its
// clock, that a PUT or POST carries a Content-Length within the endpoint's limit (a GET has
// no body and usually no Content-Length), and the signature in constant time; it answers
// 401, or 413 for the length. A body whose hash differs is answered 400.
//
// A file is a recording segment, an identity file or a statistics file. Its identity is the
// SHA-256 of its uncompressed content. It is sent gzip-compressed, split into pieces of at
// most 1,500,000 bytes numbered from 0. A stored file never changes; changed content is a
// new file.
//
//	GET  /files/{sha256}                 200 {"state":"complete"}, 200 {"state":"partial","pieces":[0,1]}, 404 unknown
//	PUT  /files/{sha256}/pieces/{index}  201 stored, 200 same bytes held, 409 other bytes held
//	POST /files/{sha256}                 201 stored, 200 already complete, 409 {"missing":[2]}, 422 mismatch
//	POST /games/{session}                200 or 201 confirmed, 409 {"missing":["<sha256>"]}
//
// A commit body is {"kind","session","name","size","encoding":"gzip","encodedSize",
// "encodedSha256","pieces"}; kind is segment, identity or stats. The receiver joins the
// pieces and checks encodedSize and encodedSha256. A game body is {"session","over",
// "segments","identity","stats"}: over is ended, stopped or abandoned, segments are in
// order, and identity and stats are null when absent. A game is posted once all its files
// are committed, and again whenever its file set changes: a newer statistics file, or a
// segment written after a resume that followed an abandoned post. A repeated post replaces
// the game's record. Commit and game bodies are at most 64 KB. Local retention counts from
// the latest 2xx game answer.
//
// The service sends every piece again once after a 400, 413 or 422 on a commit or a 409 on
// a piece, waits at least Retry-After after a 429, and retries 401, 5xx and network errors
// with waits growing from 10 seconds to 30 minutes.
//
// The full reference is specs/005-replay-recording/contracts/upload-protocol.md.
package send
