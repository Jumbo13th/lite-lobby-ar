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
  exists but no shipped config uses it; the addon leaves it alone, and a test server
  can select it from the server configuration for readable saves, see R9).

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
    remaining and the daylight setting as they were before any hold, elapsed game
    seconds excluding held time, statistics session id and file, the squad table
    (per squad: entity name, radio frequency) and the slot table (per slot: body
    UUID from `PersistenceSystem.GetId`, squad entity name, holder identity key,
    holder display name, KIA, locked, sort key).
  - `LL_TriggerData` / `LL_TriggerSerializer`: per trigger stat key, the fired flag
    and the subclass state (`m_fSecondsLeft`, `m_fHeld`/`m_bCaptured`,
    `m_fProgress` and the contest holder, `m_bBothSeen`), through two virtual hooks
    on `LL_TriggerComponent` that subclasses override; the subclass state is
    written and restored whether or not the trigger has fired, and a restored
    trigger's first activation after the load keeps the restored fields instead of
    re-initialising them (`LL_TriggerMissionEndTimer.c` lines 38-40 and
    `LL_TriggerZoneContest.c` lines 118-126 reset them on activation today).
    Triggers register in a server-side static list on `OnPostInit`.
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
  cannot delete a marker they placed before the crash (spec edge case). Storing the
  owner's identity and re-linking on reconnect was considered and dropped: it needs
  a per-marker identity map and a rewrite of ownership after every reconnect, for
  the one case of deleting an old marker.
- **Statistics**: not serialised into the save, but tied to it. `LL_StatsManager`
  already writes `$profile:LL_GameStats/<session>-live.json` every 60 s with its
  JSON structs. Reloading that file on resume was rejected: at crash time it is up
  to nine minutes newer than the world and would credit kills of players who are
  alive again. Instead, on `SCR_PersistenceSystem.GetOnBeforeSave()` the manager
  writes `<session>-save-<yyyymmdd-hhmmss>-<n>.json` (`n` a per-session counter, so
  two saves in one second never share a name) and the session record stores that
  name, or an empty string when the write failed (`WriteSnapshot` is void and logs
  its failure today, `LL_StatsManager.c` lines 1461-1476; it gains a bool result).
  On resume the manager loads exactly that file and keeps the session id so later
  files continue the family; a missing or unreadable file is logged, flagged in the
  recorder so the next publish reports incomplete statistics, and does not refuse
  the resume (spec clarification). Pruning: `GetSaves(mission, cb)` is
  asynchronous (`SaveGameManager.c` lines 51-56, callback with a success flag,
  `SaveGameObtainCallback.c` lines 12-22). After a successful callback, a file is
  deleted when it is older than the oldest listed save point's
  `GetSavePointCreatedUnix()` by more than sixty seconds (the margin covers a file
  written just before its own save point's timestamp), never while a write is in
  flight, and never on a failed or empty listing. This follows `saveRetention`
  without a second count and removes orphans of failed world saves; a file from an
  abandoned branch after a rollback stays until it is that old, a bounded leftover
  that is accepted. Run once after each successful save.
  **Point in time**: the before-save callback runs before the serializers read
  (`PersistenceSystem.c` lines 118-125 distinguish before-read from after-commit);
  a non-blocking save may read entities over its duration, so the statistics
  describe the start of the save and the world the span of it, at most one save
  duration apart. Accepted while SC-003 holds the duration under a second.
- **Recorder continuation**: the live snapshot structs (`LL_StatsTypes.c` lines
  130-147) do not carry everything the recorder needs to continue. Inventory of
  what a resume must also restore, from `LL_StatsManager.c`: the recording start
  at lines 258-280, which does five things (sets `m_bRecording`, starts the
  clock, sweeps participants and freezes the commander suggestion, hooks the
  assignment event, schedules and runs the live write); death recording,
  assignment handling, finalisation and publication all return while
  `m_bRecording` is false (lines 361-374, 389-390, 464-465, 880-881). The resume
  therefore does not skip the start: it runs `ResumeRecording_S`, which applies
  the session record's statistics session id first (the manager minted a fresh
  one at start-up, lines 110 and 1495-1512, and a missing file cannot supply the
  saved one), loads the snapshot's file (or sets the incomplete flag when it is
  missing), then sets the flag, hooks the event and schedules the live write,
  exactly once, without the sweep, the commander freeze or the immediate write;
  first-slot holders frozen as
  player ids at lines 300-303 (written by identity key instead, so placeholders
  never become first holders), survival resolution by player id at lines 432-448
  and identity resolution at lines 190-242, which falls back to `pid:<id>` for an
  id the engine does not know (both resolve through the lobby's reverse key map,
  `GetReconnectKeyForPlayer_S`, so an absent placeholder keeps the saved identity's
  credit), the one-time survival resolution guarded at lines 409-413, and the
  event timestamps at lines 1484-1491, which switch from a process tick to the
  mission clock (R5) so they survive a resume. The incomplete flag is written into
  the file so it survives another snapshot and resume. The nested-JSON readback
  limitation noted at `LL_StatsTypes.c` lines 150-152 means the round trip is
  demonstrated in the quickstart before it is relied on.

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
- **Squad membership is not saved for player bodies.** `SCR_AIGroupSerializer`
  writes only AI members: `Serialize` skips any character `EntityUtils.IsPlayer`
  reports as player-controlled ("Players are stored via player controller so we can
  decide if and how they join on reconnect", lines 50-58), and player membership is
  restored through a `WhenAvailable` on the player controller (lines 334-347,
  367-384), which the lobby's neutralised controller record never satisfies. A body
  a player controlled at the snapshot therefore comes back with no group, and
  `LL_PlayableComponent.RegisterWithManager` rejects a body with no group
  (`LL_PlayableComponent.c` lines 145-150). A body whose player was disconnected
  at save time is an AI member and is re-joined by the game. Decision: the slot
  record names the squad by the group entity's own name (`IEntity.GetName()`), not
  by `LL_SlotData.m_sGroupName`, which is the translated callsign when the squad
  has one and the entity name only as a fallback (`LL_PlayableComponent.c` lines
  265-299); the feature requires a unique entity name per squad (FR-021), checked
  at game start. The body-available task re-attaches the body with
  `SCR_AIGroup.AddAIEntityToGroup` (`SCR_AIGroup.c` line 1927, the call the group
  uses for its own spawned members at line 1763) when the body has no group, before
  registration. A body whose squad entity cannot be found refuses the resume
  (FR-014).
- **Attachment needs the agent**: `AddAIEntityToGroup` returns false when the
  character's `AIControlComponent` has no agent yet (`SCR_AIGroup.c` lines
  1931-1936), and the lobby already defers attachment of a freshly spawned body
  for that reason (`LL_LobbyManager.c` lines 844-846). Availability of a loaded
  entity does not mean its deferred deserialisation is complete
  (`PersistenceSystem.c` lines 89-97; the task may even run at once, during
  deserialisation, before `ACTIVE`). Registration itself is not a call that
  returns success: `RegisterWithManager` is void, schedules its own 500 ms retries
  while the network id is unassigned, has other early returns (lines 127-150),
  and after twenty tries stamps a fallback id that clients cannot resolve
  (`LL_PlayableComponent.c` lines 78-136). Decision: one bounded operation per
  body, on one absolute deadline the game mode arms at `ACTIVE` (ten seconds)
  and the operation reads on each tick; before it is armed there is no timeout.
  Each tick, wait for the agent, attach the saved squad if the body has no
  group, call `RegisterWithManager` only when the network id is valid, and then
  observe: "done" means the manager returns a slot for this entity whose squad is
  the saved one; the fallback-id branch is never taken while a resume is pending
  (it is a refusal cause instead); every outstanding operation stops when the
  resume is refused.
- **Untracked bodies**: `PersistenceSystem.GetId` is null for an instance that is
  not persistent (`PersistenceSystem.c` lines 30-42). Skipping such a slot would
  make it invisible to the completeness check. Decision: the serializer writes the
  slot with an empty body reference and logs it by name; the finaliser refuses a
  record with an empty body; the game mode logs every registered body with a null
  persistence id and every squad without a unique entity name when the roster is
  complete, at the transition into GAME and before `SetSavingAllowed(true)`
  (FR-021), not at world start, when squads are still spawning members
  (`SCR_AIGroup.c` lines 2595-2607) and registration is deferred
  (`LL_PlayableComponent.c` lines 46-59); so the mission is fixed before an event.
- **Pre-save deletion**: a body deleted before the save unregisters its slot
  (`LL_PlayableComponent.c` lines 63-74, `LL_LobbyManager.c` lines 431-444,
  1183-1194), so the slot is not in the snapshot and does not come back. Existing
  behaviour, kept.
- **Registration order**: the body-available task runs when the engine has loaded
  the body; the squad entity is world-placed and exists earlier. The task waits for
  the agent, attaches the squad, marks the playable runtime-spawned and sets the
  sort key, and only then does the component's own registration run. Attachment
  activates the AI; successful registration installs the existing deactivation
  repeater (`LL_PlayableComponent.c` lines 166-181) and possession uses the
  established `SetInitialMainEntity` path (`LL_LobbyManager.c` lines 492-497).
- **Reservations without player ids**: today a disconnected holder is the old player
  id left in the slot plus an entry in `m_aDisconnectedPlayerIds`,
  `m_mPlayerNames`, `m_mPlayerReconnectKeys[id]` and `m_mDisconnectedPlayers[key]`.
  After a resume no player ids exist, so the finaliser allocates a placeholder id
  per absent holder from a range the engine never assigns (1,000,000 upward) and
  fills all four structures; the reverse key map matters because the next
  snapshot's serializer reads the holder key through it (an invented id has no
  engine identity to ask, `LL_LobbyManager.c` lines 2077-2091), and without it a
  second crash before the holder returns would lose the reservation.
- **Claiming a reservation is not the ordinary reconnect path.** That path
  (`OnPlayerConnected` → key → `GetReconnectSlot`, `LL_LobbyManager.c` lines
  2116-2140 and 2255-2261) consumes the reservation on lookup, refuses a locked
  slot silently, and clears the old holder before `TakeSlot_S`, which then rejects a
  destroyed slot (`LL_SlotData.IsAvailable`, lines 61-64): a KIA holder would lose
  their slot. `ReconnectPlayer` itself calls `TakeSlot_S` (lines 2150-2155), which
  checks availability (lines 637-641), so it cannot follow an ownership transfer
  either: the slot is occupied by then; and the rest of `TakeSlot_S` (lines
  644-660) first leaves the player's current slot, then assigns and broadcasts,
  updates the faction and possesses at once in GAME, with the reconnect caller
  owning the 500 ms readiness delay (lines 2139-2140) and the Game Master
  replacement relying on the take being synchronous before it deletes the old body
  (lines 903-915). Decision: `TakeSlot_S` is not changed at all. Ordinary
  disconnected holders share the reservation map (`m_mDisconnectedPlayers`, line
  2188), so the claim first checks that the reserved slot's current holder is a
  placeholder id (at or above 1,000,000) and otherwise leaves the entry to the
  existing path with its lock and phase rules. A resume claim
  `ClaimResumedSlot_S(playerId, key)` consumes such a reservation once and owns
  the whole transfer itself: it removes the placeholder (`RemovePlayer_S`, which
  clears the name and disconnected entries), sets the slot's player id through
  `ApplyTakeSlot` plus the existing `RpcDo_TakeSlot` broadcast exactly once (the
  one assignment event, lines 1197-1205), calls `SetPlayerEngineFaction_S`, and
  for an alive slot calls `PossessSlot_S` after the same 500 ms readiness delay
  the reconnect caller uses, checking the player is still connected when it
  fires; it never calls `LeaveSlot_S` or `TakeSlot_S`, so no slot is left and
  re-taken and no second assignment fires. The assignment queues the voice
  manager's next-frame resolution (`LL_VoNChannelsManager.c` lines 95-145),
  which computes parking while the player has no body yet; after the delayed
  possession the claim calls `UpdateParked_S` once, no second channel hop. The
  finaliser's spectator entry for unresolved players fires one second later
  (`LL_LobbyPlayerComponent.c` lines 862-874); `EnterSpectatorNow` re-checks
  that the player controls no living body, so an identity that arrives inside
  that second and possesses first is not sent to spectator. A KIA slot gets the
  same transfer and the spectator entry (which already routes voice,
  `LL_GameModeCoop.c` lines 691-699) with no possession. The finaliser calls the
  claim for every player already connected (a player can connect during the load
  phase, before any reservation exists), the connect handler calls it for later
  arrivals before the ordinary reconnect lookup, and the identity-audit hook calls
  it when an identity arrives after connect (lines 2088-2091 cache the identity
  without replaying today). Placeholder ids pass `RemovePlayer_S` and the
  spectator entry unchanged (lines 989-999, 1263-1268; `LL_GameModeCoop.c` lines
  691-700). The roster shows the greyed holder names through the existing
  replication.
- **Voice**: `LL_VoNChannelsManager.AssignChannelsForState_S` routes every slot
  with a non-negative holder into its squad channel (`LL_VoNChannelsManager.c`
  lines 309-329), and the slot-assignment event queues a channel update without a
  KIA check (lines 95-122, 134-141). Decision: both skip placeholder ids (at or
  above 1,000,000) and destroyed slots; a KIA claim ends with the spectator
  routing (lines 333-338); the finaliser assigns channels before it replays
  claims, so the claim's routing is the last word.
- **Skipping the fresh start**: `OnGameStart` sets SLOTSELECTION only when no save is
  active (`SaveGameManager.GetActiveSave()` is null). `SCR_GameModeCampaign.Start`
  uses the same guard through `SCR_PersistenceSystem.IsLoadInProgress()`.
- **The second source of doubled slots: world-placed squads.** The operator saw
  every slot doubled after a restart with the save types enabled, which is why the
  mission guide tells makers to untick them. The mechanism: lobby squads are
  `SCR_AIGroup` entities placed in the world layers with `m_aUnitPrefabSlots`, and
  `SCR_AIGroup.EOnInit` queues a spawn of every listed member on each world load
  (`RequestSpawn`, `Game/Entities/SCR_AIGroup.c` lines 2578-2612). The members are
  runtime characters, so the save holds them as their own records and the engine
  spawns them back; the group then spawns a second set from its prefab list. The
  game's `SCR_AIGroupSerializer` only prevents this for groups the engine itself
  spawns from the save (`DeserializeSpawnData` sets `SCR_AIGroup.IgnoreSpawning(true)`
  before the prefab is instantiated); a loaded group never passes through that
  path. The game's ambient patrol system carries the guard for its own case:
  `if (SCR_PersistenceSystem.IsLoadInProgress()) return;` "otherwise full-size
  groups get spawned even if they are marked as eliminated in the save file"
  (`Game/Systems/SCR_AmbientPatrolSystem.c` line 91). Fix: a modded
  `SCR_AIGroup` in `scripts/game/Modded/LL_M_SCR_AIGroup.c` whose `EOnInit`
  skips the member spawn when `SCR_PersistenceSystem.IsLoadInProgress()` is true
  and otherwise calls the vanilla one. Loaded groups are still tracked
  (`AIGroup.conf` matches `EntityClass "AIGroup"`, and loaded entities get
  deterministic ids), so the group's own record re-joins the restored members
  through `WhenAvailable`, exactly as for an engine-spawned group. Members whose
  bodies were deleted before the save (fresh-body swap, redundant-unit removal)
  have no record and are not spawned by anyone. The `IsLoadInProgress` window is
  the load phase plus one second of world time, so groups a Game Master places
  later in a resumed session spawn normally. Squads a mission creates from script
  are outside this guard; the guide tells makers to create them from the lobby's
  game-start hook, which a resume does not run.
- **Squad frequencies**: the game saves a group's frequency only for groups flagged
  playable in its own group system (`SCR_AIGroupSerializer.c` lines 100-126,
  256-260); lobby squads leave `SCR_AIGroup` playability at its default of off
  (`SCR_AIGroup.c` lines 183-184), so nothing is saved for them, and re-running
  `AssignSquadFrequencies_S` (`LL_LobbyManager.c` lines 526-577) allocates by
  remaining slot order, which shifts every squad after a squad that vanished.
  Decision: the session record carries a squad table (entity name, frequency); the
  finaliser sets each squad's frequency from it before any body is possessed, since
  possession tunes the body's radio from the group (lines 499-505).
- **Refusal**: by the time the persistence system reports `ACTIVE`, bodies, ruins,
  inventories and Game Master edits are already in the world; a game mode cannot
  turn that into a fresh world by changing its state, and purging on refusal would
  destroy the very snapshots an operator needs to roll back. Decision (operator,
  2026-09-18): a refused resume first calls `SetSavingAllowed(false)` and
  `SetEnabledSaveTypes(0)`, the way the game's own exit flow prevents a second
  shutdown save (`SCR_PauseMenuUI.c` lines 530-536; `RequestClose` only sets the
  exit flag, `Game.c` lines 84-87), then logs its cause and calls
  `GetGame().RequestClose()`; nothing is purged, and the snapshot list is the same
  before and after. The refusal cases are: no lobby record deserialized when
  `ACTIVE` arrives (the game mode subscribes to `GetOnStateChanged` itself in
  `OnGameStart` when a save is active, so a missing record still terminates), a
  native load failure (`OnAfterLoad(bool success)` is a protected event on the
  persistence system, `PersistenceSystem.c` line 127, overridden by
  `SCR_PersistenceSystem` for notifications, lines 40-54, and exposed to nobody;
  the lobby mods that class in `Modded/LL_M_SCR_PersistenceSystem.c`, calls
  super and forwards the result to the game mode, the pattern of every other
  lobby override; the state event and the result event are separate and their
  order is not established, lines 17-19 and 40-54, so a forwarded failure
  refuses on its own, at once, through the idempotent refusal, whether or not
  `ACTIVE` ever arrives, and a failure after finalisation still refuses), an
  unknown record version, a slot record with an empty body reference,
  whose body did not become available within the load phase or whose squad entity
  is not found, and a body that registers no slot. Nothing is dropped silently.
  A container that restarts the process afterwards starts fresh, because the
  launcher passes the parameter once; acceptable, recorded in the contract's
  incident procedure. `RequestClose` only sets the exit flag (`Game.c` lines
  84-87); what the launcher does next is its policy, observed in Scenario 7.3.
- **Protection window**: restored groups and agents are activated by the game's
  serializer during the load (`SCR_AIGroupSerializer.c` lines 193-194, 410) and
  `WhenAvailable` does not imply every deferred task has run (`PersistenceSystem.c`
  lines 89-97). The hard-freeze flag is therefore set in `OnGameStart` as soon as
  `GetActiveSave()` is non-null, before `ACTIVE`, so the damage gate and the input
  lock apply to anything that exists; the vehicle stop runs at `ACTIVE`, when the
  vehicles exist. Whether physics advances between the load and `ACTIVE` is
  engine-side, **unverified**; the quickstart logs each body's saved and held
  position and compares them.
- **Alternatives considered**: a component serializer on `LL_LobbyManager` bound
  through a GUID override of `GameMode.conf` (works, but mixes entity-bound records
  with state records; the state form is uniform for all three).

## R5. The hold

- **Decision**: reuse the hard freeze with an indefinite duration. A negative
  `m_fHardFreezeRemaining` means "held until an admin releases", no timer is
  scheduled, and the HUD shows "resuming, N of M players back" (computed on each
  client from the replicated slot table and `IsPlayerDisconnected`) instead of the
  countdown, plus the snapshot time. The existing `/hardfreeze 0` ends it;
  `EndHardFreeze_S` on a held resume restores the pre-hold state: if the record's
  `hardFreezeRemaining` is above zero it starts the ordinary timed hard freeze with
  that value (`StartHardFreeze_S`), else it restarts the freeze timer if freeze time
  remains; day advance goes back to the setting saved from before the hold, not to
  whatever a snapshot taken during a hold happened to see (`m_bDayAdvanceWasOn`,
  `LL_GameModeCoop.c` lines 545-569, is the only place that intent lives today, and
  the game's weather serializer saves the current setting,
  `TimeAndWeatherManagerEntitySerializer.c` lines 29, 80, 158-160; the finaliser
  re-asserts the stopped day advance after `ACTIVE`, since the load order of that
  serializer is not proved). **The freeze countdown** (`UpdateFreezeTimer_S`) is
  scheduled only by the fresh-start entry (lines 403-405) and never by the
  hard-freeze release (lines 526-541): the resume must schedule it exactly once,
  at the end of whichever hold is the last (the resume hold when no timed freeze
  remained, the restored timed freeze otherwise), or freeze time stays positive
  forever and the mission-end timer never starts (`LL_TriggerMissionEndTimer.c`
  lines 33-34). The admin adjustment (`AdjustFreezeTime_S`, lines 625-629)
  removes and re-schedules the countdown directly, and the explicit end and the
  normal expiry remove it (lines 441-445, 643-645): every one of these goes
  through one `ScheduleFreezeCountdown_S` / `CancelFreezeCountdown_S` pair that
  owns the `m_bFreezeTimerScheduled` flag, so an admin adjustment during a hard
  freeze cannot produce a second callback at release.
- **The mission clock**: the game's `m_fTimeElapsed` is `[RplProp]` (line 213),
  advanced on every machine in `EOnFrame` only while running or in pre-game and,
  by default, only while a player is connected (`m_bAdvanceTimeRequiresPlayers`,
  lines 206-207, 2093-2109), and re-sent by the server whenever it has advanced
  ten seconds past the last correction (lines 2113-2119, a threshold in elapsed
  time, not wall time, so a held clock sends no corrections); the spectator clock
  reads it through `GetElapsedTime()` (`LL_SpectatorMenu.c` lines 242-254); the
  lobby's own `m_fGameStartTimestamp` has no reader but its getter. Decision
  (operator, 2026-09-19): every hard freeze holds time, saves on or off. Not by
  subtracting (that runs backwards whenever the base did not add, an empty server
  for instance): the game mode records `m_fHeldElapsed` when a hard freeze begins,
  overrides `EOnFrame`, and on a proxy first compares the replicated field with
  the value it wrote last frame: a difference before super runs can only be a
  correction from the server (the field has no replication callback, line 213,
  and the base's own advance happens inside super), so it becomes the new anchor;
  then calls super, and while `IsHardFreezeActive()` writes `m_fHeldElapsed`
  back over the protected field, on every machine. A client that sees the flag
  before the value holds its local estimate until the value arrives; one that
  sees the value first is corrected by the write-back once the flag arrives.
  Which arrives first is native packet ordering, a dedicated-server check. The
  base's correction test (lines 2115-2118) runs before the write-back, so the
  first held frame may send one correction of `held + slice`; after that the
  threshold is not reached again until release, when corrections resume from
  that stored value. The snapshot's `elapsedSeconds` is
  `GetElapsedTime()`; the resume writes it back into the protected field and into
  `m_fHeldElapsed`; the statistics recorder timestamps events with the same value
  instead of a process tick (`LL_StatsManager.c` lines 1484-1491). Nothing is
  added to the wire for the clock. The base method's finite-duration end check
  (lines 2132-2142, `SCR_GameGameModeStateComponent.c` line 23) reads the value
  before the write-back; lobby missions set no duration, the guide says so, and a
  restored value past a duration ends the mission as the engine would. A snapshot
  during an ordinary timed hard freeze saves the live `m_fHardFreezeRemaining`; a
  snapshot during a resume hold saves the remainder the hold protects. **Scope**:
  this changes the ordinary hard freeze for every mission: today the freeze
  countdown drains through it (`LL_GameModeCoop.c` lines 437-448 have no
  hard-freeze check), so a mission's freeze grows by its hard-freeze length;
  FR-016 and FR-022 say so, and Scenario 4 checks it with saves off.
- **The spectator countdown** (operator, 2026-09-19, FR-023): the mission-end
  timer keeps `m_fSecondsLeft` server-side, subtracts one per executed callback
  and replicates nothing (`LL_TriggerMissionEndTimer.c` lines 14, 38-63). A
  callback count and the mission clock diverge whenever the game does not
  advance the clock but the callback still runs (an empty unheld server, lines
  2101-2109 of the base game mode) or across fractional hold boundaries.
  Decision: the timer fires from the mission clock. Two `[RplProp]` scalars on
  the game mode, set once each on the server and bumped once:
  `m_iMissionEndDuration` (the timer's configured seconds, set when the timer
  registers; 0 when the mission has none) and `m_fMissionEndStartedAt` (the
  clock reading when the countdown began; -1 until then). The timer's tick
  fires when `GetElapsedTime() >= startedAt + duration`; its remaining time is
  that difference; `m_fSecondsLeft` goes away; the saved state is the start
  reading, kept on a restore. The spectator menu computes
  `duration - (elapsed - startedAt)` clamped to `[0, duration]` when started,
  shows `duration` held when not yet started, and falls back to the elapsed
  clock when the duration is 0. Because the clock is held during every hard
  freeze, the countdown stands still with it at no extra cost. Both values are
  in the session record so a resume restores them before any spectator
  connects. One such timer of positive duration per mission is the supported
  configuration (`LL_TriggerMissionEndTimer.c` lines 10-11 accept any value and
  each instance registers on its own, `LL_TriggerComponent.c` lines 46-57); the
  roster check logs more than one or a zero duration, and the first registered
  drives the display. Characters are already locked by the hard
  freeze (movement, view and weapon disable flags, gadget and inventory gates).
  Damage protection is extended: the hitzone gate that already zeroes damage
  during freeze also checks `IsHardFreezeActive()`, for every hitzone, AI included.
  The freeze-zone return countdown runs on the client
  (`LL_ZoneRestrictionComponent.c` lines 128-146) and reads the replicated hold
  flag to pause; the server kill handler (`LL_LobbyPlayerComponent.c` lines
  972-992) stays the validated decision. Marker placement is refused while held in
  both the client flow (`LL_MapMarkersUI.c` lines 139-164) and the server gate
  (`LL_M_SCR_MapMarkerSyncComponent.c` lines 17-44), neither of which checks the
  hard freeze today. Triggers and the mission-end countdown already stop on
  `IsHardFreezeActive()`; the freeze timer gains the same check.
- **Triggers not yet started**: a mission-end timer's activation only queues a
  wait for the freeze to end (`LL_TriggerMissionEndTimer.c` lines 22-25); the
  remaining seconds are zero until that wait completes and assigns the duration
  (lines 28-42); a contest sets its initial holder on activation
  (`LL_TriggerZoneContest.c` lines 118-126). A snapshot during the initial hard
  freeze, or between its end and the end of the freeze, must not restore those
  zeros as progress. Decision: the trigger record carries `started`, defined per
  trigger as "its own countdown or evaluation is running", which each subclass
  answers itself (the timer: the countdown tick is scheduled; the contest: the
  initial state has been set); a trigger that had not started restores nothing
  but its stat key and initialises normally; a started one restores its fields and
  skips the re-initialisation. The capture trigger's
  derived defender flag (`LL_TriggerZoneCapture.c` lines 61-62, 123-124) is
  restored with `captured`.
- **What the hold cannot do**: no script API pauses the AI world or the physics
  world (searched `AIWorld` and `Physics` generated interfaces). Mission AI may walk
  during the hold; it cannot damage or be damaged, and any vehicle it drives is
  stopped with the rest. Accepted limit, stated in the spec.
- **Vehicles on the ground**: at `ACTIVE` the server runs the game's pilot-dropped
  recipe on every vehicle in the world: `CarControllerComponent` → `Shutdown()`,
  `StopEngine(false)`, `SetPersistentHandBrake(true)`; `HelicopterControllerComponent`
  → `SetPersistentWheelBrake(true)`, `SetAutohoverEnabled(true)`, engine left as it
  is (`SCR_BaseGameMode.OnPlayerDisconnected`, lines 935-951; the forced engine stop
  on `BaseVehicleControllerComponent.c` lines 53-56 is documented for cinematics and
  is not used); `TrackedControllerComponent` has no persistent brake (only the car
  class declares one, `CarControllerComponent.c` lines 25-27) and gets `Shutdown()`
  and `StopEngine(false)` only, so a tracked vehicle on a slope may creep (accepted,
  the release logs any vehicle that moved more than a metre). These calls are
  server-side and engine-replicated. While the hold flag is set, a modded
  `OnBeforeEngineStart()` returns false, so nobody starts an engine, and the get-in
  and get-out actions are refused through the same `CanBePerformedScript` gate the
  crew lock already uses.
- **Freeze zones**: the vanilla restriction zones the lobby creates for the freeze
  register themselves on init (`SCR_EditorRestrictionZoneEntity.c` lines 77-99) and
  are removed only by the freeze-end path (`LL_GameModeCoop.c` lines 451-489); the
  zone prefab carries no persistence component, so they come back with the world
  regardless of the saved freeze time. The finaliser runs that same unregister-and-
  remove path when the restored freeze time is zero (the deletion-order lesson of
  the freeze-end deaths applies). The lobby polygon zone resets its state on
  possession (`LL_ZoneRestrictionComponent.c` lines 89-94) and arms its countdown
  only after entering the safe side (lines 128-145), so a player restored outside
  is enforced from their first entry, as any player who has not entered yet;
  accepted.
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
  pilot who drops mid-flight. Two caveats the spike must answer: the `Physics`
  interface has setters but no getters for the linear factor and damping
  (`Physics.c` lines 80, 97), so the original values are the engine defaults the
  spike records, not values read back; and the joint holder also corrects the
  transform every simulation step (`SCR_JointDummyHolderEntity.c` lines 26-35),
  which the pin may need too. Exit criteria of the spike: the aircraft holds
  position through the hand-over to the pilot's machine and back, through a
  release (it must actually be released and fly), through a second hold, with a
  passenger and no pilot, and with a damaged engine. This is the first task of the
  hold phase; if any criterion fails, the operator decides the fallback (spec edge
  case), the candidates being autohover only with its drift accepted, or ground
  placement at a defined safe spot with the crew kept aboard. Neither is chosen by
  the plan.
- **Alternatives considered**: `Physics.SetActive(ActiveState.INACTIVE)` (same
  ownership caveat, no vanilla caller at all); `NwkMovementComponent.EnableSimulation(false)`
  (vanilla uses it only on the editor camera and headgear items).

## R6. Saving: when, how, how heavy

- **Decision**: the engine's own autosave (server configuration, R9) does the
  saving; the addon gates it with `SetSavingAllowed(true)` on entering GAME and
  `SetSavingAllowed(false)` on leaving it, and additionally disallows while a body
  replacement is pending. The exclusion is synchronous, not polled: the counter is
  incremented and `SetSavingAllowed(false)` called before the replacement's first
  mutation (`LL_LobbyManager.c` line 826 spawns the new body, line 849 schedules the
  finish after 600 ms, lines 912-915 delete the old one; a 1 s poll could miss the
  whole window), and released exactly once per replacement on a terminal exit: the
  spawn-failure return (lines 826-831, before any finish is scheduled), the
  finish's early return when its new body is already gone (lines 876-877), the
  finish's success, its abandonment after the retry limit and its hand-off
  failure; the finish's registration retry (lines 888-891) is not an exit. A
  resource-load failure before the spawn (lines 801-805) happens before the
  acquisition and releases nothing. `SetSavingAllowed(true)` follows the release
  that brings the count to zero while in GAME. **Busy waits**: `IsBusy()` covers
  save creation, migration and deletion, and the manager raises
  `OnBusyStateChanged(bool)` (`SaveGameManager.c` lines 16, 76-78); the global
  after-save event fires for every save type including the shutdown one
  (`SCR_PersistenceSystem.c` lines 29-31), so it is not an idle signal and a
  waiter subscribed to it would re-fire on its own final save. Decision: every
  wait in the addon (a replacement asked for during a save, the debriefing
  discard, the stop-time save) is one `LL_SaveWaiter` object: it subscribes to the
  busy-state event and is also re-evaluated by the replacement release that
  brings the count to zero (finishing a replacement changes no manager state, so
  the busy event alone could leave a waiter asleep for ever); it runs its action
  the first time the manager is not busy and the replacement count is zero,
  unsubscribes before running it, and is dropped; the action itself never
  creates another waiter. A disallowed save is queued, not cancelled (lines
  24-28), and nothing cancels one in flight. **First save of a game phase**: the
  fresh-start entry only begins the body replacements (`LL_GameModeCoop.c` lines
  379-380; attachment and hand-off run later, `LL_LobbyManager.c` lines 846-915),
  so saving is not allowed until that entry has dispatched its whole batch and
  finished its setup (a flag set at its end) and the pending count is zero: the
  end of the entry checks once (covering nothing-to-replace and every-spawn-failed),
  and the release that brings the count to zero checks again; a zero reached
  inside the loop, before the flag, allows nothing; the resume finaliser sets the
  flag and re-allows the same way at its end,
  since it bypasses the state entry (`SetGameModeState` alone does not run
  `OnEnterState_S`, `LL_Modded.c` lines 6-14, `LL_GameModeCoop.c` lines 307-309).
  **The stop-time snapshot**: the addon request follows the game's own exit flow
  (`SCR_PauseMenuUI.c` lines 512-536): wait, request the `SHUTDOWN` save exactly
  once, and on its successful completion `SetSavingAllowed(false)` for the same
  reason the menu does ("do not trigger another shutdown save on exit
  transition", lines 530-536); if the after-save callback has already reported a
  completed shutdown-type save since the stop began, the addon does not request
  one. Exactly one stop-time snapshot either way. The only request flag is
  `BLOCKING` (`ESaveGameRequestFlags.c` lines 12-15). Whether the engine waits
  for that save before the process exits is a dedicated-server gate. No manual save, no addon command. Save duration is measured
  between `SCR_PersistenceSystem.GetOnBeforeSave()` and `GetOnAfterSave()` and
  logged with type and result.
- **Planned stop**: the save type `SHUTDOWN` is documented as "graceful application
  exit", `SCR_BaseGameMode.OnGameEnd()` is the game's own "right before game end"
  hook (`game.c` lines 758-772 forward the engine event to it) and reaches the
  game-mode components, and the startup reference's `-autoshutdown` "ensures the
  correct server shutdown process". A close signal on the server process is
  therefore expected to produce a shutdown save on its own. As a second line the
  lobby requests a `BLOCKING` `SHUTDOWN` save from `OnGameEnd` when in GAME and no
  save is in flight, the shape of `SCR_PauseMenuUI.c` lines 518-537 minus the
  menu. The quickstart verifies the save and measures how long the process takes
  to exit; that number becomes the container's stop grace period. An OS kill
  remains a crash: the last scheduled snapshot applies.
- **Enabling**: the mission header's save-type flags gate everything
  (`game.c OnMissionSet` → `SetEnabledSaveTypes`), default 15 = all. With the
  game-mode attribute off, `OnGameStart` calls `SetEnabledSaveTypes(0)`, which the
  header's own description says disables the whole persistence system, so an
  unconfigured mission writes nothing.
- **Cost**: **unverified**. The `BLOCKING` flag means "save all immediately"; the
  non-blocking form spreads the work, but how much main-thread time a save of a
  127-player world takes is engine-side. The quickstart measures it at scale; the
  server owner tunes `autoSaveInterval` from that measurement.

## R7. Which save is resumed, and how the server finds it

- **Decision** (operator, 2026-09-18): a plain restart is fresh, a resume is
  explicit. The server configuration sets `loadSessionSave: false` (R9), and
  whoever restarts the server adds the game's `-loadSessionSave` startup parameter
  to continue. The official startup-parameter reference (game version 1.6 and
  later) documents it for `ArmaReforgerServer.exe`: used alone it "will attempt to
  locate the latest save game data for the current scenario on launch"; with a UUID
  argument it loads that specific save point, the UUID being "found inside each
  save point's `meta-info.json` file". That second form is the rollback: any of the
  retained snapshots can be chosen. `-keepSessionSave` is not passed. Script sees
  the result through `SaveGameManager.GetActiveSave()` and
  `PersistenceSystem.WasDataLoaded()`; `SCR_PersistenceSystem.IsLoadInProgress()`
  wraps both.
- **Rationale**: the automatic alternative (resume whenever a snapshot exists, with
  an age cut-off and a chat command to discard) was rejected by the operator as
  needing commands and a heuristic. Explicit selection needs neither, keeps the
  decision with the person who already restarts the process, and gives rollback.
- **What remains to confirm**: that the launch parameter takes precedence over
  `loadSessionSave: false` in the config file (documented separately, precedence
  not stated). Quickstart Scenario 0 checks it first. If it does not, no addon
  fallback is built: a `$profile:` choice file was considered and rejected because
  nothing consumes it after one start, so a plain restart would resume again. The
  operator's alternative needs no code: `loadSessionSave` flipped to true in the
  configuration for that one start (newest snapshot only; a named snapshot then
  needs the parameter to work). The result of Scenario 0 goes back to the operator
  with that option.
- **Operator steps**: `loadSessionSave: false` in `config.json`; `-loadSessionSave`
  or `-loadSessionSave <uuid>` on the launch line when a resume is wanted. The
  event server today runs without either.
- **Selecting the persistence config**: the mission header's world-systems config
  field is the only route on a shipped server; the `-worldSystemsConfig` parameter
  exists but the reference marks it "only works on diag exe" and "when starting a
  mission directly (not via MissionHeader.conf)".
- **Completeness, unverified**: `SaveGame` exposes version and addon compatibility
  queries (`SaveGame.c` lines 42-48) and no completeness flag; the manager promises
  only that a failed overwrite keeps the old data (`SaveGameManager.c` lines
  43-50), which is narrower than "a crash inside a save leaves the previous save
  point loadable". Three engine behaviours are therefore **unverified** and gated
  by quickstart scenarios rather than assumed: that a crash-interrupted newest save
  is skipped for the previous one (FR-013), that a changed addon version is refused
  (FR-014), and that a saved seat whose vehicle is absent places the character on
  the ground (spec edge case). The lobby's own record refuses an unknown version
  regardless.
- **Discard**: `SetSavingAllowed(false)`, then
  `PersistenceSystem.ClearStorage(PersistenceSessionStorage)` and
  `SaveGameManager.Purge(mission, playthrough)` of the active save's playthrough
  only, the body of the protected `SCR_BaseGameMode.HandleOnGameModeEndSaveData`,
  which a subclass can call directly. Called on DEBRIEFING only, after any save in
  flight has finished; afterwards the phase gate keeps saving disallowed for the
  rest of the session. Never called on a refused resume (R4). There is no discard
  command: a game the admins abandon is ended the normal way, by advancing to
  debriefing, and a snapshot nobody asks for is never loaded anyway.

## R9. The server configuration already drives most of this (read 2026-09-18)

The official server-configuration reference has a `persistence` block (1.6.0),
"set up to automatically work for most use cases by default":

| Key | Type, range, default | Meaning |
|-----|----------------------|---------|
| `autoSaveInterval` | number, 0..60, default 10 | minutes between automatic saves, "if possible for the current mission"; 0 disables |
| `saveRetention` | number, 1..128, default 10 | save points kept for the current mission |
| `loadSessionSave` | bool, default true | "Automatically load the latest available save point on first startup" |
| `keepSessionSave` | bool, default false | keep the playthrough's save points after the mission is finished |
| `hiveId` | number, 0..16383, default 0 | separates UUIDs when several servers share one database |
| `databases` / `storages` | named objects | override or add database presets (a `preset` resource name plus `options`) and re-point storages |

Persistence is disabled entirely by overriding the mission header from the same
file: `game.gameProperties.missionHeader.m_eSaveTypes: 0`.

Consequences for the plan:

- **Loading is a config default the lobby turns off.** `loadSessionSave` is on by
  default, which would resume on every restart; the operator wants restarts fresh,
  so the config sets it to false and the `-loadSessionSave` parameter (R7) is the
  explicit resume.
- **The engine has a periodic autosave.** Every ten minutes by default, capped at
  sixty. The addon's own save timer (R6) would be a second, competing autosave.
  The addon does not time saves at all: it gates them by phase with
  `SetSavingAllowed` (GAME only), and the cadence is server configuration. The
  interval attribute was dropped from the spec (operator decision, 2026-09-18).
- **Retention and multi-server are solved.** `saveRetention` prunes old save
  points; `hiveId` answers the spec's two-servers edge case.
- **A readable save exists for debugging.** The game ships a JSON save-game
  database preset next to the binary one; the `databases` override selects it for
  a test server so a save point can be opened in a text editor.
- **Why the event server did not save on 2026-09-13.** Its log shows
  `[SaveGameManager] Starting new playthrough nr.0` for the mission and no save in
  47 minutes, with the engine defaults in force. The mission's header names no
  world-systems config, so no persistence system is registered and "if possible for
  the current mission" is false. The lobby's config chain (R2) is what makes saves
  possible; from that moment every mission using it autosaves by engine default
  unless the game-mode switch turns the save types off (FR-016).

## R8. What a resume cannot restore (accepted, from the spec)

Velocity of anything, towed and slung loads, small destructibles (no serializer for
`SCR_DestructibleEntity`, multi-phase destruction, tyres, glass), wrecks, fires,
craters, debris, projectiles, ragdoll pose, the ownership of player-placed markers,
AI and physics pause during the hold. The spec's edge cases state them; the plan
does nothing about them.
