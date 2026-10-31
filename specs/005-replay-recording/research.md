# Research: Replay Recording

Every fact below was read from `Arma-Reforger-Script-Diff` at v1.8.0.13 (59 script files
differ from v1.8.0.10, none of the APIs cited here) or from the addon's own scripts.
What the scripts cannot prove is marked **unverified** and carried as a spike (S1 to S6)
that the operator runs in the demo world on a dedicated server before the code that
depends on it is written.

## R1. Recording format: text lines

- **Decision**: UTF-8 text, one record per line, a record type and space-separated
  fields; strings as JSON string literals. Full grammar in
  [contracts/recording-format.md](contracts/recording-format.md).
- **Rationale**: the game's own built-in recorder, `SCR_GameModeStatistics`, writes
  semicolon-separated text lines from a 1 Hz sampler with a buffered flush
  (`scripts/Game/GameMode/Statistics/SCR_GameModeStatistics.c:449-515`,
  `SCR_BaseGameMode.c:1939-1987`). A line format makes a torn tail harmless (the last
  incomplete line is dropped), lets readers skip record types they do not know
  (FR-020), and lets a person read the file (SC-012). Compressed for sending, text
  costs little more than binary.
- **Alternatives considered**: fixed-width binary records written with
  `FileHandle.Write(int, 4)` (unreadable, a torn record cannot be detected, an unknown
  record type desynchronises the reader); the engine's `BinarySaveContext` with value
  compression (a proprietary layout no website can read); one JSON document per file
  (cannot be appended, every save rewrites the whole file).

## R2. Durability without flush or rename

- **Decision**: records are built into an in-memory list and written every 9 seconds:
  `FileIO.FileExists` decides between `FileMode.WRITE` (create) and `FileMode.APPEND`,
  every line goes out with `Write(line + "\n")`, then `Close()`. A flush writes at most 300
  lines per 100 ms tick and continues on the next tick, so a large buffer (after a
  snapshot) never lands in one frame. A failed open stops the recording (FR-027).
- **Rationale**: `FileHandle` has no flush (`scripts/Core/generated/System/FileHandle.c:68-132`);
  data reaches the disk on close. A 9-second cadence keeps a record's way to the disk
  inside FR-018's 10 seconds even with a chunked flush, at about 0.1 opens per second. The exists-then-create-or-append order is the
  game's own `SCR_FileIOHelper.AppendFileContent` (`scripts/Game/Helpers/SCR_FileIOHelper.c:360-433`),
  because creation by `APPEND` is not documented. `WriteLine` appends CR LF; `Write` with
  `"\n"` keeps one line ending. There is no rename (`SCR_FileIOHelper.RenameFile` is copy
  plus delete), so a file is known to be final by its end record or by its server having
  stopped (R17), never by its name.
- **Alternatives considered**: one handle kept open for the session, as the game's debug
  recorder does (a crash loses everything still in engine buffers); open and close per
  record (hundreds of opens per second at 127 players).

## R3. Sampling

- **Decision**: one repeating call-queue tick every 100 ms. Each tick checks the next
  tenth of the characters and the next fifth of the vehicles and trackers, so every
  character is checked once per second and every vehicle twice. A record is written when
  the rounded position moved at least 1 metre on an axis or the heading turned at least 5
  degrees since the entity's last record. Positions are integer metres, heading integer
  degrees. Sampling pauses while a hard freeze holds the mission clock. A snapshot is built
  over consecutive ticks, at most 100 entities' records per tick, and the records produced
  meanwhile are held and written after `SNAP-END`, so a snapshot of a full server never
  lands in one frame (SC-001's 2 ms per step).
- **Rationale**: the addon's tracker sampler already uses the 1 metre / 5 degree test on
  rounded metres (`scripts/game/Markers/LL_TrackerManager.c:256-304, 365-367`); it runs
  its whole pass in one 250 ms tick, which at 127 players plus AI and vehicles would put
  every check into one frame, so the recorder spreads the pass over ten ticks
  (FR-026). Metres are enough for a map replay and keep lines short.
- **Alternatives considered**: a fixed write of every entity every half second, moved or
  not (tens of megabytes per game, corpses and parked vehicles written forever);
  per-frame sampling (no need at map scale).

## R4. Characters

- **Decision**: `SCR_CharacterRegistrationComponent.GetChimeraCharacters()` is walked by
  the sampler; a character seen for the first time gets a recording id, its faction
  (`SCR_ChimeraCharacter.GetFaction()`), its AI group if it has one (`AIAgent.GetParentGroup()`,
  read at first sight because AI is deactivated when unconscious), and subscriptions to its
  life-state, seat and shot events. A character no longer in the list is recorded as
  removed.
- **Rationale**: the list is a static array maintained by the game for every character
  including AI and corpses (`scripts/Game/Components/SCR_CharacterRegistrationComponent.c:8-59`),
  so no world query is needed.

## R5. Vehicles

- **Decision**: `SCR_EditableEntityCore`: `GetAllEntities` once at recording start, then
  `Event_OnEntityRegistered` and `Event_OnEntityUnregistered`, keeping entries whose
  `GetEntityType()` is `VEHICLE`. Subscriptions are redone per world, since the invokers
  are re-created at world cleanup.
- **Rationale**: the core exists on a dedicated server (`scripts/Game/game.c:557-560`) and
  the game's own server code enumerates and subscribes to it the same way
  (`SCR_DynamicSimulationEditorComponent.c:39, 88-95`). Vehicles and static weapons and
  mortars all register as `VEHICLE`, world-placed and spawned alike (`Vehicle_Base.et:56`,
  `Default_SCR_EditableEntityComponent.ct:4`); turrets mounted on vehicles have no
  editable component, so nothing is counted twice. Wrecks stay registered until deleted.
- **Unverified (S4)**: a modded vehicle that does not inherit the game's vehicle base would
  be missing. The spike compares the registered count at game start with the vehicles
  placed in the demo world.
- **Alternatives considered**: `SCR_BaseGameMode.GetOnControllableSpawned/Deleted` (not
  proven to fire for vehicles); the addon's own squad-vehicle links (only squad vehicles).

## R6. Vehicle kinds and display data

- **Decision**: the kind comes from `SCR_AIVehicleUsageComponent.GetVehicleType()`
  (helicopter, plane, static weapon, artillery, wheeled, tracked), refined for wheeled
  vehicles by `Vehicle.m_eVehicleType` (car, truck, APC) and for unclassified ones by the
  root class (`Turret` is a static weapon); the table is in
  [data-model.md](data-model.md). The display name is the entity's display name; the
  prefab's file name is recorded beside it.
- **Rationale**: the AI usage component is set on every vanilla vehicle base
  (`Wheeled_Base.et:49-50`, `Helicopter_Base.et:21-22`, `Tripod_Base.et:20`,
  `Mortar_Base.et:33`); `EVehicleType` alone has no helicopter or tracked value; tracked
  vehicles read as unclassified and fall back to `tracked` through their simulation.

## R7. Vehicle seats

- **Decision**: per character, `SCR_CompartmentAccessComponent.GetOnCompartmentEntered/Left`;
  the seat role from `FindCompartment(slotID, mgrID).GetType()` (`PILOT` driver, `TURRET`
  gunner, `CARGO` passenger) and the seat's unique name beside it, which is how a
  commander seat is told apart (the BTR-70's is a turret named `CommanderTurret`).
- **Rationale**: these invokers fire on the server for players (server-only data-collector
  modules subscribe, `SCR_DataCollectorDriverModule.c:21-33`) and for AI (agent components
  subscribe, `SCR_AIInfoComponent.c:145-146`); the game has no commander seat type.

## R8. Deaths and knock-outs

- **Decision**: the recorder overrides the game-mode component hook
  `OnControllableDestroyed(SCR_InstigatorContextData instigatorContextData)`, as the
  statistics do. Deletion relations (`DELETED`, deleted by the editor) record a removal,
  never a death. The cause is the damage type of the newest damage-history entry that is
  not small bleeding, read inside the callback because the history is cleared 1.5 s after
  death (`SCR_CharacterDamageManagerComponent.c:684-699`). The weapon is the attacker's
  weapon at its last recorded shot when that shot was less than 10 seconds before,
  otherwise its current weapon (for a gunner, the turret's weapon manager found through
  the compartment controller, `SCR_CharacterControllerComponent.c:1149-1161`). The distance
  is between attacker and victim. A knock-out is the life-state change to
  `INCAPACITATED` (`m_OnLifeStateChanged`, proven server-side in
  `SCR_ExtendedCharacterIdentityComponent.c:412-443`); its attacker is the damage
  manager's last instigator, which the game keeps as "the cause of unconsciousness"
  (`SCR_CharacterDamageManagerComponent.c:2009-2019`).
- **Rationale**: the game's own kill log walks the damage history the same way
  (`SCR_BaseGameMode.c:1050-1202`); no vanilla code derives a weapon, and the weapon of the
  last shot is right more often than whatever is held at death (a grenade thrown, then a
  rifle drawn). `OnControllableDestroyed` also fires for vehicles
  (`SCR_DataCollectorComponent.c:375-382`).
- **Unverified (S3)**: whether the instigator is set before the life-state callback runs.
  The spike reads it at once and one frame later.

## R9. Shots

- **Decision**: on each recorded character, `EventHandlerManagerComponent.RegisterScriptHandler`
  for `OnProjectileShot` and `OnGrenadeThrown`, callback
  `(int playerID, BaseWeaponComponent weapon, IEntity projectile)`. Shots are counted per
  shooter and written once per shooter per second as a summary: count, the direction of the
  first projectile's velocity (or the shooter's heading when the projectile is already
  gone), and the weapon. The weapon is also remembered for R8.
- **Rationale**: the game registers exactly these events server-side on player characters
  (`SCR_DataCollectorShootingModule.c:8-41`, registered at spawn by
  `SCR_DataCollectorComponent.c:330-337`); turret fire is raised on the gunner's handler
  (`SCR_AnimatedBeltComponent.c:53-58`).
- **Unverified (S1)**: whether these events fire for AI characters on the server. Fallback
  if they do not: `OnAmmoCountChanged` on the same handler, counting decreases of the
  magazine as shots (registered server-side by `SCR_SpawnProtectionComponent.c:59-63`).
  The fallback loses the projectile, so AI directions use the shooter's heading.
- **Alternatives considered**: `SCR_MuzzleEffectComponent.GetOnWeaponFired()` (proven for
  AI on the server, but per weapon instance: every pick-up would need a new subscription);
  an impact trigger on the base bullet (rejected by FR-025: it silences rounds that have
  their own trigger).

## R10. Explosions

- **Decision**: three sources.
  1. `SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect()`, subscribed on the server and
     removed at teardown (the invoker is static): the exact detonation point
     (`outMat[0]`) and the instigator. It covers the 40 mm grenades M433, M406 and VOG-25,
     the rockets M72A3, PG-7VM, RPG-75, PG-22, Hydra 70 and S-5, and the 81 mm and 82 mm
     mortar rounds (`scripts/Game/Effects/ProjectileEffects/SCR_ExplosionAmmoEffect.c:12-25`;
     server use in `SCR_FireSupportRequestedTaskEntity.c:103-116`).
  2. Thrown grenades: the grenade entity from `OnGrenadeThrown` is followed until its
     reference reads null; its last position is the detonation point.
  3. Rockets the effect does not cover (PG-7VL, PG-7VR, any launcher round from a
     mission's own addon): projectiles from `OnProjectileShot` whose weapon type is a
     rocket or grenade launcher and that did not raise source 1 within their flight are
     followed the same way. While anything is followed, positions are read every frame;
     when a reference reads null, a short trace from the last position along the last
     velocity refines the detonation point.
  At most 32 objects are followed at once; beyond that, new ones are not followed.
- **Rationale**: hand grenades and several rocket types have no script-visible detonation
  (`Grenade_M67.et:9-20`: a native timer trigger). Script references to deleted entities
  read as null, which the game itself relies on (`SCR_BaseTriggerComponent.c:93-96`).
- **Unverified (S2)**: that grenades and rockets are deleted at detonation, and how close
  the refined point lands. Placed mines and charges, autocannon and heavy machine-gun
  high-explosive or incendiary rounds are not recorded (FR-011 leaves rounds without a
  server-visible detonation out).

## R11. Map markers

- **Decision**: every second the recorder reads `GetStaticMarkers()` and
  `GetDisabledMarkers()` from the marker manager and compares them with the previous read
  by marker id: new ids are recorded as placed, missing ids as removed. An edit is a
  removal and a placement, as the addon's marker dialog already does it
  (`scripts/game/Map/LL_MapMarkersUI.c:386-424`).
- **Rationale**: both methods return copies (`SCR_MapMarkerManagerComponent.c:48-70`), the
  dedicated server holds every marker, and one diff catches player placements, server
  placements and expiries in one place, where hooks would miss server-side removals. Side
  visibility is the marker's faction bit mask (0 is everyone), resolved to faction keys on
  the server. Icon and colour indices are resolved to the addon's placed-marker entries
  (`scripts/game/Markers/LL_PlacedMarkers.c:72-111`).
- **Note for the receiving website**: marker text is kept on the server as typed; the game
  filters profanity only on clients (`SCR_MapMarkerManagerComponent.c:534-568`). A public
  viewer should filter it too.

## R12. Map areas, manual markers, trackers, trigger zones

- **Decision**:
  - `LL_MapZoneComponent` and `LL_ManualMarker` get a server-side static registry in the
    shape of `LL_StatZoneComponent`'s (`scripts/game/Zones/LL_StatZoneComponent.c:16-42`)
    and public getters for what they draw. A map area's label is its shape entity's name,
    as for statistics zones. Its outline is the polygon the component itself draws, so the
    replay shows what players saw. Its visibility over time comes from a new read-only
    method, `IsShownNow()`, that evaluates the same phase and timing conditions as the
    drawing (hidden phases, hide at freeze end, hide delay) without the faction part; the
    drawing code itself is not touched, because Principle I keeps working production code
    as it is. The recorder evaluates it once per second and records each change.
  - Trackers already have a server registry (`LL_TrackerComponent.GetAll`,
    `scripts/game/Markers/LL_TrackerComponent.c:77-125`); each becomes a recorded entity of
    kind `tracker`, sampled like a vehicle, with its label and visible sides from
    `ExportConfig`.
  - `LL_TriggerZoneBase` gets a public getter that builds its outline on demand from its
    zone entity with `LL_TriggerComponent.BuildPolygon`; it is built only at activation
    today (`scripts/game/Triggers/LL_TriggerZoneBase.c:32-44`). Captures and holder changes
    are recorded next to the existing statistics calls
    (`LL_TriggerZoneCapture.c:169-171`, `LL_TriggerZoneContest.c:300-302`), every trigger
    firing inside `LL_TriggerComponent.Fire()` after its latch and outside the statistics
    branch (`LL_TriggerComponent.c:122-145`).
- **Found while reading, not changed here**: `LL_MapZoneComponent` and
  `LL_ZoneRestrictionComponent` place points as origin plus local offset, which ignores the
  shape's rotation, while `BuildPolygon` uses `CoordToParent` and its comment says the
  former misplaces rotated shapes. The recorder follows what the map draws. The operator
  decided on 2026-10-31 to fix the map zones in a separate change after this feature.

## R13. Roster, players, identities

- **Decision**: the roster comes from `LL_LobbyManager.GetSlots()` and its invokers (slot
  registered, unregistered and updated, player assigned and unassigned, name updated,
  connection changed). A slot is identified by its sort key, which survives a body
  replacement; a slot's squad by its group id and callsign. Each player gets a recording
  reference keyed by identity (`GetReconnectKeyForPlayer_S`), so a reconnect with a new
  player id keeps the reference. Display names come from `GetPlayerName`. Who controls
  which character is polled once per second through `PlayerManager.GetPlayerControlledEntity`.
  The reference-to-identity pairs go to the identity file only.
- **Rationale**: the slot record's network id changes with every fresh body
  (`scripts/game/Data/LL_SlotData.c:6-33`); the player id changes on every reconnect
  (`LL_LobbyManager.c:2380-2437`). The connection invoker also fires on an admin login with
  `connected=true` (`LL_LobbyManager.c:2449-2459`), so the recorder records only actual
  changes of state.

## R14. Session identity

- **Decision**: the recorder takes `LL_StatsManager.GetInstance().GetSessionId()` when it
  is not empty; otherwise it mints its own identity in the same format (local start date
  and time plus world slug), once per loaded world. It resolves the identity one frame
  after its own initialisation, when every component's `OnPostInit` has run. The
  statistics code is not changed.
- **Rationale**: the statistics mint their identity at server start only when they are
  enabled (`scripts/game/Stats/LL_StatsManager.c:104-111, 269, 1605-1627`); reading it
  gives both file families the same identity, and minting the same format otherwise keeps
  recordings on servers without statistics.
- **Alternatives considered**: moving the minting into a helper shared with the
  statistics (rejected: it rewrites working production code, against Principle I).

## R15. Crash-resume and segments

- **Decision**: a separate persistent state, `LL_ReplayData` with `LL_ReplaySerializer`,
  registered in `Configs/Systems/Persistence/LL_Persistence.conf` next to the trigger and
  marker states; it stores the recording's identity, its segment number, the mission time
  of the snapshot, the next free entity, weapon and player numbers, and the pairs of
  persistent entity id and recording id. `LL_GameModeCoop.CompleteResume_S` hands it to the
  recorder next to `stats.ResumeRecording_S`, after the mission clock is restored. The
  recorder opens the next segment file, whose header names the resumed mission time, and
  writes a full snapshot. A clean server stop writes a stop record to the segment; only
  debriefing writes the end record.
- **Rationale**: the session state has a strict version check that refuses older
  snapshots (`scripts/game/Persistence/LL_SessionData.c:187-203`); the trigger and marker
  states show the tolerant shape (`LL_TriggerData.c:34-109`, `LL_MarkerData.c:32-150`), and
  a missing record is already a working case. A clean stop is not the end: the session
  resume feature can continue a game from its shutdown save.

## R16. Game-flow hooks

- **Decision**: the recorder starts when it sees GAME in its `OnGameStateChanged` and no
  resume is pending (the statistics' own test, `LL_StatsManager.c:255-267`), and ends at
  DEBRIEFING. Direct calls from `LL_GameModeCoop` record: freeze countdown start
  (`ScheduleFreezeCountdown_S`), freeze end (`OnFreezeTimeEnded_S`, which can run twice when
  an admin revives a lapsed freeze), admin freeze changes (`AdjustFreezeTime_S`,
  `EndFreezeTime_S`), hard freeze start, end and changes (`StartHardFreeze_S`,
  `EndHardFreeze_S`, `AdjustHardFreeze_S`), and the server stop (`OnGameEnd`).
- **Rationale**: none of these has an invoker today; a direct call is the simplest link
  and matches how the statistics are fed.

## R17. Server status file and the end of a game

- **Decision**: while configured, the recorder keeps a one-line status file
  (`$profile:LL_Replays/status.txt`), rewritten at server start and with every flush: idle,
  resume pending, recording a given session and segment, or done. The service treats a
  game as over when its last segment has the end record; or when the status file, written
  after the game's last record, shows a different session; or when neither the status file
  nor the game's files changed for the configured wait. A pending resume holds every
  unfinished game.
- **Rationale**: at server start the statistics session identity is a fresh one until the
  resume completes (`LL_StatsManager.c:347-348`), so a new identity alone does not mean a
  different game; the pending-resume state tells the service to wait.

## R18. Addon version in the header

- **Decision**: the header carries the recording format version only. **This amends
  FR-004** and needs the operator's confirmation.
- **Rationale**: no API returns an addon's version at runtime (`GameProject` offers GUIDs,
  id and title only, `scripts/GameLib/generated/GameProject.c:17-26`). A constant in the
  scripts would duplicate the version the changelog already holds, against "one source per
  fact". The format version is what a reader needs to parse a file.
- **Alternatives considered**: a script constant bumped with every changelog entry.

## R19. Names in a recording

- **Decision**: entity display names, role names, weapon names and seat names are recorded
  as the game provides them on the server, which can be localization keys such as
  `#AR-Vehicle_Mi8MT_Name`, with the prefab's file name beside every entity as a readable
  fallback. Player names are recorded as displayed (`[TAG]Callsign`).
- **Unverified (S5)**: whether a dedicated server can translate keys. If it can, the
  recorder writes the English text instead of the key.

## R20. The `/replay` command

- **Decision**: the `/verify` path: client registration and admin check in
  `LL_LobbyPlayerComponent`, `RpcAsk_ReplayStatus` re-checked with `SCR_Global.IsAdmin` on
  the server (a non-admin gets nothing back), and an owner reply `RpcDo_ReplayStatus` with
  the state and five numbers, which the client turns into translated lines through
  `ShowCommandHelp` (`LL_LobbyPlayerComponent.c:1160-1281, 1445-1516`).

## R21. Instrumentation

- **Decision**: the recorder times every flush step and every sampling tick with the
  system tick count and logs one summary line per snapshot (every five minutes): records by
  type, bytes written, the summed tick and flush milliseconds of the period and their
  average per second, the longest flush step and the longest tick. Frame cost at scale is
  read from the server's `-logStats` frame-time lines with recording on and off.
- **Rationale**: Principle V asks for measurement before claims. The tick count has
  millisecond resolution, so the summed figure undercounts steps shorter than a
  millisecond; SC-001 reads it together with the frame-time comparison, which has no such
  floor.

## R22. The host service

- **Decision**: a Go program, `ll-uplink`, in `uplink/`, using the standard library only,
  built as one static Linux binary. One process serves one game server, so two servers on a
  host run two processes with their own configuration and state (FR-038). It polls the
  profile folders every 15 seconds, keeps its own state file (written to a temporary file
  and renamed), logs to standard output and never listens.
- **Rationale**: a single binary with no runtime to install suits a game host; the
  standard library covers HTTPS, HMAC, SHA-256, gzip and JSON, so there is no third-party
  code to audit. Polling avoids a file-watching dependency and costs nothing at this rate.
- **Alternatives considered**: a shell script with a timer (state, retries, signing and
  rate control become fragile); uploading from the game itself (no compression, request
  bodies under 1 MB, and the game would carry the retry logic).

## R23. Upload protocol

- **Decision**: [contracts/upload-protocol.md](contracts/upload-protocol.md): per-request
  HMAC-SHA256 over method, path, timestamp and body hash; a 300-second window; content
  identity by SHA-256 of the uncompressed file; gzip pieces of at most 1,500,000 bytes;
  idempotent piece, commit and game requests; `429` honoured.
- **Rationale**: a receiver can check every header before reading a body (FR-031); pieces
  stay below the 2 MB body limits receivers commonly run; content identity makes resends
  harmless (FR-032).
- **Where it is documented**: in the code that speaks it (Principle VII): the package
  documentation of `uplink/internal/send` states the endpoints, headers, signing string,
  limits and JSON shapes in a few lines and points to the contract for the full reference.

## R24. Compression

- **Decision**: gzip.
- **Rationale**: every browser decompresses gzip natively, so a viewer can read a stored
  file as is; the Go standard library writes it.
- **Alternatives considered**: zstd and brotli (not decodable natively by every browser).

## R25. Rate cap

- **Decision**: a token bucket on body bytes, default 8,000 kbit/s, configurable.
- **Rationale**: a finished two-hour recording of a few megabytes goes out in seconds
  (SC-008) while the cap stays a small fraction of a game host's uplink.

## R26. Metrics

- **Decision**: an optional metrics file in the Prometheus text format, rewritten after
  every pass through a temporary file: last successful upload time, files and bytes
  waiting, failures by kind.
- **Rationale**: the node exporter's text-file collector, common on game hosts, reads such
  files with no configuration (FR-036).

## R27. Build and release of the service

- **Decision**: a workflow `.github/workflows/uplink.yml` runs `gofmt`, `go vet`,
  `go build` and `go test` on changes under `uplink/`, and builds the static Linux binary
  for a release when the operator publishes one.
- **Rationale**: the same four checks every Go change must pass; agents never tag or
  publish (constitution, Development Workflow).
