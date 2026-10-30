# Implementation Plan: Session Resume After a Server Crash

**Branch**: `004-session-resume` | **Date**: 2026-09-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/004-session-resume/spec.md`

**Amended**: 2026-10-11, random squad deployment (spec User Story 5, FR-024 to FR-035): Design item 11, research R10, quickstart Scenario 8, the amendment tables below.

**Amended**: 2026-10-21, timed particle effects and timed map zones (spec User Stories 6 and 7, FR-036 to FR-046): Design items 12 and 13, research R11 and R12, quickstart Scenarios 9 and 10, the amendment tables below.

**Amended**: 2026-10-21, random deployment extended (spec User Story 5, FR-028 revised, FR-047 to FR-049): Design item 11 (last bullets), quickstart Scenario 8 steps 11 and 12, the amendment table below.

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

The amendment of 2026-10-11 adds random squad deployment, which shares nothing with
the above but two hooks and one saved flag. A world entity per deploying squad names
the squad and its spawn points; the server draws one point and places the whole
squad around it, at the start of the game phase or when freeze time ends, on the
ground or at a height. Bodies nobody controls are moved by the game's own entity
move; a player's body is moved on that player's machine, as the game's own player
teleport does, through one owner message. At the start of the game phase no body is
moved under a player at all: the fresh body every slotted player already receives
is created at the point. No replicated value is added.

The amendment of 2026-10-21 adds timed particle effects. A mission maker places a
particle effect with the addon's timing component, which keeps it out until the end
of freeze time or the start of the game phase and stops it after a set number of
seconds on the mission clock. Every machine decides on its own from state it already
receives, so nothing is added to the network. The addon ships one continuous red
signal smoke made from the game's red smoke grenade.
The same amendment gives a map zone a hide delay counted from the end of freeze.
Both features take the end of freeze from one reading on the game mode: a mission-end
timer's replicated start when the mission has one, otherwise the moment the machine
saw the countdown end, otherwise the configured freeze length.

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

**Amendment 2026-10-11 (random squad deployment)**: one new script file, edits to five existing ones, two new prefabs, the demo-world placement by the operator; no config file, no layout, no localization key, no guide page. Dependencies: `SCR_EditableEntityComponent.SetTransform`, `SCR_Global.TeleportPlayer`, `CompartmentAccessComponent.GetOutVehicle`, `SCR_WorldTools.FindEmptyTerrainPosition`, `SCR_Math2D.GenerateRandomPointInRadius`, the existing body replacement in `LL_LobbyManager`. One owner message and one saved bool are added. Tested in the demo world on a dedicated server (quickstart Scenario 8); the landing under a parachute is checked on a mission that has one, since the game and the demo world have none.

**Amendment 2026-10-21 (timed particle effects)**: one new script file, one particle file derived from the game's red smoke grenade particle, the demo-world placement by the operator; no prefab, no config file, no layout, no localization key, no guide page. Dependencies: `ParticleEffectEntity.Play`, `Stop` and `StopEmission`, `SCR_BaseGameMode.GetElapsedTime`, `LL_GameModeCoop.GetFreezeTimeRemaining` and `GetFreezeTimeDuration`. Nothing is added to the network. Tested in the demo world on a dedicated server (quickstart Scenario 9). The second part edits `LL_GameModeCoop` (the freeze-end reading, an `onRplName` callback on the existing freeze countdown) and `LL_MapZoneComponent` (one attribute); quickstart Scenario 10.

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

Amendment 2026-10-11, random squad deployment (Design item 11):

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Every step is the game's: a body nobody controls is moved by `SCR_EditableEntityComponent.SetTransform` (the Game Master's entity move, which teleports where the body is owned and informs the replication scheduler); a player's body is moved on the owner's machine by `SCR_Global.TeleportPlayer`, reached by an owner message exactly as `SCR_PlayersManagerEditorComponent` reaches it; a seated player leaves the seat first with `GetOutVehicle(EGetOutType.TELEPORT, …)` and a retry, the sequence of `SCR_TutorialGamemodeComponent`'s fast travel, because the teleport would otherwise move the vehicle; a seated body without a player is taken out as `SCR_DisembarkContextAction` does; positions come from `SCR_Math2D.GenerateRandomPointInRadius` and `SCR_WorldTools.FindEmptyTerrainPosition`; a position above the surface passes `SCR_Global.IsPositionWithinTerrainBounds`. The fresh body at the start of the game phase is the addon's existing replacement with a different spawn transform. Nothing is spiked. | PASS |
| II. Ask, Never Assume | Eight product answers from the operator are in the spec's session of 2026-10-11. The linking idiom was decided on the operator's instruction to follow the addon's own practice: every existing link is an entity name (R10). | PASS |
| III. KISS | One component on a world entity, one static list, one owner message, one saved flag. No manager component, no moment enum (a checkbox), no spawn-point class or component of the addon's own (the prefab to place is the game's `SCR_Position`), no second body-replacement path. | PASS |
| IV. YAGNI | Five attributes, each read. No random position inside an area, no per-point settings, no announcement, no parachute, no guide page, no admin command. | PASS |
| V. Replication | Replicated values added: none. RPCs added: one, `RpcDo_OwnerDeploy(vector)` on `LL_LobbyPlayerComponent`, server to the moved player only, once per end-of-freeze deployment, one vector; visibility: that player. `RplSave`/`RplLoad`: unchanged. The spawn-marker broadcast carries fewer entries for a mission with an end-of-freeze deployment (the squad is left out on the server) and is not sent a second time. The draw is server-only; the component does nothing on a client. | PASS |
| VI. Comments and Hygiene | `LL_RandomDeploymentComponent`, `LL_` prefix. WHY comments on: the pending spawn transform (a freeze zone counts a body that leaves), the once-per-game flag (a revived freeze), the owner-side execution, the draw before the state change. No other addon is named; the parachute is "the mission's". | PASS |
| VII. Optional Integrations | Not an integration. A mission without the entity is unchanged. | PASS |
| Engine and Asset Constraints | Two new prefabs: `LL_RandomDeployment.et` (a generic entity with the component, like the trigger prefabs) and `LL_SpawnPoint.et` (the game's `SCR_Position` with nothing on it, asked for by the operator so a mission maker has a spawn point to place, as the addon ships a flag pole for zone flags). The operator waived the Workbench-only rule for these two files on 2026-10-11, so the agent wrote them and their `.meta` files by hand with fresh ids; the operator opens them in Workbench and confirms the GUIDs. No existing `.et` is touched and no script references the new GUIDs. Demo-world entities placed by the operator. | PASS on the operator's waiver |
| User Interface | None. | PASS |
| Localization | None: no user-visible string; attribute descriptions are not localized. | PASS |
| Testability | Scenario 8 runs in the demo world on a dedicated server and fails on every broken requirement. One check cannot run there: the landing under a parachute (SC-011's last clause), because neither the game nor the demo world has one; it runs on a mission that equips it. The air placement itself (who is in the air, at what height, who is on the ground) is checked in the demo world. | PASS with one check outside the demo world |

Amendment 2026-10-21, timed particle effects (Design item 12):

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | The effect is the game's placed ambient particle, `ParticleEffectEntity` (the class of the game's placed smokestacks), driven by its own `Play`, `Stop` and `StopEmission`. Time is the game's replicated elapsed time with the addon's freeze countdown, read as `LL_TriggerMissionEndTimer` reads them. The particle is the game's red smoke grenade with its emission envelope removed, in the form of the game's continuous smokestack particle. Nothing is spiked. | PASS |
| II. Ask, Never Assume | Four answers from the operator in the spec's session of 2026-10-21: the branch, the start moment as a setting, the late joiner's remaining time without network cost, the one red particle. | PASS |
| III. KISS | One component, a once-a-second tick on the call queue, two attributes; no manager, no registry, no prefab. | PASS |
| IV. YAGNI | Two attributes, both read. No start delay, no colour beyond red, no sound, no burn-until-the-end option. | PASS |
| V. Replication | Replicated values added: none. RPCs added: none. `RplSave`/`RplLoad`: unchanged. Local-only: each machine starts and stops its own copy from the game state, the freeze countdown and the mission clock it already receives; a dedicated server skips the component. | PASS |
| VI. Comments and Hygiene | `LL_` prefix on the script and the particle. WHY comments on: the first tick's stop (the game plays a placed particle on load), the late joiner's start reading, the emission stop. No other addon is named. | PASS |
| VII. Optional Integrations | Not an integration. A mission without the component is unchanged. | PASS |
| Engine and Asset Constraints | No `.et` is written. One `.ptc`, derived from a game particle, registered through Workbench so that Workbench wrote its `.meta`; the operator confirms it loads. | PASS |
| User Interface | None. | PASS |
| Localization | None: no user-visible string; attribute descriptions are not localized. | PASS |
| Testability | Scenario 9 runs in the demo world on a dedicated server and fails on every broken requirement; the operator places one particle entity. | PASS |

Amendment 2026-10-21, second part, timed map zones and the freeze-end reading (Design item 13):

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | The reading reuses three values the addon already has: the mission-end timer's replicated start, the replicated freeze countdown (an `onRplName` callback, the engine's own notification of a received value) and the configured freeze length. The zone's visibility check is the existing one, asked on map open and every frame. Nothing is spiked. | PASS |
| II. Ask, Never Assume | Four answers from the operator in the spec's session of 2026-10-21: a delay setting on the zone, working on its own without the existing checkbox, counted from the end of freeze, for map zones only. | PASS |
| III. KISS | One attribute on the zone, one getter on the game mode that both features read. | PASS |
| IV. YAGNI | No delay for manual markers, no new component, no other start moment for the zone. | PASS |
| V. Replication | Replicated values added: none; the existing `m_fFreezeTimeRemaining` gets an `onRplName` callback, which changes nothing that is sent. RPCs added: none. `RplSave`/`RplLoad`: unchanged. The freeze-end reading is local to each machine. | PASS |
| VI. Comments and Hygiene | One WHY comment on the order of the reading. The timed effect drops its own copy of the rule. | PASS |
| VII. Optional Integrations | Not an integration. A zone without the setting is unchanged. | PASS |
| Engine and Asset Constraints | No asset. | PASS |
| User Interface | None beyond the zone being drawn or not. | PASS |
| Localization | None. | PASS |
| Testability | Scenario 10 in the demo world on a dedicated server. | PASS |

Amendment 2026-10-21, random deployment: line drop, air drop of bodies without a player, spawn-point arrow (Design item 11):

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Bodies without a player are moved by the same `MoveUnowned_S` (the game's `SetTransform`) and holstered with the game's `RemoveGadgetFromHand` / `TryEquipRightHandItem`, as the player's drop does on its machine. The line uses the point's own transform axis and `GetSurfaceY`. The arrow is the game's `Shape.CreateArrow` inside `#ifdef WORKBENCH`. | PASS |
| II. Ask, Never Assume | Three answers from the operator in the spec's session of 2026-10-21; the operator saw bodies without a player land under the mission's parachute. | PASS |
| III. KISS | Five attributes on the existing component, two helpers, one Workbench-only component on the existing spawn-point prefab. | PASS |
| IV. YAGNI | No obstacle or water search for the line, no per-body spacing, no runtime arrow. | PASS |
| V. Replication | Replicated values added: none. RPCs added: none. Bodies without a player are moved on the server, which owns them; players still get the one owner message per deployment. | PASS |
| VI. Comments and Hygiene | WHY comments on the server-side holster and on the line's missing clearance search. | PASS |
| VII. Optional Integrations | Not an integration. A deployment without the line setting places as before; an air drop now brings bodies without a player down from the height as well. | PASS |
| Engine and Asset Constraints | `Prefabs/Spawn/LL_SpawnPoint.et` gains the arrow component; the operator confirms it in Workbench. | PASS |
| User Interface | None. | PASS |
| Localization | None. | PASS |
| Testability | Scenario 8 steps 11 and 12 in the demo world on a dedicated server. | PASS |

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
7. Amendment 2026-10-11: open `Prefabs/Spawn/LL_RandomDeployment.et` (a generic
   entity carrying `LL_RandomDeploymentComponent` and nothing else) and
   `Prefabs/Spawn/LL_SpawnPoint.et` (the game's `SCR_Position`), both written by
   the agent on the operator's waiver for these files; confirm they load and that
   Workbench kept the GUIDs of their `.meta` files. No script references either
   prefab, so moving them to another folder breaks nothing.
8. Amendment 2026-10-11: in the demo world, give two squads an entity name if they
   have none, place six `LL_SpawnPoint` entities and name them (three per squad),
   place two `LL_RandomDeployment` entities with the attributes of quickstart
   Scenario 8, and put a car next to the first squad's placed position.
9. Amendment 2026-10-21: compile the addon and report any error verbatim; confirm
   that `Particles/LL_SignalSmoke_Red.ptc` loads (the agent registered it through
   Workbench, which wrote its `.meta`).
10. Amendment 2026-10-21: in the demo world, place the game's
    `Prefabs/Particles/EnvironmentParticle_Base.et` beside a squad's placed position,
    set its effect path to `Particles/LL_SignalSmoke_Red.ptc`, add
    `LL_TimedParticleComponent` with `m_fBurnSeconds` 30, and set the demo game
    mode's freeze time to one minute for the test.
11. Amendment 2026-10-21: in the demo world, place an `LL_MapPolyZone` with
    `m_fHideDelaySeconds` 30 and `m_bHideAfterFreezeTime` off; for step 4 of quickstart
    Scenario 10, a mission-end timer trigger lasting longer than the test.

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
        ├── LL_M_SCR_AIGroup.c                         # new: suppresses initial and queued member spawns for restored map squads
        ├── LL_M_SCR_PersistenceSystem.c               # new: forwards the native after-load result to the game mode
        ├── LL_M_SCR_PlayerReconnectDataSerializer.c   # new: writes and reads nothing
        ├── LL_M_SCR_PlayerControllerSerializer.c      # new: writes and reads nothing
        ├── LL_M_SCR_HitZone.c                         # damage gate also checks the hard freeze (the hold)
        ├── LL_M_SCR_MapMarkerSyncComponent.c          # server marker gate also checks the hard freeze
        └── LL_M_VehicleControllerComponent.c          # new: OnBeforeEngineStart refuses during the hold
```

Amendment 2026-10-11, random squad deployment:

```text
Lite Lobby/
├── Prefabs/Spawn/LL_RandomDeployment.et         # new: generic entity + the component (hand-written on the operator's waiver)
├── Prefabs/Spawn/LL_SpawnPoint.et               # new: the game's SCR_Position, the spawn point a mission maker places and names
└── scripts/game/
    ├── Core/
    │   ├── LL_RandomDeploymentComponent.c       # new: attributes, server-side list, draw, positions, placement, start-time log
    │   ├── LL_GameModeCoop.c                    # draw before the state turns GAME, placement before the body loop, end-of-freeze run behind a once-per-game flag, the flag's getter and restore
    │   ├── LL_LobbyManager.c                    # the body replacement takes a pending spawn transform when one is waiting
    │   └── LL_LobbyPlayerComponent.c            # one owner message: leave the seat if seated, then the game's own player teleport
    ├── Markers/LL_SpawnMarkers.c                # no marker for an end-of-freeze squad; the drawn point for a start-of-game squad
    └── Persistence/LL_SessionData.c             # freezeEndDeployed, record version 2
```

Amendment 2026-10-21, timed particle effects and timed map zones:

```text
Lite Lobby/
├── Particles/LL_SignalSmoke_Red.ptc             # new: the game's red smoke grenade particle, continuous
└── scripts/game/
    ├── Core/LL_GameModeCoop.c                   # the freeze-end reading both features share
    ├── Effects/LL_TimedParticleComponent.c      # new: starts and stops a placed particle by game state and mission clock
    └── Zones/LL_MapZoneComponent.c              # the hide delay
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
   spawn their prefab members on resume, including queued expansion after the
   load completes (research R4). The guard uses map-entity identity and the world's
   persistence load origin; fresh starts and later dynamic squads use vanilla
   spawning. The restored bodies are the only bodies. `OnGameStart` with a save active: skip
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

11. **Random squad deployment** (amendment of 2026-10-11; FR-024 to FR-035;
    research R10).
    - **Authoring.** One world entity per deploying squad carries
      `LL_RandomDeploymentComponent` with five attributes
      ([data-model.md](data-model.md)): the squad's entity name, the entity names
      of its spawn points, the moment (a checkbox: end of freeze, on by default;
      off is the start of the game phase), the height and the dispersion radius.
      A link is an entity name resolved with `FindEntityByName`, as every link in
      the addon is. A spawn point is any named entity; only its position is read. The addon
      ships `LL_SpawnPoint.et` to place, as it ships a flag pole for zone flags:
      the game's `SCR_Position`, which draws a sphere and an arrow in the World
      Editor and does nothing in the game.
      The component registers in a static server-side list that is flushed on a
      world change, the trigger list's pattern, carries no replication and does
      nothing on a client.
    - **Draw.** `Math.RandomInt` over the points that resolved, on the server.
      Start-of-game deployments draw in `SetLobbyState` before the state turns
      GAME, because the state change itself triggers the spawn-marker snapshot,
      which must already know the point. End-of-freeze deployments draw in
      `OnFreezeTimeEnded_S`. A mission with no freeze time never reaches that
      hook, so its end-of-freeze deployments are drawn and placed with the
      start-of-game ones.
    - **Positions.** One ground position per living slot body of the squad (the
      slots whose group id is the squad's, the enumeration the spawn markers
      use): up to ten tries of `SCR_Math2D.GenerateRandomPointInRadius` inside
      the radius, each settled by `SCR_WorldTools.FindEmptyTerrainPosition`
      within two metres, accepted when at least 1.5 m from every position already
      given out; when no try passes, the last candidate is used and logged. A
      body with a connected player gets the height added; a body without one
      stays on the ground.
    - **Start of the game phase** (FR-029). In `OnEnterGame_S`, before its body
      loop, every placed body of the squad is moved to its ground position with
      `SCR_EditableEntityComponent.SetTransform`, which runs the teleport where
      the body is owned, here the server, and informs the replication scheduler.
      For each slot with a connected player the component also keeps a pending
      spawn transform keyed by the placed body, the ground position plus the
      height; `RespawnSlotCharacter_S` takes it in place of the placed body's
      transform. The fresh body is therefore created at the point, on the ground
      or in the air, and its player never stood in a freeze zone around the
      placed position. The pending transforms are dropped at the end of
      `OnEnterGame_S`. A slot nobody holds, or one held for a disconnected
      player, keeps its placed body at the ground position; whoever enters it
      later gets a fresh body there through the unchanged possession path
      (FR-031).
    - **End of freeze** (FR-029, FR-030). `OnFreezeTimeEnded_S` runs the
      end-of-freeze deployments once per game behind `m_bFreezeEndDeployed`; a
      freeze the admin adjustment revives cannot clear it. A body a connected
      player controls is moved on that player's machine:
      `LL_LobbyPlayerComponent.Deploy_S(position)` sends one owner message and
      the owner calls `SCR_Global.TeleportLocalPlayer`. When the owner's
      character sits in a vehicle, the owner first leaves the seat with
      `GetOutVehicle(EGetOutType.TELEPORT, …)` and checks again every half
      second, ten times at most, because the teleport of a seated player moves
      the vehicle. A body without a connected player is server-owned: seated, it
      is taken out the same way on the server; then it is moved with
      `SetTransform`. A get-out that never completes is logged and that body
      stays where it is.
    - **Spawn markers** (FR-032). `BuildSquadMarkers_S` leaves out a squad with
      an end-of-freeze deployment in both of its snapshots, so the marker is
      never sent. For a squad with a start-of-game deployment the game-phase
      snapshot uses the drawn point, because the bodies are replaced after the
      snapshot is taken.
    - **Resume** (FR-033). `m_bFreezeEndDeployed` is written into the session
      record (`freezeEndDeployed`, record version 2) and restored by
      `CompleteResume_S`. A resume never runs `OnEnterGame_S`, so nothing is
      drawn or placed by the resume itself; a deployment still ahead runs when
      the restored freeze ends after the release.
    - **Log** (FR-035). At the start of the game phase, one line per deployment
      whose squad name resolves to no squad, per point name that resolves to no
      entity, per deployment left with no point, and per point named by two
      deployments. Per deployment run: the squad, the drawn point, the bodies
      placed and the height.
    - **Not built**: a parachute or any fall control, a message to players, a
      guide page, a random position inside an area, settings per point.
    - **Amendment 2026-10-21.** Bodies without a player arrive at the height as well,
      holstered on the server first (`HolsterForDrop_S`); the mission's parachute
      brings them down. A line drop (`m_bLinear` with `m_fLineSpacing`,
      `m_fLineJitter`, `m_fLineSpread`, `m_fLineRise`) replaces the radius spread:
      `BuildLine_S` lays the bodies along the drawn point's forward axis, kept from
      `Draw_S` (`m_vPointDirection`), and `LineRise` raises each later one. The
      spawn-point prefab carries `LL_SpawnPointArrowComponent`, which draws that
      axis in the World Editor only.

12. **Timed particle effects** (amendment of 2026-10-21; FR-036 to FR-042; research
    R11).
    - **Authoring.** The mission maker places a particle effect entity (the game's
      `EnvironmentParticle_Base.et`, as the game places its smokestacks) with its
      effect path set to the addon's `Particles/LL_SignalSmoke_Red.ptc` or any
      continuous particle, and adds `LL_TimedParticleComponent` with two attributes
      ([data-model.md](data-model.md)): `m_bAtFreezeEnd` (on by default; off is the
      start of the game phase) and `m_fBurnSeconds` (900 by default).
    - **Per machine.** On every machine but a dedicated server the component ticks
      once a second through the call queue. The first tick stops the particle, which
      the game plays on load like any placed ambient effect (R11.1). Each tick reads
      `GetState()`, `GetFreezeTimeRemaining()` and the game's `GetElapsedTime()`, all
      already replicated (R11.3):
      - before the game phase, or at the end of freeze while the countdown runs: out;
      - the start reading: for the end of freeze, the game mode's freeze-end
        reading (Design item 13); for the game phase, zero;
      - burning while the clock minus the start reading is under `m_fBurnSeconds`,
        then `StopEmission()`, which lets the emitted smoke finish (R11.2), and the
        tick ends;
      - once lit, the countdown is not read again (FR-040); leaving the game phase
        stops it.
    - **Particle.** `LL_SignalSmoke_Red.ptc` is the game's
      `Smoke_grenade_M18_Red.ptc` with its emission time, repeat flag and the curves
      that shape a grenade's ninety-second burn removed, as the game's continuous
      smokestack particle has none, and the first emitter's birth rate lowered from
      10 to 3, near the mean of its tapering burst (R11.2).
    - **Not built**: a prefab, other colours, a start delay, a burn-until-the-end
      option, sound, a server-side or replicated clock.

13. **Timed map zones and the freeze-end reading** (amendment of 2026-10-21, second
    part; FR-043 to FR-046; research R12).
    - **Zone.** `LL_MapZoneComponent` gets `m_fHideDelaySeconds` (0, off, by
      default) beside `m_bHideAfterFreezeTime`. `IsVisibleForLocalPlayer()` hides the
      zone when the state is GAME, the countdown is zero, and either the checkbox is
      on or the delay is above zero and the clock minus the freeze-end reading has
      reached it. The zone already asks that question
      when the map opens and every frame while it is open.
    - **Reading.** `LL_GameModeCoop.GetFreezeEndedAt()`: `m_fMissionEndStartedAt`
      when it is set (the mission-end timer's start, replicated and saved);
      otherwise `m_fFreezeEndSeenAt`; otherwise `m_iFreezeTime / 1000`.
      `m_fFreezeEndSeenAt` is local to each machine and set once: on the server in
      `OnFreezeTimeEnded_S`; on a client by an `onRplName` callback on the existing
      `m_fFreezeTimeRemaining`, when it arrives at zero after this machine saw it
      above zero. A client that joins after the freeze only ever sees zero and keeps
      the fallback.
    - **Timed effects** take their start from `GetFreezeEndedAt()` when they light,
      instead of watching the countdown themselves.
    - **Not built**: a delay for manual markers, a new replicated value, a delay
      counted from anything but the end of freeze.

## Constitution Check (post-design)

Re-run after the five external reviews of 2026-09-18 and 2026-09-19 (the fifth: "ready with the listed corrections", all recorded): three
once-set replicated scalars added and declared (principle V), the mission clock
reusing the game's own replicated value with a write-back hold, no prefab touched, two hand-authored configs and one launch
parameter listed for the operator, vanilla patterns named per mechanism, one spike
that returns to the operator on failure, the reviews' operator questions answered
in the spec's second and third clarification sessions. PASS on the documents; the
dedicated-server gates in the quickstart remain open until run.

Amendment 2026-10-11, re-run after Design item 11: no replicated value, one owner
message to the moved player, one saved bool with a version bump, two new
prefabs, a vanilla source named for every engine call, no spike. PASS on the
documents; Scenario 8 on a dedicated server is the open gate, and the parachute
landing stays open until it is run on a mission that has one.

Amendment 2026-10-21, re-run after Design item 12: no replicated value, no RPC, no
prefab, one particle file derived from the game's, a game source named for every
call, no spike. PASS on the documents; Scenario 9 on a dedicated server is the open
gate.

Amendment 2026-10-21, second part, re-run after Design item 13: no replicated value
(the callback on an existing property adds nothing to the wire), no RPC, one
attribute on the zone, one reading for both features. PASS on the documents;
Scenario 10 on a dedicated server is the open gate.

## Complexity Tracking

No violations to justify.
