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
record, a phase gate on the game's own autosave, and a resume path that re-attaches
every restored body to its squad, brings the mission up in GAME under an
admin-released hold, and gives every returning player their slot back through a
resume claim that feeds the existing identity-keyed reconnect flow for alive slots.
Three once-set replicated scalars are added (the snapshot time for the hold text,
the mission-end timer's duration and start reading for the spectator countdown);
nothing else. Two behaviours apply to every mission from now on: time stands still
during any hard freeze, and spectators see the mission-end countdown. A snapshot
that cannot be resumed as a whole shuts the server down with a logged cause and
purges nothing.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.8.0.10 (persistence files verified identical in 1.8.0.13)

**Primary Dependencies**: the game's `PersistenceSystem`/`SCR_PersistenceSystem`, `SaveGameManager`, `ScriptedStateSerializer`, `SaveContext`/`LoadContext`, the persistence config chain (`EditableMission.conf`), `SCR_MapMarkerManagerComponent`, the vehicle controller components, the existing `LL_LobbyManager` reconnect path and hard freeze

**Storage**: the engine's session save for the server profile (binary, engine-managed location); the existing statistics live snapshot in `$profile:LL_GameStats/`

**Testing**: manual, in the demo world on a dedicated server (see [quickstart.md](quickstart.md)); the save cost at 127 players on the live server

**Target Platform**: dedicated server; clients change in the HUD hold text, the hold gates, the aircraft pin mirror, the held clock and the spectator countdown

**Project Type**: game addon; scripts, two config files, localization keys

**Performance Goals**: a save point never disconnects a player; per-save duration logged and under one second at 127 players; three once-set replicated scalars added, nothing else

**Constraints**: server authority for every decision; no `[RplProp]` on collections; no prefab edits by agents; no new admin chat command; the aircraft hold has no vanilla precedent and is spiked first in its phase; a plain restart is fresh (`loadSessionSave: false` in the server configuration) and a resume is the game's own `-loadSessionSave` launch parameter, whose precedence over the config file is confirmed on this server in the first quickstart scenario, with no addon fallback if it is not; the game saves squad membership for AI members only, so the lobby re-attaches bodies itself; no AI or physics pause exists in the script API

**Scale/Scope**: about twelve new script files, edits to twelve existing ones, two `.conf` files, about ten localization keys, two guide pages, three operator steps (Workbench, server configuration, launch line), one deployment procedure recorded in the contract after measurement; two behaviours that apply to every mission, saves on or off: time stands still during any hard freeze, and spectators see the mission-end countdown

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Every mechanism is the game's: state serializers follow `SCR_TaskSystemSerializer`, UUID waits follow `SCR_AIGroupSerializer`, the "after load" hook follows `SCR_ReconnectSerializer` and the native load result comes through a modded `SCR_PersistenceSystem` forwarding its own protected event, the clock hold reuses the game's replicated elapsed time and its per-machine advance, the save guard follows `SCR_CampaignMilitaryBaseComponent`, the shutdown save follows `SCR_PauseMenuUI` and `SCR_BaseGameMode.OnGameEnd`, the vehicle stop follows `SCR_BaseGameMode.OnPlayerDisconnected`, the config chain follows `MissionSystems.conf` and `Mission.conf`, the start-up load is the documented `-loadSessionSave` parameter. One thing has no vanilla precedent and is spiked, not assumed: the airborne pin (research R5). | PASS with one spike |
| II. Ask, Never Assume | Every product question was answered by the operator in the spec, including the four raised by the external review (refusal shuts the server down, a missing statistics file does not refuse, the existing identity rule holds reservations, the launcher supplies the one-start parameter). The engine unknowns are spikes or quickstart gates whose failure returns to the operator with options. | PASS |
| III. KISS | One persistence config, three states, one hold (the existing hard freeze with no countdown), one reconnect flow (the existing possession path, fed by a resume claim that transfers a reservation). Statistics reuse their existing snapshot structs plus the recorder's continuation fields. Marker ownership is not re-linked. Squads are found by the entity name the lobby already requires. | PASS |
| IV. YAGNI | One attribute, read. No save-type selection knob, no per-entity include list, no JSON backend switch, no resume-without-hold option, no addon-side snapshot chooser. | PASS |
| V. Replication | Replicated values added: three scalars on the game mode, all `[RplProp]`, server-authored, each set once and bumped once, visible to every client, no per-frame change: `m_fResumedSnapshotTime` (the hold text), `m_iMissionEndDuration` and `m_fMissionEndStartedAt` (the spectator countdown's inputs; clients derive the remaining time locally). The mission clock reuses the game's own replicated elapsed time, held on every machine during any hard freeze by writing the held value back, through the replicated hard-freeze flag that already exists; the server's periodic clock correction is driven by elapsed time and therefore falls silent during a hold. RPCs added: none. `RplSave`/`RplLoad`: unchanged. Explicit `BumpMe` calls stay authority-only. The hold reuses the two hard-freeze scalars with a new meaning for a negative remaining time. Reservations after a resume reuse the player-name, disconnected-list and slot-assignment replication that already exists, through placeholder ids the server assigns. Saving is server-only. The aircraft pin on the owning client reads the replicated hold flag; nothing is sent for it. | PASS |
| VI. Comments and Hygiene | New files carry the `LL_` prefix; modded serializers live in `scripts/game/Modded/` as `LL_M_<VanillaClass>.c` with vanilla parameter names. WHY comments only on: the placeholder id range, the negative hold value, the resume gate in playable registration, the marker owner decision, the pin's ownership caveat. No other addon is named anywhere. | PASS |
| VII. Optional Integrations | Not an integration: no endpoint, no secret. The feature is off until the mission maker ticks it, and off means no save, no file, no log line. | PASS |
| Engine and Asset Constraints | No `.et` edit by an agent. Two new `.conf` files, hand-authored, listed for the operator; their GUIDs re-read from `.meta` after import. The mission header's world-systems config and the game-mode checkbox are operator steps. No global vanilla resource is overridden: the lobby's config inherits the vanilla chain, it does not replace it. | PASS |
| User Interface | One HUD text change on the existing hard-freeze HUD (resume mode); the existing spectator clock widget shows the mission-end countdown when the mission has that timer (FR-023), no new layout. Chat replies and broadcasts through the existing admin message path. | PASS |
| Localization | New keys `LL-Resume_*` in `ll_localization.st`, EN + RU, runtime tables regenerated. | PASS |
| Testability | Every functional requirement has a quickstart scenario that fails when it is broken (Scenarios 0 to 7); the fault cases (interrupted save, addon change, missing record, missing statistics, hold snapshot, repeat resume) are Scenario 7 in the demo world. SC-001, SC-003 and SC-006 need 127 players and are acknowledged as event-server runs; a failed measurement stays failed until it passes or the operator changes the criterion. | PASS |

**Workbench steps for the operator** (also in [contracts/config-files.md](contracts/config-files.md)):

1. Import the two new `.conf` files, confirm they load, and hand back the GUIDs
   Workbench wrote into their `.meta` files so the references between them are
   re-synced.
2. On `Missions/LobbyDemo.conf`, set the world-systems config field to
   `LL_LobbySystems.conf`; leave the save-type flags at their default.
3. Tick `m_bSessionSaves` on the demo world's game-mode entity for testing; revert
   afterwards.
4. In the dedicated server's `config.json`, the `persistence` block (research R9):
   set `loadSessionSave` to **false** (a plain restart is fresh; the default of true
   would resume on every restart), leave `keepSessionSave` false, set
   `autoSaveInterval` to the wanted cadence (1 for the test server), and on the
   test server point the session storage at the JSON database preset so a save
   point can be read.
5. Nothing on any prefab.
6. After the planned-stop measurement, record the deployed incident procedure in
   the contract: entrypoint, stop signal, profile mount, how the resume parameter is
   added for one start and removed, how a snapshot UUID is found, and the stop
   budget with margin.

## Project Structure

### Documentation (this feature)

```text
specs/004-session-resume/
├── plan.md              # This file
├── research.md          # Phase 0: engine facts and decisions R1 to R8
├── data-model.md        # Phase 1: attributes, the three states, neutralised records
├── quickstart.md        # Phase 1: validation on a dedicated server
├── contracts/
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
    │   ├── LL_GameModeCoop.c                    # attribute, save phase gate + cause warning, roster checks on entering GAME, resume entry + refusal, hold without countdown, clock hold for every hard freeze (EOnFrame), mission-end scalars, vehicle hold, zone reconciliation, debriefing discard, shutdown save, LL_SaveWaiter
    │   ├── LL_VehicleHold.c                     # airborne helicopters pinned for the hold: trace altitude, server ownership, prefab damping restore, forced engine start, give-back at release (T023 outcome)
    │   ├── LL_DamagePause.c                     # running damage effects deactivated for the hold and re-armed at release (T026 outcome)
    │   ├── LL_LobbyManager.c                    # placeholder holder ids, resume seating of reservations, resume claim owning its own transfer (TakeSlot_S untouched), squad frequency restore, synchronous replacement exclusion
    │   ├── LL_LobbyPlayerComponent.c            # client-side aircraft pin mirror (no new chat command); spectator entry re-checks for a living body
    │   └── LL_PlayableComponent.c               # one bounded resume retry: agent, squad re-attach, registration
    ├── Zones/LL_ZoneRestrictionComponent.c      # client: return countdown paused while the hold flag is set
    ├── Map/LL_MapMarkersUI.c                    # client: placement refused while held
    ├── VoN/LL_VoNChannelsManager.c              # placeholders and KIA slots skipped by state assignment
    ├── Persistence/                             # new folder
    │   ├── LL_SessionData.c                     # PersistentState proxy + LL_SessionSerializer
    │   ├── LL_TriggerData.c                     # PersistentState proxy + LL_TriggerSerializer
    │   └── LL_MarkerData.c                      # PersistentState proxy + LL_MarkerSerializer
    ├── Triggers/
    │   ├── LL_TriggerComponent.c                # server-side trigger list, SaveState/LoadState hooks, fired restore, restored flag that skips re-initialisation on first activation
    │   ├── LL_TriggerMissionEndTimer.c          # fires from the mission clock (start reading + duration); reports both to the game mode; started = tick scheduled
    │   ├── LL_TriggerZoneCapture.c              # hook: held, captured
    │   ├── LL_TriggerZoneContest.c              # hook: progress, holder
    │   └── LL_TriggerSupremacy.c                # hook: bothSeen
    ├── Stats/
    │   ├── LL_StatsManager.c                    # recorder continuation embedded in the session record (CaptureContinuation_S / ResumeRecording_S); identities by reconnect key; events on the mission clock
    │   └── LL_StatsTypes.c                      # first-slot holders by identity key; recorder clock and one-time flags in the snapshot struct
    ├── ../../docs/src/content/docs/create-mission/world.mdx      # guide: leave Save Types, set Systems Config (FR-019)
    ├── ../../docs/src/content/docs/ru/create-mission/world.mdx   # same, Russian
    ├── UI/LL_HardFreezeHud.c                    # resume text: "N of M players back"
    ├── UI/Spectator/LL_SpectatorMenu.c          # clock widget: mission-end countdown when the mission has the timer
    └── Modded/
        ├── LL_M_SCR_AIGroup.c                         # new: EOnInit skips member spawn while a save is being applied
        ├── LL_M_SCR_PersistenceSystem.c               # new: forwards the native after-load result to the game mode
        ├── LL_M_SCR_PlayerReconnectDataSerializer.c   # new: writes and reads nothing
        ├── LL_M_SCR_PlayerControllerSerializer.c      # new: writes and reads nothing
        ├── LL_M_SCR_HitZone.c                         # damage gate also checks the hard freeze (the hold)
        ├── LL_M_SCR_MapMarkerSyncComponent.c          # server marker gate also checks the hard freeze
        └── LL_M_VehicleControllerComponent.c          # new: OnBeforeEngineStart refuses during the hold
```

**Structure Decision**: persistence code gets its own `Persistence/` folder because
it is a new system with three files that belong together; everything else edits the
class that already owns the behaviour. The vehicle get-in refusal joins the existing
crew-lock gate rather than a new user action.

## Design

1. **Enable and save** (`LL_GameModeCoop`, FR-001, FR-002, FR-012, FR-016).
   Attribute `m_bSessionSaves` after the freeze block. `OnGameStart`: with the
   switch off, `SetEnabledSaveTypes(0)` and nothing else runs. With it on,
   subscribe to `GetOnBeforeSave`/`GetOnAfterSave` for the duration log and to ask
   the statistics manager for its snapshot first. The cadence is the engine's own
   autosave from the server configuration (research R9); the addon only gates it:
   `SetSavingAllowed(true)` on entering GAME, `SetSavingAllowed(false)` on leaving
   it, so a queued autosave from an earlier phase lands at game start and none
   lands in slot selection, briefing or debriefing. The body-replacement exclusion
   is synchronous: one acquisition before the spawn, one release on each terminal
   exit (spawn failure, new body gone at finish, finish success, finish
   abandoned, hand-off failure), none on a retry; saving is allowed once the
   fresh-start entry has dispatched its whole batch (a flag at its end) and the
   count is zero, checked at the end of the entry and by each release that
   reaches zero, fresh start and resume alike; a replacement asked for
   during a save waits through a one-shot `LL_SaveWaiter`, re-evaluated on the
   manager's busy-state event and on the count reaching zero (research R6). When
   the count first reaches zero in GAME, before saving is allowed, the game mode
   logs every squad without a unique entity name, every registered body without a
   persistence id, and any second or zero-duration mission-end timer (FR-021,
   FR-023). No addon timer and no interval attribute (operator
   decision, 2026-09-18).
2. **Records** (`Persistence/`, FR-003, FR-004). Three `PersistentState` proxies
   and their `ScriptedStateSerializer`s per [data-model.md](data-model.md).
   `LL_SessionSerializer.Serialize` reads `LL_LobbyManager` slots, squads and
   entities, the game mode's pre-hold timers and daylight setting, and the
   statistics session id and file; it returns `DEFAULT` outside GAME so nothing is
   written in other phases. `Deserialize` stores the records, registers a
   `WhenAvailable` task per body and the `ACTIVE` state handler.
   `LL_TriggerSerializer` iterates the trigger list and calls each trigger's hook,
   fired or not. `LL_MarkerSerializer` copies the marker manager's static list.
3. **Neutralised vanilla records** (`Modded/`, FR-008). Two modded serializers
   returning `DEFAULT` on save and true on load.
4. **Resume** (`LL_GameModeCoop`, `LL_LobbyManager`, `LL_PlayableComponent`,
   `LL_M_SCR_AIGroup`, FR-005, FR-006, FR-014, FR-017). World-placed squads do not
   spawn their prefab members while the save is being applied (research R4), so
   the restored bodies are the only bodies. `OnGameStart` with a save active: skip
   SLOTSELECTION, set `m_bResumePending`, set the hard-freeze flag at once so the
   damage gate and input lock cover everything that loads, and subscribe to the
   persistence state change, so a missing lobby record is detected at `ACTIVE`;
   a modded `SCR_PersistenceSystem` forwards the native after-load result, and a
   failure refuses at once on its own. Each body-available task starts one
   bounded operation that reads the deadline the game mode arms at `ACTIVE` (no
   timeout before that): wait for the body's AI agent, re-attach the body to the
   squad entity named in its slot record when it has no group
   (`AddAIEntityToGroup`), mark the playable runtime-spawned, set its sort key,
   register it once its network id is valid, and observe the registered slot;
   done means the manager holds a slot for the body in the saved squad, a
   fallback id is a refusal, and every operation stops on refusal. The `ACTIVE` handler (`FinaliseResume_S`): match every slot
   record to a registered slot by body, refuse on any miss or empty body
   reference; restore each squad's frequency from the squad table; apply KIA and
   lock per slot; allocate placeholder holder ids and seat reservations, reverse
   key map included; set state GAME through a resume entry (`SetGameModeState` +
   the elapsed clock, the held value, the mission-end scalars and the timers
   restored, no body spawning, no fresh recorder start, no freeze restart yet);
   re-assert the stopped day advance; reconcile the freeze zones against the
   restored freeze time; call the vehicle-stop hook; assign voice channels
   (placeholders and KIA slots excluded), then replay the resume claim for every
   player already connected; send the rest to spectator; arm the statistics
   recorder from the saved session id and the snapshot's file; restore markers
   and triggers; clear `m_bResumePending`; allow saving if no replacement is
   pending; log. A refusal (no record, load failure, unknown
   version, empty body reference, missing body or squad, unregistered body or
   fallback id) disables saving and the save types, logs the cause and requests
   the game to close; nothing is purged (research R4).
5. **The hold** (FR-007, FR-022). `StartResumeHold_S` sets the hard-freeze flag
   with remaining -1 and no timer, records the pre-hold hard-freeze remaining and
   daylight setting; the HUD renders the resume text. Every hard freeze, resume
   hold or timed, now holds time (operator decision): the game mode records the
   clock reading when a hard freeze begins and its `EOnFrame` override writes it
   back over the game's elapsed clock every frame while the flag is set, on every
   machine, a proxy first adopting any correction it sees before the base
   advance; the freeze timer, triggers and countdown do not advance; the client
   zone countdown pauses; the hitzone gate zeroes damage for every hitzone; marker
   placement is refused by the client flow and the server gate. Vehicles: the game's pilot-dropped recipe
   on every vehicle, engine start refused by the modded `OnBeforeEngineStart`,
   get-in and get-out refused by the crew-lock gate while held, airborne
   helicopters pinned (research R5) and mirrored on the owning client. The
   existing `/hardfreeze 0` → `EndHardFreeze_S`: unpin, autohover on pinned
   aircraft, log any vehicle that moved more than a metre, restore daylight to the
   pre-hold setting, then either start the saved timed hard freeze (whose own end
   schedules the freeze countdown) or schedule the freeze countdown now if freeze
   time remains; exactly one of the two schedules it, through the one
   schedule/cancel pair every freeze path uses (fresh start, admin adjustment,
   explicit end, expiry); broadcast. The hold text carries the snapshot time
   (`m_fResumedSnapshotTime`, one of three replicated
   addition) and both accepted limits.
6. **No owner controls** (FR-009, FR-010, FR-011, FR-018). DEBRIEFING runs the
   discard (saving disallowed, session storage cleared, the active playthrough
   purged) after any save in flight has finished, the body of the protected
   `HandleOnGameModeEndSaveData`, behind an `LL_SaveWaiter`. `OnGameEnd` in GAME
   follows the game's own exit flow through the same one-shot waiter: when the
   manager is not busy and no replacement is pending, request a blocking
   `SHUTDOWN` save exactly once unless a shutdown-type save has already
   completed since the stop began; on its successful completion disable saving so
   the engine's own exit save cannot write a second; let the exit proceed. One
   stop-time snapshot, whichever path writes it. The quickstart measures the
   stop grace period for idle, busy and replacement-pending stops and checks that
   exactly one stop-time save is written. At game
   start with the checkbox on, the game mode logs one line when the enabled save
   types are zero, when `PersistenceSystem.GetInstance()` is null, and when the
   persistence config lacks the lobby's collection, and logs which snapshot is
   active or that none is, so a misconfiguration never looks like a fresh start.
7. **Statistics in the snapshot** (`LL_StatsManager`, `LL_StatsTypes`, FR-004,
   FR-020, US4; amended 2026-09-19). The session serializer embeds
   `CaptureContinuation_S()` (session, start stamp, winner, one-time flags,
   first-slot holders by identity key, players, events, zones, commanders); on
   resume `ResumeRecording_S(state)` restores it and arms recording exactly once
   (flag, assignment hook, live-write timer) without the participant sweep, the
   commander freeze or the immediate write; every identity resolves through the
   lobby's reconnect keys so absent placeholders keep their credit; events are
   timestamped with the mission clock. No statistics file per snapshot, nothing
   to prune.
8. **Guide** (FR-019). The world chapter's "untick all four Save Types" step becomes
   "leave Save Types, set Systems Config to the lobby's config", EN and RU, with the
   screenshot note updated, plus one line for script-created squads, one for the
   unique entity name every squad needs, one that the freeze time now counts from
   the end of the hard freeze, and one that lobby missions set no game duration.
10. **Spectator countdown** (FR-023). The mission-end timer reports its configured
    seconds to the game mode when it registers and the clock reading when its
    countdown starts; the game mode holds both as once-set replicated scalars; the
    timer itself fires when the mission clock reaches start plus duration, so
    display and firing share one basis; the spectator menu's existing clock widget
    shows the derived remaining time when the duration is non-zero (held at the
    full duration until started, standing still with the clock during any hard
    freeze, clamped to the duration) and the elapsed clock otherwise. Both values
    are in the session record. One positive-duration timer per mission is the
    supported configuration; anything else is logged at the roster check, which
    counts the exact class only: timed announcements inherit the timer, are
    unlimited and keep their own start reading in the trigger record.
9. **Confirmation and spike first** (research R5, R7). Task order starts with the
   config files and the fresh-versus-resume run (Scenario 0, engine records only),
   then the lobby records; the aircraft pin is the first task of the hold phase, on
   a dedicated server with an ownership transfer, before the rest of the hold is
   built on it.

## Constitution Check (post-design)

Re-run after the five external reviews of 2026-09-18 and 2026-09-19 (the fifth: "ready with the listed corrections", all recorded): three
once-set replicated scalars added and declared (principle V), the mission clock
reusing the game's own replicated value with a write-back hold, no prefab touched, two hand-authored configs and one launch
parameter listed for the operator, vanilla patterns named per mechanism, one spike
that returns to the operator on failure, the reviews' operator questions answered
in the spec's second and third clarification sessions. PASS on the documents; the
dedicated-server gates in the quickstart remain open until run.

## Complexity Tracking

No violations to justify.
