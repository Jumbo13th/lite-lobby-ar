# ll-uplink

A server owner whose website implements the `ll-upload/1` protocol runs this program after
turning on replay recording: it sends each finished game's recording, identity file and
statistics from the game server's profile to that website.

## Build

```sh
cd uplink
CGO_ENABLED=0 go build -trimpath -o ll-uplink ./cmd/ll-uplink
```

Or take `ll-uplink-linux-amd64` and `ll-uplink-linux-amd64.sha256` from a release:

```sh
sha256sum -c ll-uplink-linux-amd64.sha256
install -m 755 ll-uplink-linux-amd64 /usr/local/bin/ll-uplink
```

## Configure

One JSON file per game server, readable by its owner only (`chmod 600`); the service refuses
to start otherwise. The fields and their rules are in
[the configuration contract](../specs/005-replay-recording/contracts/config-files.md#host-service-the-ll-uplink-configuration-file).
The website provides the server identity and the secret.

## Run

Run one instance per game server, as the user that owns that server's profile folder, since
it deletes confirmed files there. A template unit serves any number of game servers:

```ini
# /etc/systemd/system/ll-uplink@.service
[Unit]
Description=ll-uplink for %i
Wants=network-online.target
After=network-online.target

[Service]
User=gameserver
ExecStart=/usr/local/bin/ll-uplink -config /etc/ll-uplink/%i.json
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```sh
systemctl enable --now ll-uplink@server-1
```

The service logs to standard output, opens no listening socket and connects only to the
configured website. Every 15 seconds it checks the profile; a game is sent once it is over,
that is after its server starts another game, or after it stays stopped longer than
`StoppedWaitMinutes`.

## Metrics

With `MetricsFile` set, every check rewrites that file in the Prometheus text format, for
example in the node exporter's text-file folder. Every series carries
`server="<ServerId>"`.

| Metric | Meaning |
|--------|---------|
| `ll_uplink_last_success_timestamp_seconds` | when the website last accepted a file or a game |
| `ll_uplink_pending_files` | files of finished games the website does not hold yet |
| `ll_uplink_pending_bytes` | their uncompressed size |
| `ll_uplink_failures_total{kind}` | failed attempts since start: `auth` (check the host clock and the secret), `invalid`, `rate_limited`, `transient`, `local` (a file it cannot read or a state it cannot save) |
| `ll_uplink_confirmed_games_total` | games the website confirmed since start |

## Retention

`RetentionDays`, counted from the website's latest confirmation of a game, the service
deletes that game's segment files, identity file and statistics files from the profile, and
nothing else. It never deletes a file the website has not confirmed, and keeps all of a
game's files while any of them waits to be sent.
