# Contract: configuration

## Recorder: `$profile:LL_Replays.json`

The server owner writes this file to turn recording on. It is read once at server start.

```json
{ "Enabled": true }
```

| Field | Type | Meaning |
|-------|------|---------|
| `Enabled` | bool | `true` records every game on this server |

- File absent, or `Enabled` false: the recorder does nothing. It writes no file, no
  log line, no status file, and `/replay` answers that recording is not enabled. Unlike
  the statistics and the player check, the recorder never writes a template file.
- File present but unreadable: one warning line names the file; the recorder stays off.
- Recordings go to `$profile:LL_Replays/` (see
  [recording-format.md](recording-format.md)). The recorder creates the folder and never
  deletes anything in it.

## Session persistence: `Configs/Systems/Persistence/LL_Persistence.conf`

Edited by the agent, opened and checked by the operator in Workbench. One persistent
state is added next to the trigger and marker states, in the same shape:

- in the `Lobby` configuration group, a `StatePersistenceConfig "{<fresh id>}"` with
  `SelfDelete 0`, `Collection "{43DB609DE0025271}"` (the existing `LL_Lobby` collection)
  and `Serializer LL_ReplaySerializer "{<fresh id>}" {}`;
- in `PersistentStates +{ … }`, `LL_ReplayData "{<fresh id>}" {}`.

Nothing else in the file changes. A snapshot taken before this change has no replay
record; resuming it continues the game with a new recording, as a plain start would.

## Host service: the `ll-uplink` configuration file

One file per game server, passed with `-config`. The service refuses to start when the
file can be read by anyone but its owner, because it holds the secret.

```json
{
  "Website": "https://example.org/api/replays/v1",
  "ServerId": "server-1",
  "Secret": "<at least 32 random characters, shared with the website>",
  "Profile": "/srv/arma/server-1/profile",
  "StateDir": "/var/lib/ll-uplink/server-1",
  "RateCapKbit": 8000,
  "StoppedWaitMinutes": 30,
  "RetentionDays": 14,
  "MetricsFile": "/var/lib/node_exporter/ll_uplink_server-1.prom"
}
```

| Field | Required | Rule | Meaning |
|-------|----------|------|---------|
| `Website` | yes | `https://` | base address of the receiving website's upload protocol |
| `ServerId` | yes | 1 to 64 of `a-z`, `0-9`, `-` | the identity the website knows this server by |
| `Secret` | yes | at least 32 characters | the key the website shares with this server |
| `Profile` | yes | an existing folder | the game server's profile folder (the one `$profile:` points to) |
| `StateDir` | yes | writable | where the service keeps what the website has confirmed |
| `RateCapKbit` | no, 8000 | 100 to 1,000,000 | the most it sends per second, in kilobits |
| `StoppedWaitMinutes` | no, 30 | 5 to 1,440 | how long a game cut off by a stop waits for a resume before it is sent |
| `RetentionDays` | no, 14 | 1 to 365 | how long confirmed files stay on the host |
| `MetricsFile` | no | a path | where the Prometheus text-format metrics are written; none when absent |

The service reads `LL_Replays/` and `LL_GameStats/` under `Profile`, writes only under
`StateDir` and to `MetricsFile`, and deletes only files the website confirmed once the
retention time has passed.

### Running it

One process per game server, kept running by the host's service manager:

```text
ll-uplink -config /etc/ll-uplink/server-1.json
```

It logs to standard output without secrets, opens no listening socket, and connects
only to `Website`.
