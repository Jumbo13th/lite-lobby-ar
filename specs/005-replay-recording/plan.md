# Implementation Plan: Replay Recording

**Branch**: `005-replay-recording` | **Date**: 2026-10-31 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/005-replay-recording/spec.md`

## Summary

A server-only game-mode component, `LL_ReplayRecorderComponent`, records every game
phase into one text file per segment under `$profile:LL_Replays/`. It starts when the
game phase begins and ends at debriefing. It observes only the game's own events and
reads entity state, and it adds nothing to the network except the admin's `/replay`
reply. It is inert until the server owner writes `$profile:LL_Replays.json`.

**What it records:**
- **Movement:** characters and trackers from the game's lists, vehicles from the
  editable-entity core. A 100 ms tick checks a slice of them, so characters are checked
  once a second and vehicles twice. A record is written after a 1 metre move or a 5 degree
  turn.
- **Combat:** deaths and vehicle kills come from the game-mode destroyed hook with the
  damage history; knock-outs from the life-state event with the damage manager's last
  instigator. Shots are summarised per shooter per second from the shot events.
- **Explosions:** the game's explosion-effect event, plus following thrown grenades and
  uncovered rockets until they disappear.
- **Map and roster:** markers by a once-a-second diff of the server's marker lists; map
  areas, manual markers, trigger zones, the roster and the game flow from the addon's own
  objects.

**How it writes:**
- Lines are buffered and appended every 9 seconds, at most 300 lines per tick, the file
  closed each time.
- A full snapshot is written every five minutes, built over consecutive ticks.
- While a hard freeze holds the clock, a wall-clock anchor precedes each new second of
  records.
- A one-line status file tells an outside reader what the server is doing.
- Identity GUIDs go only to a separate identity file.
- A crash-resume continues the same recording in a new segment file through its own
  persistent record.

A Go program in `uplink/`, the host service `ll-uplink`, runs beside each game server. It
never listens. Once a game is over (its end record is written, the server has started a
different game, or it has stayed stopped past a configured wait), it sends the game's
files with the statistics to the receiver, the configured website, over the documented
`ll-upload/1` protocol:
- HMAC-signed requests;
- content-addressed gzip pieces of at most 1.5 MB;
- idempotent commits;
- rate-capped;
- retrying with growing waits.

It deletes local files only after the website confirms them and the retention time has
passed.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.8.0.10 (APIs cited from the
1.8.0.13 dump, unchanged between the two for every file used); Go 1.27 for the host
service (standard library only)

**Primary Dependencies**:
- **Engine:**
  - `SCR_BaseGameModeComponent` (`OnGameStateChanged`, `OnControllableDestroyed`)
  - `SCR_CharacterRegistrationComponent`, `SCR_EditableEntityCore`
  - `SCR_CharacterControllerComponent.m_OnLifeStateChanged`
  - `EventHandlerManagerComponent` (`OnProjectileShot`, `OnGrenadeThrown`)
  - `SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect`
  - `SCR_CompartmentAccessComponent`, `ExtendedDamageManagerComponent.GetDamageHistory`
  - `SCR_MapMarkerManagerComponent`, `PlayerManager`
  - `FileIO`/`FileHandle`
  - the persistence system (`PersistentState`, `ScriptedStateSerializer`)
- **Addon:** `LL_GameModeCoop`, `LL_LobbyManager`, the trigger, map-zone, marker and
  tracker components, `LL_LobbyPlayerComponent`
- **Service:** Go `net/http`, `crypto/hmac`, `crypto/sha256`, `compress/gzip`,
  `encoding/json`, `log/slog`

**Storage**:
- **Game server:** text files in `$profile:LL_Replays/`, plus one record in the
  game's session save.
- **Host service:** a JSON state file in its state folder.

**Testing**:
- **Game:** manual, in the demo world on a dedicated server
  ([quickstart.md](quickstart.md)), spikes S1 to S6 first, and the scale run on the event
  server.
- **Service:** `go test` with a protocol receiver inside the test process, run by CI.

**Target Platform**: dedicated server (Linux) for the recorder; the clients change only
for `/replay`; a Linux game host for the service

**Project Type**: game addon (scripts, one config edit, localization keys) plus a small
host program

**Performance Goals**:
- The recorder averages under 3 ms of work per second and never takes over 2 ms in one
  frame at 127 players and 100 AI.
- No replicated value is added.
- A two-hour game takes at most 60 MB on disk and at most 10 MB as sent.
- The service sends a finished game within 2 minutes, under its rate cap.

**Constraints**:
- No prefab edits by agents, and no change to any weapon or ammunition prefab.
- The engine has no file flush, no rename and no compression.
- REST requests from the game are not used.
- The service is outbound only.
- Upload requests stay at or under 1.5 MB.
- Nothing about a running or still resumable game leaves the host.
- No other project, website or community is named in the repository.

**Scale/Scope**:
- **Scripts:** six new files (five recorder files and one persistence record) and edits
  to eight existing ones; the statistics code is not changed.
- **Config and localization:** one `.conf` edit, six localization keys.
- **Service:** a Go module of five packages with tests, and one CI workflow.
- **Docs:** changelog entries in English and Russian; three Workbench steps for the
  operator.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Every mechanism follows a vanilla pattern (details below the table). Six behaviours cannot be read from the scripts and are spiked first: S1 AI shot events, S2 detonation by disappearance, S3 knock-out instigator timing, S4 vehicle coverage, heading and append creation, S5 key translation on a server, S6 projectile in the delayed callback. | PASS with six spikes |
| II. Ask, Never Assume | The product questions were answered in the spec's clarifications. The two items that returned to the operator are decided (2026-10-31): the FR-004 amendment (R18) and the map-zone rotation finding (R12). Every spike's failure has a planned fallback, and the result is recorded. | PASS |
| III. KISS | One component, one writer, one status line, one persistent record. Direct calls from the addon's existing choke points rather than an event bus. A diff instead of marker hooks. One service process per server, polling, standard library only. | PASS |
| IV. YAGNI | One recorder setting (`Enabled`). The service settings are each required by FR-028, plus the optional metrics path required by FR-036. No sampling knobs, no live view, no in-game viewer, no extra record types beyond the spec. | PASS |
| V. Replication | No replicated value, no `RplSave`/`RplLoad` change, no broadcast. One client-to-server request `RpcAsk_ReplayStatus` (no arguments, admin re-checked on the server) and one owner reply `RpcDo_ReplayStatus` (six values). Visibility: admin only, the requesting player, on demand. Everything else is server-local. | PASS |
| VI. Comments and Hygiene | New types carry `LL_`. Comments are WHY-only: the close-per-flush reason, the five-minute snapshot, the status file's purpose, the deletion-is-not-death rule, the followed-object cap. No other addon or website is named; the contracts are the documents. | PASS |
| VII. Optional Integrations | The recorder is off until `$profile:LL_Replays.json` exists. Unconfigured, it writes no file and no log line, and `/replay` answers "not enabled". The upload protocol is documented in the code that speaks it (the package documentation of `uplink/internal/send`), with the contract as the full reference; the service has no mock, and its tests run their own receiver. | PASS |
| Engine and Asset Constraints | No `.et` edit by an agent. The operator adds the component to the lobby game-mode prefab and places the demo-world test items. One `.conf` edit (`LL_Persistence.conf`), written by the agent with fresh ids and listed for the operator. No global vanilla resource is overridden. | PASS |
| User Interface | No screen. `/replay` replies through the existing chat help line. | PASS |
| Localization | `LL-Replay_*` keys and `LL-Command_HelpReplay` in English and Russian, with the runtime tables regenerated. | PASS |
| Testability | Every requirement maps to a quickstart scenario in the demo world on a dedicated server. The scale criteria (SC-001 to SC-003) are an event-server run. The service has automated tests in CI. | PASS |
| Open Source Standards | No address, identity or secret in the repository; the service reads them from its configuration file. Identity GUIDs never appear in public files. | PASS |

Vanilla patterns behind Principle I:

| Mechanism | Pattern it follows |
|-----------|--------------------|
| Text recorder with a buffered flush | `SCR_GameModeStatistics` |
| Create-or-append | `SCR_FileIOHelper.AppendFileContent` |
| Shot events | `SCR_DataCollectorShootingModule` |
| Kills | `OnControllableDestroyed`, as the addon's statistics already use it |
| Cause of death | the game's `LogKillEvent` damage-history walk |
| Explosions | `SCR_FireSupportRequestedTaskEntity`'s server subscription |
| Seats | the data collector's compartment invokers |
| Vehicle registry | `SCR_DynamicSimulationEditorComponent`'s use of the editable-entity core |
| Persistent record | the addon's own trigger and marker states, which follow `SCR_TaskSystemSerializer` |

## Project Structure

### Documentation (this feature)

```text
specs/005-replay-recording/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── recording-format.md
│   ├── upload-protocol.md
│   ├── config-files.md
│   └── admin-command.md
├── checklists/requirements.md
└── tasks.md              # /speckit-tasks
```

### Source Code (repository root)

```text
Lite Lobby/
├── scripts/game/Replay/
│   ├── LL_ReplayRecorderComponent.c   # config, lifecycle, game-mode hooks, /replay state
│   ├── LL_ReplayWriter.c              # line builders, buffer, flush, status and identity files
│   ├── LL_ReplayEntities.c            # entity table, sampling slices, seats, roster and players
│   ├── LL_ReplayCombat.c              # life states, deaths, shots, explosions, followed objects
│   └── LL_ReplayMap.c                 # markers diff, map areas, manual markers, trigger zones
├── scripts/game/Persistence/LL_ReplayData.c   # continuation record and serializer
├── scripts/game/Core/LL_GameModeCoop.c        # edit: flow hooks, resume hand-over
├── scripts/game/Core/LL_LobbyPlayerComponent.c  # edit: /replay
├── scripts/game/Triggers/LL_TriggerComponent.c, LL_TriggerZoneBase.c,
│   LL_TriggerZoneCapture.c, LL_TriggerZoneContest.c   # edit: recorder calls, outline getter
├── scripts/game/Zones/LL_MapZoneComponent.c   # edit: registry, getters, visibility method
├── scripts/game/Markers/LL_ManualMarker.c     # edit: registry, getters
├── Configs/Systems/Persistence/LL_Persistence.conf   # edit: replay state
└── Language/ll_localization.st                # edit: keys; runtime tables regenerated

uplink/
├── go.mod
├── cmd/ll-uplink/main.go
└── internal/
    ├── config/    # load and validate, refuse a config readable by others
    ├── scan/      # status file, recordings, statistics, the game-over rule
    ├── send/      # protocol package documentation; gzip, identity, pieces, signing, rate cap, retries
    ├── state/     # committed files and confirmed games, atomic writes
    └── metrics/   # Prometheus text file

.github/workflows/uplink.yml
docs/src/content/docs/changelog.mdx and its Russian page   # entry for the shipping version
```

**Structure Decision**: the recorder is one new system folder, `scripts/game/Replay/`,
like `Stats` and `Triggers`. Its persistent record sits with the other records in
`Persistence`. The host service is a separate Go module at the repository root, because
it is built and deployed apart from the addon.

## Design

1. **Lifecycle.** `LL_ReplayRecorderComponent` is a game-mode component on
   `LL_GameMode_Lobby.et`.
   - **Server start:** in `OnPostInit` on the server it reads the config. If recording
     is on, it writes `status.txt` as `idle`, or as `resume` when
     `LL_GameModeCoop.WillResume()`.
   - **Start:** on `OnGameStateChanged(GAME)` with no resume pending it opens segment 1.
   - **End:** on DEBRIEFING it writes `FLOW debrief` and `END debrief`.
   - **Clean stop:** on `OnGameEnd` it writes `STOP shutdown`.
   - **Teardown:** it removes its subscription to the static explosion invoker and the
     editable-core events.
   - **Access:** `GetInstance()` gives the addon's call sites a null-safe way in. Every
     call site does nothing when there is no recording.

2. **Files and writing.** `LL_ReplayWriter` builds every line through a handful of
   field formatters, so the grammar lives in one place.
   - **Strings:** escaped as JSON literals; control characters other than tab and the
     line breaks become spaces.
   - **Buffering:** lines collect in memory. Every 9 seconds the writer appends them
     (create when missing, then append) and closes the file, at most 300 lines per tick;
     a larger buffer continues on the next tick.
   - **Every flush:** it writes a `U` anchor and rewrites `status.txt`.
   - **Held clock:** while a hard freeze holds the mission clock, a `U` anchor goes before
     the first record of every new wall-clock second (FR-015).
   - **Identity file:** pairs are appended with the same flush.
   - **Snapshots:** written at segment start and every 300 seconds of mission time. Each
     is built over consecutive ticks, at most 100 entities per tick; records produced
     meanwhile are held and written after `SNAP-END`.
   - **Failure:** a failed open sets the state to `failed`, logs one line, and stops all
     recording work.
   - **Summary:** every five minutes it logs counts, bytes, the summed tick and flush
     milliseconds with their average per second, the longest flush step and the longest
     tick (R21).

3. **Sampling and roster.** `LL_ReplayEntities` runs the 100 ms tick (R3).
   - **Entities:**
     - characters are walked in tenths of the game's list;
     - vehicles come from the editable-entity core (R5) and trackers from their
       registry, both walked in fifths;
     - new entities get an `E` record and their subscriptions, vanished ones a `Q`;
     - seat changes come from each character's compartment events (R7).
   - **Hard freeze:** the tick skips sampling while a hard freeze holds the clock.
   - **Once per second:**
     - controlled entities are polled for `C`;
     - shot counters are flushed as `F`;
     - markers are diffed;
     - area visibility is evaluated.
   - **Roster:** the lobby manager's invokers produce `S`, `O`, `D` and `N`. A player
     reference is keyed by identity.

4. **Combat.** `LL_ReplayCombat` (R8 to R10):
   - **Life state and knock-outs:** a life-state subscription per character. Knock-outs
     record the damage manager's last instigator, read at once or one frame later
     depending on S3.
   - **Deaths:** the component's `OnControllableDestroyed` override hands over the
     context. Deletions become `Q`. Deaths read the damage history's newest non-trivial
     entry for the cause, and the attacker's last-shot weapon (if within 10 seconds) or
     current weapon.
   - **Shots:** `OnProjectileShot` and `OnGrenadeThrown` are registered on every
     character, with the `OnAmmoCountChanged` fallback if S1 fails. Shots are counted per
     shooter per second; the weapon dictionary grows as weapons are first seen.
   - **Explosions:** the explosion-effect invoker records `B` with the exact point.
   - **Followed objects:** grenades and rockets of launcher weapons that raise no
     explosion event join a list of at most 32. While the list is non-empty, a per-frame
     call reads their positions. A vanished reference ends with a short trace along the
     last velocity and a `B` record.

5. **Map and zones.** `LL_ReplayMap` (R11, R12):
   - **Markers:** a once-a-second diff of the marker manager's two lists, with faction
     flags, icons and colours resolved on the server.
   - **Manual markers and areas:** `LL_ManualMarker` and `LL_MapZoneComponent` gain a
     server registry and public getters. The map zone also gains `IsShownNow()`, a new
     read-only method that evaluates the same phase and timing conditions as its drawing;
     the drawing code is not touched (Principle I).
   - **Trigger zones:** defined at recording start through a new public outline getter
     on `LL_TriggerZoneBase`. Captures and holder changes are recorded beside the
     statistics calls; trigger firings inside `Fire()` after the latch.

6. **Game flow.** `LL_GameModeCoop` calls the recorder when the freeze countdown starts,
   when freeze time ends (each time), on admin freeze changes, and on hard freeze start,
   end and changes. Each call does nothing without a recording.

7. **Session identity.** The recorder resolves its session one frame after its own
   initialisation: `LL_StatsManager.GetInstance().GetSessionId()` when it is not empty,
   otherwise an identity it mints once per loaded world in the same format (R14). The
   statistics code is not changed.

8. **Crash-resume.** `LL_ReplayData` with `LL_ReplaySerializer`, registered in
   `LL_Persistence.conf` (R15).
   - **Saving:** `Serialize` asks the recorder for its continuation and writes nothing
     without a recording. The continuation holds the session, segment, mission time,
     counters and the pairs of persistent id and recording id, identity and player
     reference, and weapon and number.
   - **Restoring:** `Deserialize` keeps the record pending.
     `LL_GameModeCoop.CompleteResume_S` hands it over next to `stats.ResumeRecording_S`.
     The recorder then opens segment `n + 1` with `H resumed` and a snapshot.
   - **Older snapshots:** an unknown version or a missing record means a new recording,
     never a refused resume.

9. **`/replay`.** `LL_LobbyPlayerComponent` registers the command next to `/verify`.
   - **Client:** the client checks admin.
   - **Server:** `RpcAsk_ReplayStatus` re-checks and answers the owner with
     `RpcDo_ReplayStatus(state, session, segment, seconds, kilobytes, lastWriteAgo)`,
     which the client shows as one translated line.
   - **Help:** `/help` gains one line ([contracts/admin-command.md](contracts/admin-command.md)).

10. **The host service.** `ll-uplink -config <file>`:
    - **Startup:** it validates the config (`https`, identity pattern, secret length,
      ranges, owner-only permissions) and loads its state.
    - **Every 15 seconds:**
      - it reads `status.txt`, lists the segments and identity files by session, and
        lists the statistics files;
      - it decides which games are over (R17);
      - for each unconfirmed game it commits every file (skipping committed ones and
        asking the receiver about partial ones), posts the game, and stores the
        confirmation;
      - it re-posts a confirmed game whose newer statistics file appeared;
      - it deletes the local files of games confirmed longer ago than the retention time;
      - it rewrites the metrics file.
    - **Sending:** all bytes pass a token bucket at the cap. A failure backs off from 10
      seconds to 30 minutes with jitter and honours `Retry-After`.
    - **Logging and listening:** it logs with `slog` to standard output, never the secret
      and never a request header, and opens no listening socket.

11. **Build and release.** `.github/workflows/uplink.yml` runs `gofmt -l`, `go vet`,
    `go build` and `go test ./...` for changes under `uplink/`. When the operator
    publishes a release, it builds `CGO_ENABLED=0 GOOS=linux GOARCH=amd64` and attaches
    the binary.

12. **Documentation.**
    - **Changelog:** an entry, in English and Russian, for the Workshop version that
      ships the feature: the component, the config file, `/replay`, the host service.
    - **Protocol in the code:** the package documentation of `uplink/internal/send`
      states the endpoints, headers, signing string, limits and JSON shapes in a few lines
      (Principle VII) and points to the contract.
    - **Full references:** the contracts in this folder. Recorder code points to the
      recording format with one-line comments.
    - **Setup for server owners:** where it lives is the operator's call (see Operator
      steps).

## Operator steps

- **Workbench:** add `LL_ReplayRecorderComponent` to
  `Prefabs/MP/Modes/Editor/LL_GameMode_Lobby.et`; open
  `Configs/Systems/Persistence/LL_Persistence.conf` and confirm the new entries; in the
  demo world, place the test crate, set the NeutralZone hide delay and allow
  unconsciousness ([quickstart.md](quickstart.md)); compile.
- **Game server:** write `$profile:LL_Replays.json`.
- **Game host:** install the `ll-uplink` binary from a release, write one config file
  per game server (owner-only), run it under the service manager. The website provides the
  server identity and secret.
- **Decided 2026-10-31:** the header carries the format version only and FR-004 is
  amended (R18); the server-owner setup text lives in `uplink/README.md`; the map-zone
  rotation finding (R12) is fixed in a separate change after this feature.

## Constitution Check (post-design)

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Unchanged from the pre-design check. Three additions are tied to spikes: the per-frame follow and the refinement trace (S2), the knock-out instigator timing (S3), the shot fallback (S1). Working production code is not restyled: the statistics code is untouched, and the map zone's drawing keeps its own visibility code while a new read-only method mirrors it. | PASS with six spikes |
| II. Ask, Never Assume | The design adds no product decision. The three operator decisions are listed under Operator steps. | PASS |
| III. KISS | Five recorder files split by concern, each with one caller. The persistent record reuses the existing collection. The service has five small packages and no dependency outside the standard library. | PASS |
| IV. YAGNI | Every record type maps to a requirement. The status file exists for FR-029. Followed objects exist for FR-011's rockets and grenades. | PASS |
| V. Replication | Unchanged: one admin request, one owner reply; nothing else. | PASS |
| VI. Comments and Hygiene | Unchanged. The registries added to two existing components follow `LL_StatZoneComponent`'s shape. | PASS |
| VII. Optional Integrations | Unchanged. The service is optional; the recorder works without it. The protocol lives in the service's package documentation. | PASS |
| Engine and Asset Constraints | Unchanged: one prefab step for the operator, one `.conf` edit listed for checking. | PASS |
| Testability | Every spike and requirement is a quickstart step. The service's protocol behaviour is covered by tests that run in CI. | PASS |

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|------|-----------|--------------------------------------|
| A second language (Go) in the repository | FR-028 to FR-038 need a process outside the game that compresses, signs, retries and caps its rate | A shell script with a timer makes state, signing and rate control fragile; sending from the game itself has no compression, a 1 MB request limit, and would put retries in the game |
| Following grenades and some rockets every frame while they fly | FR-011 requires their detonation points and the game reports them through no event | Following them on the 100 ms tick misplaces a rocket by tens of metres |
| Snapshots and flushes spread over consecutive ticks | SC-001 allows at most 2 ms per recorder step, and a full-server snapshot is about a thousand lines | Building or writing it in one frame risks exceeding the step limit at 127 players |
