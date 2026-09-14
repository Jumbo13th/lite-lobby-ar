# Implementation Plan: Session Resume After a Server Crash

**Branch**: `004-session-resume` | **Date**: 2026-09-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/004-session-resume/spec.md`

## Summary

Build on the game's own session-save system (the persistence world system, the
save-game manager and their script serializers) instead of writing a save format.
The lobby ships one persistence config that inherits the game's editable-mission
chain, so characters, vehicles, items, seating, damage, ruins, doors, AI groups,
Game Master placements and weather are saved by the game itself. The lobby adds
three persistent states of its own (session and slots, triggers, map markers), two
modded serializers that neutralise the game's reconnect cleanup and player-controller
record, a periodic save timer with admin commands, and a resume path that brings the
mission up in GAME under an admin-released hold and lets the existing
identity-keyed reconnect flow put every player back into their own body. Nothing new
is replicated.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.8.0.10 (persistence files verified identical in 1.8.0.13)

**Primary Dependencies**: the game's `PersistenceSystem`/`SCR_PersistenceSystem`, `SaveGameManager`, `ScriptedStateSerializer`, `SaveContext`/`LoadContext`, the persistence config chain (`EditableMission.conf`), `SCR_MapMarkerManagerComponent`, the vehicle controller components, the existing `LL_LobbyManager` reconnect path and hard freeze

**Storage**: the engine's session save for the server profile (binary, engine-managed location); the existing statistics live snapshot in `$profile:LL_GameStats/`

**Testing**: manual, in the demo world on a dedicated server (see [quickstart.md](quickstart.md)); the save cost at 127 players on the live server

**Target Platform**: dedicated server; clients change only in the HUD hold text, the hold gates and the aircraft pin mirror

**Project Type**: game addon; scripts, two config files, localization keys

**Performance Goals**: a save point never disconnects a player; per-save duration logged and under one second at 127 players; no added replication

**Constraints**: server authority for every decision; no `[RplProp]` on collections; no prefab edits by agents; the aircraft hold has no vanilla precedent and is spiked first; how the dedicated server applies the save at start is engine-side and spiked first

**Scale/Scope**: about ten new script files, edits to six existing ones, two `.conf` files, about twenty localization keys, two Workbench steps for the operator

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Every mechanism is the game's: state serializers follow `SCR_TaskSystemSerializer`, UUID waits follow `SCR_AIGroupSerializer`, the "after load" hook follows `SCR_ReconnectSerializer`, the save guard follows `SCR_CampaignMilitaryBaseComponent`, the shutdown save follows `SCR_PauseMenuUI`, the vehicle stop follows `SCR_BaseGameMode.OnPlayerDisconnected`, the config chain follows `MissionSystems.conf` and `Mission.conf`. Two things have no vanilla precedent and are spiked, not assumed: the airborne pin and the dedicated-server load at start (research R5, R7). | PASS with two spikes |
| II. Ask, Never Assume | The three product questions were answered by the operator in the spec. The two engine unknowns are spikes whose failure returns to the operator with options, as the spec requires for the aircraft. | PASS |
| III. KISS | One persistence config, three states, one hold (the existing hard freeze with no countdown), one reconnect path (the existing one, fed placeholder holder ids). Statistics reuse their existing snapshot. Marker ownership is not re-linked. | PASS |
| IV. YAGNI | Two attributes, both read. No save-type selection knob, no per-entity include list, no JSON backend switch, no resume-without-hold option. | PASS |
| V. Replication | Replicated values added: none. RPCs added: none. `RplSave`/`RplLoad`: unchanged. The hold reuses the two hard-freeze scalars with a new meaning for a negative remaining time. Reservations after a resume reuse the player-name, disconnected-list and slot-assignment replication that already exists, through placeholder ids the server assigns. Saving is server-only. The aircraft pin on the owning client reads the replicated hold flag; nothing is sent for it. | PASS |
| VI. Comments and Hygiene | New files carry the `LL_` prefix; modded serializers live in `scripts/game/Modded/` as `LL_M_<VanillaClass>.c` with vanilla parameter names. WHY comments only on: the placeholder id range, the negative hold value, the resume gate in playable registration, the marker owner decision, the pin's ownership caveat. No other addon is named anywhere. | PASS |
| VII. Optional Integrations | Not an integration: no endpoint, no secret. The feature is off until the mission maker ticks it, and off means no save, no file, no log line. | PASS |
| Engine and Asset Constraints | No `.et` edit by an agent. Two new `.conf` files, hand-authored, listed for the operator; their GUIDs re-read from `.meta` after import. The mission header's world-systems config and the game-mode checkbox are operator steps. No global vanilla resource is overridden: the lobby's config inherits the vanilla chain, it does not replace it. | PASS |
| User Interface | One HUD text change on the existing hard-freeze HUD (resume mode). Chat replies and broadcasts through the existing admin message path. | PASS |
| Localization | New keys `LL-Resume_*` in `ll_localization.st`, EN + RU, runtime tables regenerated. | PASS |
| Testability | Reproducible in the demo world on a dedicated server with two clients; the scale measurement needs the live server, as the spec's SC-003 states. | PASS |

**Workbench steps for the operator** (also in [contracts/config-files.md](contracts/config-files.md)):

1. Import the two new `.conf` files, confirm they load, and hand back the GUIDs
   Workbench wrote into their `.meta` files so the references between them are
   re-synced.
2. On `Missions/LobbyDemo.conf`, set the world-systems config field to
   `LL_LobbySystems.conf`; leave the save-type flags at their default.
3. Tick `m_bSessionSaves` on the demo world's game-mode entity for testing and set
   the interval to 1 minute; revert afterwards.
4. Nothing on any prefab.

## Project Structure

### Documentation (this feature)

```text
specs/004-session-resume/
├── plan.md              # This file
├── research.md          # Phase 0: engine facts and decisions R1 to R8
├── data-model.md        # Phase 1: attributes, the three states, neutralised records
├── quickstart.md        # Phase 1: validation on a dedicated server
├── contracts/
│   ├── chat-commands.md # /save /resume /discard /shutdown
│   └── config-files.md  # the two .conf files and the mission header
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
Lite Lobby/
├── Configs/Systems/
│   ├── LL_LobbySystems.conf                     # new: systems config pointing at the lobby persistence config
│   └── Persistence/LL_Persistence.conf          # new: inherits EditableMission.conf, adds LL_Lobby collection + 3 states
├── Language/ll_localization.st                  # new LL-Resume_* keys (EN + RU), tables regenerated
└── scripts/game/
    ├── Core/
    │   ├── LL_GameModeCoop.c                    # attributes, save timer, resume entry, hold without countdown, vehicle hold, discard/shutdown
    │   ├── LL_LobbyManager.c                    # placeholder holder ids, resume seating of reservations, KIA/lock apply
    │   ├── LL_LobbyPlayerComponent.c            # /save /resume /discard /shutdown, /help entries, client-side aircraft pin mirror
    │   └── LL_PlayableComponent.c               # registration retry while a resume is pending
    ├── Persistence/                             # new folder
    │   ├── LL_SessionData.c                     # PersistentState proxy + LL_SessionSerializer
    │   ├── LL_TriggerData.c                     # PersistentState proxy + LL_TriggerSerializer
    │   └── LL_MarkerData.c                      # PersistentState proxy + LL_MarkerSerializer
    ├── Triggers/
    │   ├── LL_TriggerComponent.c                # server-side trigger list, SaveState/LoadState hooks, fired restore
    │   ├── LL_TriggerMissionEndTimer.c          # hook: secondsLeft
    │   ├── LL_TriggerZoneCapture.c              # hook: held, captured
    │   ├── LL_TriggerZoneContest.c              # hook: progress
    │   └── LL_TriggerSupremacy.c                # hook: bothSeen
    ├── Stats/LL_StatsManager.c                  # reload the live snapshot for a saved session id; snapshot on OnBeforeSave
    ├── UI/LL_HardFreezeHud.c                    # resume text: "N of M players back"
    └── Modded/
        ├── LL_M_SCR_PlayerReconnectDataSerializer.c   # new: writes and reads nothing
        ├── LL_M_SCR_PlayerControllerSerializer.c      # new: writes and reads nothing
        ├── LL_M_SCR_HitZone.c                         # damage gate also checks the hard freeze (the hold)
        └── LL_M_VehicleControllerComponent.c          # new: OnBeforeEngineStart refuses during the hold
```

**Structure Decision**: persistence code gets its own `Persistence/` folder because
it is a new system with three files that belong together; everything else edits the
class that already owns the behaviour. The vehicle get-in refusal joins the existing
crew-lock gate rather than a new user action.

## Design

1. **Enable and save** (`LL_GameModeCoop`, FR-001, FR-002, FR-012, FR-016).
   Attributes `m_bSessionSaves` and `m_iSaveIntervalMinutes` after the freeze
   block. `OnGameStart`: with the switch off, `SetEnabledSaveTypes(0)` and nothing
   else runs. With it on, subscribe to `GetOnBeforeSave`/`GetOnAfterSave` for the
   duration log and to ask the statistics manager for its snapshot first. Entering
   GAME (fresh or resumed) starts a `CallLater` at the interval that calls
   `RequestSavePoint(AUTO)` when in GAME, not held, not busy, not loading.
2. **Records** (`Persistence/`, FR-003, FR-004). Three `PersistentState` proxies
   and their `ScriptedStateSerializer`s per [data-model.md](data-model.md).
   `LL_SessionSerializer.Serialize` reads `LL_LobbyManager` slots and entities,
   the game mode's timers and the statistics session id; it returns `DEFAULT`
   outside GAME so nothing is written in other phases. `Deserialize` stores the
   records, registers a `WhenAvailable` task per body and the `ACTIVE` state
   handler. `LL_TriggerSerializer` iterates the trigger list and calls each
   trigger's hook. `LL_MarkerSerializer` copies the marker manager's static list.
3. **Neutralised vanilla records** (`Modded/`, FR-008). Two modded serializers
   returning `DEFAULT` on save and true on load.
4. **Resume** (`LL_GameModeCoop`, `LL_LobbyManager`, `LL_PlayableComponent`,
   FR-005, FR-006). `OnGameStart` skips SLOTSELECTION when a save is active and
   sets `m_bResumePending`. Body-available tasks mark the playable runtime-spawned
   and set its sort key; registration retries while a resume is pending and the
   group is not yet linked. The `ACTIVE` handler: apply KIA and lock per slot,
   allocate placeholder holder ids and seat reservations, set state GAME through a
   resume entry (`SetGameModeState` + timers restored, no body spawning, no freeze
   restart yet), engage the hold, stop day advance, reload statistics, restore
   markers and triggers, then clear `m_bResumePending` and log. A version mismatch
   or zero applied slots refuses the resume: log, purge, start fresh (FR-014).
5. **The hold** (FR-007). `StartResumeHold_S` sets the hard-freeze flag with
   remaining -1 and no timer; the HUD renders the resume text; the freeze timer,
   triggers and countdown do not advance; the hitzone gate zeroes damage. Vehicles:
   the vanilla stop recipe on every tracked vehicle, engine start refused by the
   modded `OnBeforeEngineStart`, get-in refused by the crew-lock gate while held,
   airborne helicopters pinned (research R5) and mirrored on the owning client.
   `/resume` → `EndHardFreeze_S`, unpin, autohover on pinned aircraft, freeze timer
   resumed if time remains, broadcast.
6. **Owner controls** (`LL_LobbyPlayerComponent`, `LL_GameModeCoop`, FR-009,
   FR-010, FR-011). Commands per [contracts/chat-commands.md](contracts/chat-commands.md).
   DEBRIEFING and `/discard confirm` run the discard: saving disallowed, session
   storage cleared, playthrough purged, timer stopped.
7. **Spikes first** (research R5, R7). Task order starts with the config files
   and the engine-load check, then the aircraft pin on a dedicated server with an
   ownership transfer, before any of the remaining work is built on them.

## Constitution Check (post-design)

Unchanged from the gate: no replication added, no prefab touched, two hand-authored
configs listed for the operator, vanilla patterns named per mechanism, two spikes
that return to the operator on failure. PASS.

## Complexity Tracking

No violations to justify.
