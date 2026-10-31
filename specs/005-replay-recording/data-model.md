# Data Model: Replay Recording

Nothing below is replicated. The recorder's state lives on the server; the part that
must survive a crash-resume is one persistent record; the service's state lives on the
host. The on-disk records are specified in
[contracts/recording-format.md](contracts/recording-format.md).

## Recorder configuration

`$profile:LL_Replays.json`, read once at server start. See
[contracts/config-files.md](contracts/config-files.md).

| Field | Type | Default | Read by |
|-------|------|---------|---------|
| `Enabled` | bool | file absent = off | the recorder's start-up check |

## Recorder runtime state (server only, not saved unless listed under the continuation)

| Part | Contents | Notes |
|------|----------|-------|
| Recording | session, segment, state (`off`, `ready`, `resume`, `rec`, `done`, `failed`), failure reason, start Unix time | state drives the status file and `/replay` |
| Writer | pending lines, held lines during a snapshot, bytes written, open path, last flush time, last snapshot time, the clock-held flag and last anchor second, counters by record type, summed and longest tick and flush time | flush every 9 s in chunks of at most 300 lines per tick; snapshot every 300 s of mission time, built over consecutive ticks |
| Entity table | per recorded entity: `eid`, a reference to the entity, kind, side, group, last written `x`, `z`, heading, sampling slot | characters from the game's character list, vehicles from the editable-entity core, trackers from their registry |
| Players | per `pref`: identity, displayed name, connected, controlled `eid` | keyed by identity, so a reconnect keeps the reference |
| Weapons | per `wid`: weapon type, name | written once as `W`, restated in snapshots |
| Groups | per `gid`: side, callsign | the game's group id |
| Shot counters | per shooter `eid`: count in the current second, first heading, `wid` | flushed as `F` once per second |
| Followed objects | at most 32: reference, last position, last velocity, kind, shooter `eid`, `wid` | grenades and rockets without an explosion event |
| Markers seen | the set of marker ids of the previous read | diffed once per second |
| Areas | per `aid`: the area component, last shown state | visibility evaluated once per second |
| Zones | per `zid`: the trigger zone | defined at recording start |

### Entity kinds

| Kind | Rule |
|------|------|
| `man` | a character |
| `heli` | vehicle AI usage type helicopter |
| `plane` | vehicle AI usage type plane |
| `mortar` | AI usage type static artillery |
| `static` | AI usage type static weapon, or root class `Turret` |
| `tracked` | AI usage type tracked, or a tracked simulation on an unclassified vehicle |
| `apc` | wheeled, vehicle type APC |
| `truck` | wheeled, vehicle type truck, fuel, command, supply, repair or arsenal truck |
| `car` | wheeled, vehicle type car |
| `vehicle` | any other vehicle, boats included (the base game has none) |
| `tracker` | a mission-maker tracker |

### Relations (from the game's death relation flags)

| Relation | Game value |
|----------|------------|
| `enemy` | killed by an enemy player or enemy AI |
| `friendly` | killed by a friendly player or friendly AI |
| `suicide` | suicide |
| `editor` | killed by an unlimited Game Master |
| `vehicle` | the victim is not a character |
| `none` | no killer is known |

Deletions (deleted, deleted by the editor) are recorded as `Q`, never as `K`.

### Seat roles

| Role | Compartment type |
|------|------------------|
| `driver` | `PILOT` |
| `gunner` | `TURRET` (a commander seat is a turret; its seat name tells it apart) |
| `cargo` | `CARGO` |

### Weapon types

`EWeaponType` in lower case without its prefix: `rifle`, `grenadelauncher`,
`sniperrifle`, `rocketlauncher`, `machinegun`, `handgun`, `fraggrenade`,
`smokegrenade`, `autocannon`; anything else is `other`.

## Continuation record (persistent, `LL_ReplayData`)

Written into the game's session save by `LL_ReplaySerializer`; read back on resume. A
missing record (an older snapshot, or recording off) means the resumed game starts a new
recording.

| Field | Type | Meaning |
|-------|------|---------|
| `version` | int | `1`; an unknown version logs a warning and the resumed game starts a new recording |
| `session` | string | the recording's session identity |
| `segment` | int | the segment that was open at the snapshot |
| `snapshotT` | int | mission time `t` at the snapshot |
| `nextEid`, `nextPref`, `nextWid` | int | the next free numbers |
| `entities` | array of {persistent id, `eid`} | so a restored character or vehicle keeps its recording id |
| `players` | array of {identity, `pref`} | so a returning player keeps their reference |
| `weapons` | array of {name, type, `wid`} | so weapon numbers stay stable |

State transitions of a recording:

```text
off ──config present──▶ ready ──GAME──▶ rec ──DEBRIEFING──▶ done
                         │               │
                         │               ├──write fails──▶ failed
                         │               └──clean stop──▶ (segment gets STOP; session save may resume it)
                         └──resume pending──▶ resume ──resume completes──▶ rec (segment n+1)
```

## Status file

One line in `$profile:LL_Replays/status.txt`, rewritten at server start and with every
flush; format in the recording contract. It carries the recording state above, mapped
to `idle` (`ready`), `resume`, `rec` and `done`; `failed` is written as `idle`.

## Service configuration

See [contracts/config-files.md](contracts/config-files.md): `Website`, `ServerId`,
`Secret`, `Profile`, `StateDir`, `RateCapKbit`, `StoppedWaitMinutes`, `RetentionDays`,
`MetricsFile`.

## Service state (`StateDir/state.json`, written through a temporary file)

| Part | Contents |
|------|----------|
| Files | per file identity: kind, session, file name, committed time |
| Games | per session: the set last posted (how it ended: `ended`, `stopped` or `abandoned`; segment identities; identity-file identity; statistics identity), the latest confirmation time, failure count and last error |

Lifecycle of a game on the host:

```text
running ──over (end record │ other session │ wait elapsed)──▶ over
over ──every file committed──▶ files sent ──game posted, 2xx──▶ confirmed
confirmed ──file set changed (newer statistics, or a segment after a resume)──▶ (new files committed, game posted again)
confirmed ──retention elapsed since the latest confirmation──▶ local files deleted
```

A game is never `over` while the status file says `resume`, or says `rec` for that
session while the status or the game's files were written within the wait; a `rec`
status that has gone stale with them is how a killed server looks, and that game ends
as `abandoned`.
