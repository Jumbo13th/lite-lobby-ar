# Research: Session Resume After a Server Crash

Every fact below was read from `Arma-Reforger-Script-Diff` at v1.8.0.13 (1.8.0.10 is
identical for these files) or from the addon's own scripts. Where a behaviour lives
in the engine and cannot be read from script, it is marked **unverified** and the
plan carries a spike for it.

## R1. Which save system to build on

- **Decision**: the game's own persistence system: the `PersistenceSystem` world
  system, `SaveGameManager`, the script serializer base classes under
  `scripts/Game/Plugins/Persistence/`, and the config chain under
  `GameData/Configs/Systems/Persistence/`. Lite Lobby ships one persistence config
  that inherits the vanilla `EditableMission.conf` chain and appends its own
  records.
- **Rationale**: it is what Conflict uses. Characters, vehicles, items, weapons,
  mines, doors, ruined buildings, AI groups, Game Master placements and weather are
  already covered by 59 vanilla serializers and per-prefab rules
  (`Configuration/Character/Character.conf`, `Vehicle/Vehicle.conf`, `Weapon/*.conf`,
  `Building/*.conf`, `GameMode/TimeAndWeather.conf`, ...). Inheriting
  `EditableMission.conf` rather than `GameMode/Conflict.conf` keeps Game Master
  placements (`SCR_EditableEntityComponentSerializer`) and leaves Conflict's base
  logic out.
- **Alternatives considered**: a Lite Lobby JSON snapshot of positions and
  inventories in `$profile` (rejected: it would re-implement inventory, seating,
  damage and destruction serialisation the engine already does natively, and could
  never restore building ruins); the binary-versus-JSON save backend
  (`Database/BinarySaveGame.conf` is what vanilla references; the JSON variant
  exists but no shipped config uses it, so it stays untouched).

## R2. How the config is selected, and what it must contain

- **Decision**: two new config files.
  `Configs/Systems/LL_LobbySystems.conf` is a `SystemSettings` inheriting
  `{1C60D2EDA2B468B8}Configs/Systems/BaseGameModeSystems.conf` whose
  `SCR_PersistenceSystem` entry points `Config` at
  `Configs/Systems/Persistence/LL_Persistence.conf`, a `PersistenceSystemConfig`
  inheriting `{5BA7C4643477E2D7}Configs/Systems/Persistence/EditableMission.conf`.
  The mission header's world-systems config is set to `LL_LobbySystems.conf`.
- **Rationale**: the persistence config is a field of the world-systems config, not
  of any prefab: `MissionSystems.conf`, `ConflictSystems.conf`, `GameMasterSystems.conf`
  each carry `SCR_PersistenceSystem { SystemLocation Server; Config "..." }`. The
  mission picks the systems config through the native `MissionHeader.GetWorldSystemsConfig()`;
  the field is native, not a script attribute, so it is set in Workbench on the
  mission header (or with the `-worldSystemsConfig` dedicated-server parameter).
  Config inheritance is the `Type : "{GUID}path" {` syntax; arrays append with `+{`
  and replace with `{`; an inherited entry is amended by restating only its GUID and
  the changed fields (`Mission.conf` lines 15-24 flip `SelfSpawn` on
  `{64ECE6462993EA13}` this way).
- **Rules and states that matter to the lobby**:
  - Player characters: `Character/Player.conf` has `SelfSpawn 0` but `Mission.conf`
    sets it back to 1, so saved player bodies come back on their own. A body whose
    player was disconnected at save time is a plain character (`Character.conf`,
    `ParentHandling "Ignore always"`, hitzones, inventory, seating) and also comes
    back.
  - Nothing in the lobby's own prefabs is tracked unless it inherits a vanilla base
    that carries the engine `Persistence` component. Lite Lobby's character and
    vehicle base overrides inherit `Character_Base.et` and `Vehicle_Base.et`, which
    carry it. `LL_VoNProxy.et`, the trigger prefabs, zones, manual markers and the
    spectator camera are plain entities with no such component and no rule: never
    tracked, which is what we want for all of them.
  - Two vanilla records must be neutralised, and there is no subtractive config
    syntax: `SCR_PlayerReconnectDataSerializer` deletes saved player characters not
    reclaimed within `SCR_ReconnectComponent.GetReconnectTimeout()` after a load
    (`SCR_ReconnectSerializer.c` lines 45-79), and `SCR_PlayerControllerSerializer`
    hands controlled-entity ids to the respawn system the lobby stubs to null
    (returns false on load, harmless but noisy). Both are script classes, so each
    gets a modded subclass in `scripts/game/Modded/` whose `Serialize` returns
    `ESerializeResult.DEFAULT` and whose `Deserialize` returns true without
    registering anything. The lobby's own reservation model replaces both.
- **Alternatives considered**: forking the vanilla group files to drop the two
  entries (rejected: copies vanilla GUIDs into the addon and breaks on every game
  update); a persistence component on each lobby trigger prefab plus a component
  serializer (rejected: eight prefab edits in Workbench and reliance on the native
  world-entity id derivation, which no script shows; a persistent state keyed by
  the trigger's existing stat key needs neither).

## R3. How the lobby's own state is recorded

- **Decision**: three `PersistentState` proxies with `ScriptedStateSerializer`s, the
  pattern `SCR_TaskSystemSerializer.c` uses for world-placed logic (an empty
  `PersistentState` subclass whose serializer ignores the instance and reads the
  live system):
  - `LL_SessionData` / `LL_SessionSerializer`: game state, freeze and hard-freeze
    remaining, elapsed game seconds, statistics session id, and the slot table
    (per slot: body UUID from `PersistenceSystem.GetId`, holder identity key,
    holder display name, KIA, locked, sort key).
  - `LL_TriggerData` / `LL_TriggerSerializer`: per trigger stat key, the fired flag
    and the subclass state (`m_fSecondsLeft`, `m_fHeld`/`m_bCaptured`,
    `m_fProgress`, `m_bBothSeen`), through two virtual hooks on
    `LL_TriggerComponent` that subclasses override. Triggers register in a
    server-side static list on `OnPostInit`.
  - `LL_MarkerData` / `LL_MarkerSerializer`: every static marker from
    `SCR_MapMarkerManagerComponent.GetStaticMarkers()` with the fields the
    manager's own `RplSave` writes (position, flags, config id, faction flags,
    rotation, type, colour, icon, text, timestamp), restored through
    `InsertStaticMarker(marker, false, true)`.
  All three are declared with `PersistentStates +{ }` and `StatePersistenceConfig`
  entries in `LL_Persistence.conf`, in a new collection `LL_Lobby` appended to the
  `WorldState` bundle, copying the shape of `Mission.conf` lines 58-88.
- **Rationale**: serializers are bound only by `GetTargetType()` plus a config entry
  (no discovery by name); a state needs no prefab, no GUID override of a vanilla
  config and no entity id derivation. `SaveContext`/`LoadContext` give
  `WriteValue`/`ReadValue`, `WriteDefault`/`ReadDefault`, `StartObject`/`EndObject`,
  `StartArray`, `StartMap`; every vanilla serializer writes a `version` first.
- **Marker owners**: `m_iMarkerID` and `m_iMarkerOwnerID` are session-scoped
  integers. Restored markers are re-inserted as server markers (owner -1) with their
  faction flags, which keeps the faction gate and admin removal but means a player
  cannot delete a marker they placed before the crash. Storing the owner's identity
  and re-linking on reconnect was considered and dropped: it needs a per-marker
  identity map and a rewrite of ownership after every reconnect, for the one case
  of deleting an old marker.
- **Statistics**: not serialised twice. `LL_StatsManager` already writes a live
  snapshot to `$profile:LL_GameStats/<session>-live.json` every 60 s. The session
  record stores the session id; on resume the manager reloads that snapshot with the
  existing JSON structs and keeps the same session id so later snapshots continue the
  same file family. The session serializer's `OnBeforeSave` hook asks the manager to
  write the snapshot first, so the two are never more than one save apart.

## R4. When the record is applied, and how bodies become slots again

- **Decision**: states deserialize during the load phase, before the persistence
  system reports `ACTIVE`. `LL_SessionSerializer.Deserialize` registers a
  `PersistenceWhenAvailableTask` per saved body UUID (the pattern of
  `SCR_AIGroupSerializer` for members) and a `GetOnStateChanged` handler for
  `ACTIVE` (the pattern of `SCR_ReconnectSerializer`). When a body becomes
  available, the task marks its `LL_PlayableComponent` runtime-spawned and sets the
  saved sort key before the component's own 500 ms registration runs. The `ACTIVE`
  handler then finalises: applies KIA and lock flags, seats the reservation for each
  holder, moves the game mode to GAME through a resume entry that skips the
  fresh-body spawn, starts the hold, and reloads statistics.
- **Registration race**: `LL_PlayableComponent.RegisterWithManager` skips a body
  with no AI group, and restored bodies are re-added to their group asynchronously
  by `SCR_AIGroupSerializer` (`WhenAvailable` per member). While a resume is pending
  the component retries instead of skipping, bounded like the existing RplId retry.
- **Reservations without player ids**: today a disconnected holder is the old player
  id left in the slot plus an entry in `m_aDisconnectedPlayerIds`,
  `m_mPlayerNames` and `m_mDisconnectedPlayers[key]`. After a resume no player ids
  exist, so the finaliser allocates a placeholder id per absent holder from a range
  the engine never assigns (1,000,000 upward), and fills the same three structures.
  The existing reconnect path (`OnPlayerConnected` → key → reserved slot →
  `LeaveSlot_S`/`RemovePlayer_S` on the old id → `ReconnectPlayer` → `TakeSlot_S` →
  `PossessSlot_S`) then works unchanged, and the roster shows the greyed holder
  names through the existing replication.
- **Skipping the fresh start**: `OnGameStart` sets SLOTSELECTION only when no save is
  active (`SaveGameManager.GetActiveSave()` is null). `SCR_GameModeCampaign.Start`
  uses the same guard through `SCR_PersistenceSystem.IsLoadInProgress()`.
- **Alternatives considered**: a component serializer on `LL_LobbyManager` bound
  through a GUID override of `GameMode.conf` (works, but mixes entity-bound records
  with state records; the state form is uniform for all three).

## R5. The hold

- **Decision**: reuse the hard freeze with an indefinite duration. A negative
  `m_fHardFreezeRemaining` means "held until an admin releases", no timer is
  scheduled, and the HUD shows "resuming, N of M players back" (computed on each
  client from the replicated slot table and `IsPlayerDisconnected`) instead of the
  countdown. `/resume` ends it; `EndHardFreeze_S` restarts the freeze timer if
  freeze time remains. Characters are already locked by the hard freeze (movement,
  view and weapon disable flags, gadget and inventory gates). Damage protection is
  extended: the hitzone gate that already zeroes damage during freeze also checks
  `IsHardFreezeActive()`. Triggers and the mission-end countdown already stop on
  `IsHardFreezeActive()`; the freeze timer gains the same check.
- **Vehicles on the ground**: at finalisation the server runs the vanilla
  disconnect recipe on every tracked vehicle: cars and tracked vehicles
  `Shutdown()`, `StopEngine(false)`, `SetPersistentHandBrake(true)`; helicopters
  `ForceStopEngine()`, `SetPersistentWheelBrake(true)`. These calls are server-side
  and engine-replicated (`SCR_BaseGameMode.OnPlayerDisconnected`, lines 935-950).
  While the hold flag is set, a modded `OnBeforeEngineStart()` returns false, so
  nobody starts an engine, and the get-in action is refused through the same
  `CanBePerformedScript` gate the crew lock already uses.
- **Aircraft in the air**: **no vanilla precedent**. No script ever deactivates a
  vehicle's physics; the only pin-in-place recipe is
  `SCR_JointDummyHolderEntity.c` lines 55-58 (`SetLinearFactor(vector.Zero)`,
  `SetDamping(1000, 1000)`, zero velocities), and physics authority follows the
  replication owner (`SCR_EditableEntityComponent.c` lines 707-747 RPC transforms
  to the owner). Plan: at finalisation the server owns every vehicle (nobody is
  connected) and pins each airborne helicopter (`GetAltitudeAGL()` above a small
  threshold) with that recipe; when a pilot re-possesses a body seated in a pinned
  aircraft, ownership moves to their client, and the client's lobby player
  component re-applies the pin locally while the replicated hold flag is set.
  Release unpins on every machine, then enables autohover so the aircraft holds
  altitude until the pilot takes the controls, which is what vanilla does for a
  pilot who drops mid-flight. This is the first task of the implementation as a
  spike; if the pin does not survive the ownership transfer, the operator decides
  the fallback (spec edge case), the candidates being autohover only, or ground
  placement.
- **Alternatives considered**: `Physics.SetActive(ActiveState.INACTIVE)` (same
  ownership caveat, no vanilla caller at all); `NwkMovementComponent.EnableSimulation(false)`
  (vanilla uses it only on the editor camera and headgear items).

## R6. Saving: when, how, how heavy

- **Decision**: `RequestSavePoint(ESaveGameType.AUTO)` (non-blocking) from a
  game-mode timer at the configured interval, only in GAME, not during the hold,
  not while `SaveGameManager.IsBusy()`, not while `IsLoadInProgress()`. `/save`
  requests `MANUAL`. `/shutdown` requests `SHUTDOWN` with `BLOCKING`, then in the
  callback disallows saving and calls
  `GameStateTransitions.RequestGameTerminateTransition()`, the shape of
  `SCR_PauseMenuUI.c` lines 518-537. Save duration is measured between
  `SCR_PersistenceSystem.GetOnBeforeSave()` and `GetOnAfterSave()` and logged with
  type and result.
- **Rationale**: there is no periodic autosave anywhere in the game scripts; Conflict
  saves on base capture with exactly this guard (`SCR_CampaignMilitaryBaseComponent.c`
  line 1651). There is no script event for the server process being told to stop,
  so a planned stop with a final save is the addon's own command; an OS-level stop
  is, for the addon, a crash, and the last periodic save applies.
- **Enabling**: the mission header's save-type flags gate everything
  (`game.c OnMissionSet` → `SetEnabledSaveTypes`), default 15 = all. With the
  game-mode attribute off, `OnGameStart` calls `SetEnabledSaveTypes(0)`, which the
  header's own description says disables the whole persistence system, so an
  unconfigured mission writes nothing.
- **Cost**: **unverified**. The `BLOCKING` flag means "save all immediately"; the
  non-blocking form spreads the work, but how much main-thread time a save of a
  127-player world takes is engine-side. The quickstart measures it at scale; the
  interval default of five minutes is provisional.

## R7. Which save is resumed, and how the server finds it

- **Decision**: rely on the engine. `SaveGame.GetId()` is documented as "primarily
  used for CLI loading by given id", `OnGameModeEnd` on a dedicated server deletes
  the session storage unless `-keepSessionSave` is passed, and `IsLoadInProgress`
  reads `GetActiveSave()` at world start. That is consistent with the engine
  applying the newest session save of the mission when a dedicated server starts
  it again, but no script shows it. Spike: enable saves in the demo world, take one,
  kill the server, restart, and read `GetActiveSave()`/`WasDataLoaded()` in the log.
- **Fallback if the engine does not**: script-side selection at start, one shot:
  `GetSaves(GetCurrentMissionResource(), cb)`, pick the newest whose
  `IsSavePointGameVersionCompatible()` and `AreSavePointAddonsCompatible()` hold,
  then `Load(save)` with the default transition, which restarts the world with the
  save active. Guarded by `GetActiveSave()` being null so it cannot loop.
  `SCR_ScenarioUICommon.ProcessLoadSave` is the vanilla shape (client menu).
- **Completeness**: `SaveGame` has no completeness flag; version and addon
  compatibility are the only checks. A save interrupted by a crash is the engine's
  problem to reject, and the spec's FR-013 is met by the engine keeping the previous
  save point, which is how save points work (each is its own record).
- **Discard**: `SetSavingAllowed(false)`, then
  `PersistenceSystem.ClearStorage(PersistenceSessionStorage)` and
  `SaveGameManager.Purge(mission, playthrough)`, the body of the protected
  `SCR_BaseGameMode.HandleOnGameModeEndSaveData`, which a subclass can call
  directly. Called on DEBRIEFING and on `/discard confirm`; after a discard the
  save timer stays off for the rest of the session.

## R8. What a resume cannot restore (accepted, from the spec)

Velocity of anything, towed and slung loads, small destructibles (no serializer for
`SCR_DestructibleEntity`, multi-phase destruction, tyres, glass), wrecks, fires,
craters, debris, ragdoll pose. The spec's edge cases state them; the plan does
nothing about them.
