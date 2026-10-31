---

description: "Task list for Replay Recording"
---

# Tasks: Replay Recording

**Input**: Design documents from `/specs/005-replay-recording/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md,
contracts/recording-format.md, contracts/upload-protocol.md, contracts/config-files.md,
contracts/admin-command.md

**Tests**: there is no automated harness for game code; the game side is validated by
the quickstart, run by the operator on a dedicated server. The host service carries Go
tests (plan, Testing), which are part of its tasks. Gates: the spikes (T010, T018,
T022) are run before the code that depends on them is finished; a failed spike takes its
planned fallback, recorded in research.md.

**Organization**: tasks are grouped by user story. Paths are relative to the repository
root; game scripts live under `Lite Lobby/scripts/game/`. "Operator" tasks are steps only
the operator performs (Workbench, server configuration, live runs); the agent hands over
exact values and waits.

**Operator decisions** (2026-10-31, plan.md Operator steps): D1, the header carries the
format version only and FR-004 is amended (task T052, done). D2, the server-owner setup
text lives in `uplink/README.md` (task T044). D3, the map-zone rotation finding is fixed
in a separate change after this feature (T057).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1 to US5)

---

## Phase 1: Setup

**Purpose**: the recorder's session identity rule and the empty Go module with its CI.

- [X] T001 [P] Create `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c` with the component's class declarations (completed in T005) and `protected string ResolveSession_S()`.
  - **Done:** the minted identity is cached on the component, which lives for one loaded world.
  - It returns `LL_StatsManager.GetInstance().GetSessionId()` (line 269) when the instance exists and the identity is not empty.
  - Otherwise it mints, once per loaded world, an identity in exactly the format of `LL_StatsManager.MintSessionIdentity` (`Lite Lobby/scripts/game/Stats/LL_StatsManager.c:1605-1627`): local `System.GetYearMonthDay`/`GetHourMinuteSecond`, `GetGame().GetWorldFile()` basename without extension as the slug, `"%1%2%3-%4%5%6-%7"` with two-digit fields. It caches the result with the `BaseWorld` it was minted for, because statics survive a world reload.
  - The statistics code is not changed (Principle I).
  - One WHY comment: statistics and replay files of one game must carry the same identity (research R14).
- [X] T002 Read `Lite Lobby/scripts/game/Stats/LL_StatsManager.c` and confirm the identity rule of T001 holds:
  - **Done:** confirmed and recorded in R14.
  - the identity is minted in `OnPostInit` only when statistics are enabled (lines 104-111);
  - `GetSessionId()` returns `""` otherwise;
  - the resume path overwrites it in `ResumeRecording_S` (lines 347-348) before the recorder resumes (T034).

  Record the result in research R14; no edit to the file.
- [X] T003 [P] Create the Go module: `uplink/go.mod` (`module github.com/Jumbo13th/lite-lobby-ar/uplink`, `go 1.27`, no `require`), folders `uplink/cmd/ll-uplink/` and `uplink/internal/{config,scan,send,state,metrics}/`, and `.github/workflows/uplink.yml`. The workflow runs on pushes and pull requests touching `uplink/**` or itself, with `actions/setup-go` reading `uplink/go.mod`: `test -z "$(gofmt -l .)"`, `go vet ./...`, `go build ./...`, `go test ./...` in `uplink/`. A second job on `release: published` builds `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -trimpath -ldflags "-s -w -X main.version=${{ github.event.release.tag_name }}" -o ll-uplink-linux-amd64 ./cmd/ll-uplink`, writes its SHA-256 to `ll-uplink-linux-amd64.sha256`, and attaches both to the release (`contents: write` for that job only, `contents: read` otherwise) (research R27).
  - **Done:** the workflow uses `actions/checkout@v7` and `actions/setup-go@v7`, the current majors; the release tag goes through an environment variable, and `--clobber` lets a re-run replace the assets.

**Checkpoint**: the recorder's identity rule exists and the statistics code is untouched; `uplink/` builds empty in CI.

---

## Phase 2: Foundational

**Purpose**: the writer, the component lifecycle and character sampling, which every story needs. The first spikes run here.

- [X] T004 Create `Lite Lobby/scripts/game/Replay/LL_ReplayWriter.c`, class `LL_ReplayWriter` (server only), per contracts/recording-format.md:
  - **Done:** `Open` and `Flush_S` are `Begin` and `FlushStep_S` (true once the buffer is on disk); the recorder rewrites the status file after each completed flush.
  - **Formatters:** `static string Str(string s)` writes a JSON string literal: escape `\`, `"`, LF, CR and tab; every other character below U+0020 becomes a space. `static string Tok(string s)` replaces spaces and double quotes with `_` and returns `-` for empty. `static string IntOr(int v, bool present)` returns `-` when absent.
  - **Buffer:** a line buffer `ref array<string>`, `void Add(string line)`, and `void SetTime(int t)`, which adds a `T` line only when `t` differs from the last one written.
  - **Open:** `bool Open(string session, int segment)` calls `FileIO.MakeDirectory("$profile:LL_Replays")` and sets the path `$profile:LL_Replays/<session>-<segment>.llr`.
  - **Flush:** `bool Flush_S()`, called by the recorder's tick while a flush is due:
    - open with `FileMode.WRITE` when `FileIO.FileExists` is false, otherwise `FileMode.APPEND` (the pattern of `SCR_FileIOHelper.AppendFileContent`);
    - write at most 300 buffered lines with `Write(line + "\n")`, then `Close()`, and remove them from the buffer; a buffer with more lines left keeps the flush due for the next tick;
    - the steps below run once the buffer is empty;
    - rewrite `$profile:LL_Replays/status.txt` with `FileMode.WRITE` as `LLS 1 <state> <session|-> <segment|-> <System.GetUnixTime()>`;
    - append any pending identity lines to `$profile:LL_Replays/<session>-identities.txt`, creating it with `LLI 1` and `H session <session>` first.
  - **Identities:** `void AddIdentity(int pref, string uuid)` queues an `I` line.
  - **Held clock:** `void SetClockHeld(bool held)`. While held, `Add` first writes `U <System.GetUnixTime()> <t>` when the Unix second differs from the last anchor's, so records written while the mission clock stands still keep their real time within a second (FR-015, contract `U`).
  - **Snapshot hold:** `void BeginSnapshot()` and `void EndSnapshot()`. Between them, lines passed to `Add` by anything other than the snapshot builder go to a held list, which is appended after `SNAP-END` (contract: nothing else between `SNAP` and `SNAP-END`).
  - **Counters:** lines by record type, bytes written, and the summed and longest flush step (`System.GetTickCount` delta).
  - **Failure:** any null `OpenFile` sets a failed flag and reason, logs once `[LL_Lobby] Replay: cannot write <path>, recording stopped`, and returns false.
  - **Comments:** one WHY comment on closing every flush (no flush exists, so data reaches the disk on close; research R2). One WHY comment on the identity file: segment files must never carry identities.
- [X] T005 Complete `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c` (declared in T001): `[ComponentEditorProps(category: "Lite Lobby", description: "Server-only replay recorder; off until $profile:LL_Replays.json enables it")] class LL_ReplayRecorderComponentClass : SCR_BaseGameModeComponentClass {}` and `class LL_ReplayRecorderComponent : SCR_BaseGameModeComponent` with `static LL_ReplayRecorderComponent GetInstance()`.
  - **Done:** the server stop is the component's own `OnGameEnd` override, which runs before the shutdown save; `WillResume` is protected, so `LL_GameModeCoop.IsResumeExpected()` wraps it.
  - **Config, in `OnPostInit` on the server in play mode:** read `$profile:LL_Replays.json` with `JsonLoadContext` (a flat `Enabled` bool, as `LL_StatsManager` reads its config at lines 124-144).
    - Absent: stay `off` with no log line.
    - Unreadable: one warning naming the file.
    - Enabled: state `ready`, or `resume` when `LL_GameModeCoop.WillResume()`. One frame later (`CallLater(…, 0)`, when every component's `OnPostInit` has run) resolve the session with `ResolveSession_S()` (T001) and write the status file once.
  - **`override void OnGameStateChanged(SCR_EGameModeState state)`** (the statistics' signature, `LL_StatsManager.c:255`):
    - On `GAME` with `!LL_GameModeCoop.IsResumePending()` and state `ready`, call `StartRecording_S()`:
      - session from `ResolveSession_S()`, segment 1;
      - the header lines of contracts/recording-format.md, with the format version and no addon version (decision D1): mission and author from `SCR_MissionHeader` (`GetGame().GetMissionHeader()`), world `GetGame().GetWorldFile()`, bounds from `GetGame().GetWorldEntity().GetTerrain(0, 0).GetTerrainBoundBox(mins, maxs)` rounded to metres, in-game date and time from `ChimeraWorld.GetTimeAndWeatherManager()` (`GetDate`, `GetHoursMinutesSeconds`), `System.GetUnixTime()`, and one `H side` per faction of `GetGame().GetFactionManager()` (key, name, colour from `GetFactionColor()` as `rrggbb`);
      - then `U`, `FLOW game` and a snapshot;
      - start `CallLater(Tick_S, 100, true)`, and mark a flush due every 9 s, which `Tick_S` carries out through `LL_ReplayWriter.Flush_S()` (T004), so a record reaches the disk within FR-018's 10 seconds.
    - On `DEBRIEFING` while recording: `FLOW debrief`, `END debrief`, flush, stop both calls, state `done`.
  - **`void OnServerStop_S()`:** writes `STOP shutdown` and flushes when recording. `LL_GameModeCoop.OnGameEnd` calls it (T029).
  - **Every flush:** also writes `U <unix> <t>`, where `t = Math.Round(GetGame().GetGameMode().GetElapsedTime() * 10)`.
  - **Teardown, in `OnDelete`:** remove both calls.
  - **Status getters for `/replay`:** state, session, segment, recorded seconds, kilobytes, seconds since the last write.
  - Depends on T001, T004.
- [X] T006 Create `Lite Lobby/scripts/game/Replay/LL_ReplayEntities.c`, character part.
  - **Done:** new characters are still registered during a hard freeze; only the sampling pauses.
  - **Record class:** `LL_ReplayEntityRecord` with `int eid`, `IEntity entity`, `string kind`, `int lastX`, `lastZ`, `lastYaw`, `bool written`.
  - **Table class:** `LL_ReplayEntities` owns the table and the next `eid`.
  - **`void Tick_S(int t)`:** called every 100 ms by the recorder.
    - Returns at once while `LL_GameModeCoop.IsHardFreezeActive()`.
    - Walks the next tenth of `SCR_CharacterRegistrationComponent.GetChimeraCharacters()` by an index cursor.
    - A character not yet in the table:
      - gets an `eid` and an `E <eid> man <faction key|-> <gid|-> <prefab file name> "<display>"` line;
      - faction from `SCR_ChimeraCharacter.GetFaction()`;
      - group, read at first sight: for an AI, `SCR_AIUtils.GetAIAgent(entity).GetParentGroup()` cast to `SCR_AIGroup`; for a slot body, the `m_iGroupId` of the slot whose `LL_LobbyManager.ResolveSlotEntity(slot.m_iRplId)` (line 472) returns this entity;
      - prefab file name from the entity's prefab resource name, path and extension cut;
      - display from the editable entity's UI info name;
      - then a call to the combat hooks (`LL_ReplayCombat.OnCharacterAdded`, T013) and the seat hooks (T021).
    - A record whose entity is null or no longer listed writes `Q <eid>` and releases its hooks.
    - Otherwise write `P <eid> <x> <z> <alt> <yaw>` when `Math.AbsInt(rounded x or z − last) >= 1` or `|yaw − lastYaw| >= 5` (wrapped at 360), where position is `GetOrigin()` rounded to metres and yaw is `GetYawPitchRoll()[0]` normalised to 0–359.
  - **`void WriteSnapshot_S()`:** restates `E` and `P` for every live record.
  - **`int EidOf(IEntity e)`:** returns `-1` when unknown.
  - Depends on T004.
- [X] T007 Instrumentation in `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c` and `LL_ReplayWriter.c`:
  - **Done:** the line also carries the `playerShots` and `aiShots` counters of T015.
  - time every `Tick_S` (sampling, snapshot building and flush steps included) with `System.GetTickCount`; keep the sum and the longest;
  - with every snapshot (every 3000 tenths of mission time, T030), log one line `[LL_Lobby] Replay: <session> seg <n> lines=<n> bytes=<n> workMs=<n> avgMsPerSec=<n.nn> maxFlushMs=<n> maxTickMs=<n> types=<T:n,P:n,F:n,...>`, where `workMs` is the summed tick time of the period and `avgMsPerSec` is `workMs` divided by the period's seconds (SC-001);
  - then reset the sums and maxima (research R21).
- [ ] T008 Operator: in Workbench add `LL_ReplayRecorderComponent` (no attributes) to `Lite Lobby/Prefabs/MP/Modes/Editor/LL_GameMode_Lobby.et` and compile; on the dedicated test server write `$profile:LL_Replays.json` with `{ "Enabled": true }`.
  - **Progress:** component added to the prefab by the agent with the operator's permission (2026-10-31, id `{6AE6272DD9E6B8A2}`); the compile and `$profile:LL_Replays.json` remain.
- [ ] T009 Operator: quickstart Scenario 0 (recording off). Remove the config, play a game and expect no `LL_Replays` folder, no `[LL_Lobby] Replay` line and no status file. Then restore the config.
- [ ] T010 Operator: spikes S4 and S5 from quickstart Scenario 1 with the foundation built.
  - **S4:** drive a car due north and due east and read the `P` headings; delete the segment file between two flushes and check the next flush creates it.
  - **S5:** check whether display names arrive as keys or text.
  - Hand back the log and the file. The agent records the outcome in research R2, R3 and R19 and adjusts T004 and T006 if a heading offset or the append path needs it.

**Checkpoint**: a recording file with header, snapshot, characters' `E`/`P`/`Q`, flow start and end, and a status file exists for a demo-world game; nothing is written when the config is absent.

---

## Phase 3: User Story 1 - Every game is recorded on the server (Priority: P1) 🎯 MVP

**Goal**: the full event content of FR-004 to FR-016 in the recording.

**Independent Test**: quickstart Scenario 2: every scripted event is in the file with its time, actors and position.

- [X] T011 [US1] Vehicles in `Lite Lobby/scripts/game/Replay/LL_ReplayEntities.c` (research R5, R6).
  - **Registry at recording start:** take `SCR_EditableEntityCore.Cast(SCR_EditableEntityCore.GetInstance(SCR_EditableEntityCore))`, call `GetAllEntities(set)` and keep entries whose `GetEntityType() == EEditableEntityType.VEHICLE`. Subscribe `Event_OnEntityRegistered` and `Event_OnEntityUnregistered` with `void OnEditableRegistered(SCR_EditableEntityComponent entity)` and the matching unregister handler, and unsubscribe at stop.
  - **Kind**, per the data-model table:
    - `SCR_AIVehicleUsageComponent.FindOnNearestParent(owner)` then `GetVehicleType()` for helicopter, plane, static weapon, static artillery and tracked;
    - for wheeled vehicles, `Vehicle.m_eVehicleType` gives APC, truck or car;
    - an unclassified `Turret` root is `static`;
    - an unclassified vehicle with a `VehicleHelicopterSimulation` is `heli`;
    - anything else is `vehicle`.
  - **`E` line:** display `GetDisplayName()`, prefab file name, side from the editable entity's faction or `-`.
  - **Sampling:** in fifths per tick, so twice per second, with the same `P` test. A destroyed vehicle stays until it is unregistered, which writes `Q`.
  - **Log:** the registered count once at start, for spike S4's comparison.
- [X] T012 [US1] Trackers in `Lite Lobby/scripts/game/Replay/LL_ReplayEntities.c`: at recording start and on each snapshot, every `LL_TrackerComponent.GetAll()` entry becomes an entity of kind `tracker` (display = label from `ExportConfig`, side field = `*` when visible to any faction, otherwise the visible faction keys joined by commas); positions from `SampleTarget(out pos, out yaw)` sampled with the vehicles; `SampleTarget` returning false writes `Q` (research R12).
  - **Done:** trackers are added once at recording start: `SampleTarget` fails only when the target is gone or destroyed, which is final; they are left out of the continuation, because a tracker shares its target's persistent id.
- [X] T013 [US1] Create `Lite Lobby/scripts/game/Replay/LL_ReplayCombat.c`, class `LL_ReplayCombat`: life states, knock-outs and the weapon dictionary (research R8).
  - **Weapon dictionary:** `int WeaponId(BaseWeaponComponent weapon)` keys on UI name plus `EWeaponType` (mapped per data-model "Weapon types") and adds `W <wid> <type> "<name>"` on first sight.
  - **`void OnCharacterAdded(IEntity character, int eid)`:** subscribes `SCR_CharacterControllerComponent.m_OnLifeStateChanged` with `void OnLifeStateChanged(ECharacterLifeState previousLifeState, ECharacterLifeState newLifeState, bool isJIP)`, the vanilla parameter names of `SCR_CharacterDamageManagerComponent.c:222`.
  - **On `INCAPACITATED`:**
    - attacker from the character damage manager's `GetInstigator().GetInstigatorEntity()`, mapped to its `eid`;
    - cause is the newest `GetDamageHistory` entry's damage type in lower case, skipping bleeding with total damage under 1;
    - weapon is the attacker's last-shot weapon if under 10 s old (T015), else its current weapon (`BaseWeaponManagerComponent.GetCurrentWeapon()`, or for a gunner the turret's weapon manager through the compartment controller, as `SCR_CharacterControllerComponent.c:1149-1161`);
    - distance between the two origins, rounded;
    - write `L <eid> u <attacker|-> <cause|-> <wid|-> <dist|->`.
  - **On `ALIVE` from `INCAPACITATED`:** write `L <eid> a`.
  - **`OnCharacterRemoved`:** unsubscribes.
- [X] T014 [US1] Deaths in `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c` and `LL_ReplayCombat.c`: `override void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)`, the statistics' signature at `LL_StatsManager.c:564`, calling super first, forwards to `LL_ReplayCombat.OnDestroyed`.
  - **Deletion relations** (`DELETED`, deleted by the editor) write `Q <eid>` only.
  - **Everything else** writes `K <victim> <killer|-> <relation> <cause|-> <wid|-> <dist|->`:
    - relation per the data-model table, `vehicle` when the victim is not a character;
    - cause and weapon as in T013;
    - the damage history is read synchronously inside the callback, because it is cleared 1.5 s after death (research R8).
- [X] T015 [US1] Shots in `Lite Lobby/scripts/game/Replay/LL_ReplayCombat.c` (research R9).
  - **Registration:** in `OnCharacterAdded`, get the character's `EventHandlerManagerComponent` and register `"OnProjectileShot"` and `"OnGrenadeThrown"` with `void OnProjectileShot(int playerID, BaseWeaponComponent weapon, IEntity entity)`, the parameter names of `SCR_DataCollectorShootingModule.c:31`, using the default delayed flag. Remove both in `OnCharacterRemoved`.
  - **Per-shooter counter:** the first shot of a second stores the heading and the `wid`. The heading is the yaw of the projectile's `ProjectileMoveComponent.GetVelocity()` when the projectile is still readable, otherwise the shooter's yaw. The shooter's last weapon and its time are stored for T013.
  - **`void SecondStep_S()`:** called once per second by the recorder, writes `F <eid> <count> <yaw> <wid>` per shooter and clears the counters.
  - **Spike counters:** keep shot counts split by player and AI, and add them to the T007 summary line.
- [X] T016 [US1] Explosions in `Lite Lobby/scripts/game/Replay/LL_ReplayCombat.c` (research R10).
  - **Subscribe at recording start:** `SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Insert(OnExplosionAmmoEffect)` with `(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)`.
  - **Record:** `B <x> <z> <alt> <kind> <shooter|-> <wid|->` at `outMat[0]`. The kind (`rocket`, `grenade`, `shell`) comes from the shooter's last weapon type: rocket launcher is `rocket`, grenade launcher is `grenade`, anything else is `shell`. The shooter comes from the instigator's entity.
  - **Remove** the subscription at stop and in `OnDelete`, because the invoker is static.
- [X] T017 [US1] Followed objects in `Lite Lobby/scripts/game/Replay/LL_ReplayCombat.c` (research R10).
  - **What is followed:**
    - from `OnGrenadeThrown`, the grenade entity;
    - from `OnProjectileShot`, projectiles whose weapon type is `WT_ROCKETLAUNCHER` or `WT_GRENADELAUNCHER`;
    - at most 32 at once; beyond that, new ones are not followed. One WHY comment on the cap.
  - **Following:** while the list is non-empty, `CallLater(FollowStep_S, 0, true)` reads each object's origin and velocity, and the call is removed when the list empties.
  - **When a reference reads null:**
    - drop it when an explosion-effect record for the same shooter arrived within the last 0.5 s, so it is not recorded twice;
    - otherwise trace from the last position along the last velocity for `|v| × 0.05 s + 2 m` with the game's `TraceParam` and `GetWorld().TraceMove`, and write `B` at the hit or at the last position, kind `grenade` or `rocket`.
- [ ] T018 [US1] Operator: spikes S1, S2, S3 and S6 from quickstart Scenario 1 with T013 to T017 built.
  - **S1 and S6:** a Game Master spawns an AI group of a side without slots, for example FIA.
  - **S2:** grenades, PG-7VM, PG-7VL, 40 mm.
  - **S3:** a rifle knock-out.
  - Hand back the summary lines and the file. The agent records the outcomes in research R8, R9 and R10 and adjusts T013, T015 and T017.
- [ ] T019 [US1] Conditional on T018 (skip, marking the outcome here, if AI shot events arrived): the `OnAmmoCountChanged` fallback in `Lite Lobby/scripts/game/Replay/LL_ReplayCombat.c`, for characters with no player.
  - Register `"OnAmmoCountChanged"` with `(BaseWeaponComponent weapon, BaseMuzzleComponent muzzle, BaseMagazineComponent magazine, int ammoCount, bool isBarrelChambered)`, the parameter names as used by `SCR_SpawnProtectionComponent.c:59-63`.
  - Count a decrease of `ammoCount` on the same magazine as that many shots.
  - The heading is the shooter's yaw.
- [X] T020 [US1] Roster and players in `Lite Lobby/scripts/game/Replay/LL_ReplayEntities.c` (research R13).
  - **Done:** the roster, owners, names and connections are compared once per second instead of through the lobby-manager invokers, so nothing is subscribed and only changed values are written. A player gets a reference at once; the identity is attached when the lobby knows it, or the player takes the earlier reference of that identity (contract, Identity file).
  - **At start and in snapshots:**
    - one `GROUP <gid> <side> "<callsign>"` per distinct `LL_SlotData.m_iGroupId` (callsign `m_sGroupName`) and per AI group first seen on a character (`SCR_AIGroup.GetGroupID()`, faction key, callsign from `GetCallsigns` or the custom name);
    - one `S <m_iSortKey> <gid> <m_sFactionKey> "<m_sName>"` per slot of `LL_LobbyManager.GetSlots()`;
    - `O <slot> <pref|->`.
  - **Lobby-manager invokers** (`LL_LobbyManager.c:64-88`; slot registered, unregistered, updated; player assigned and unassigned; name updated; connection changed): write `S`, `O`, `N` and `D` only when the recorded value actually changes, since an admin login also fires the connection invoker.
  - **Player references:** keyed by `GetReconnectKeyForPlayer_S(playerId)`. A new reference writes `N <pref> "<GetPlayerName(playerId)>"` and calls `writer.AddIdentity(pref, key)`.
  - **Once per second:** for each `GetGame().GetPlayerManager().GetPlayers()`, compare `GetPlayerControlledEntity(playerId)` with the last value and write `C <pref> <eid|->` on change.
  - **Unsubscribe** every lobby-manager invoker when the recording stops and in the recorder's `OnDelete`.
- [X] T021 [US1] Seats in `Lite Lobby/scripts/game/Replay/LL_ReplayEntities.c` (research R7).
  - **Subscription:** on a new character, subscribe its `SCR_CompartmentAccessComponent.GetOnCompartmentEntered()` and `GetOnCompartmentLeft()` with `(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)`, and unsubscribe on removal.
  - **Entered:**
    - role from `manager.FindCompartment(slotID, mgrID).GetType()`: `PILOT` is driver, `TURRET` is gunner, `CARGO` is cargo;
    - seat name from the slot's compartment name;
    - write `V <eid> <vehicle eid> <role> "<name>"`.
  - **Left:** write `V <eid> -`.
  - **Snapshots:** keep the current seat per character for the snapshot's `V` lines.
- [ ] T022 [US1] Operator: rerun S1 if T019 was implemented; otherwise nothing. Confirm the AI shot counts in the summary line.
- [X] T023 [P] [US1] Edit `Lite Lobby/scripts/game/Markers/LL_ManualMarker.c`:
  - **Done:** no size getter, because the `M+` line has no size field; `IsHiddenInState` leaves out markers hidden in the game phase.
  - a server-side static registry in the shape of `LL_StatZoneComponent.c:16-42` (world-guarded array; add on the server in play mode at init, remove on delete);
  - public getters for the drawn quad name, colour as `rrggbb`, size, description text, hidden game states, the any-faction flag and the visible faction keys.
  - No change to drawing.
- [X] T024 [P] [US1] Edit `Lite Lobby/scripts/game/Zones/LL_MapZoneComponent.c`:
  - **Done:** `GetDrawnPolygon(notnull array<vector>)`.
  - the same server registry;
  - public getters for fill colour, outline colour (`rrggbbaa`), outline width, line mode, the any-faction flag, the visible faction keys, and the label (the owner entity's name, as `LL_StatZoneComponent.c:77-83`);
  - `void GetDrawnPolygon(out array<float> points)`, returning exactly the points the component draws today (origin plus local offset);
  - add `bool IsShownNow()`, a new read-only method that evaluates the same game-state and timed conditions as the drawing's visibility check (lines 86-109) without the faction part: hidden game states, hide at freeze end, hide delay from `LL_GameModeCoop.GetFreezeEndedAt()`;
  - the drawing code is not changed (Principle I keeps working production code as it is);
  - one WHY comment on `IsShownNow()`: it mirrors the drawing's rule, and a change to one must be made to the other.
- [X] T025 [P] [US1] Edit `Lite Lobby/scripts/game/Triggers/LL_TriggerZoneBase.c`: public `bool GetZoneOutline(out array<float> polygon)` that returns `m_aPolygon2D` when built, otherwise builds it with `LL_TriggerComponent.BuildPolygon(m_sZoneEntityName, null, polygon)` (check the signature at `LL_TriggerComponent.c:486-511`) without storing it, and public `string GetZoneEntityName()`.
  - **Done:** `GetZoneOutline(notnull array<float>)`; no `GetZoneEntityName()`, which had no caller.
- [X] T026 [US1] Create `Lite Lobby/scripts/game/Replay/LL_ReplayMap.c`, class `LL_ReplayMap` (research R11, R12, contracts/recording-format.md "Map").
  - **(a) Markers, once per second:**
    - read `GetStaticMarkers()` and `GetDisabledMarkers()` from `SCR_MapMarkerManagerComponent`, never appending to the returned arrays, and diff by `GetMarkerID()`, skipping `-1`;
    - a new id writes `M+ g<id> <type> <x> <z> <rotation> <pref|-> <sides> "<icon>" <colour|-> "<text>"`:
      - type is the marker type enum name in lower case;
      - author is the owner player id mapped to its reference;
      - sides come from `GetMarkerFactionFlags()` bits resolved through the faction manager's indices, with `0` as `*`;
      - icon and colour come from the `LL_MapMarkerEntryPlaced` entries of `Lite Lobby/scripts/game/Markers/LL_PlacedMarkers.c:72-111` for placed custom markers, otherwise the raw icon entry and `-`;
    - a missing id writes `M- g<id>`.
  - **(b) Manual markers** from T023's registry: `M+ n<index> manual ...` at start and in snapshots.
  - **(c) Map areas** from T024's registry: `A <aid> <sides> <fill|-> <outline|-> <width> <line> "<label>" <points>` at start and in snapshots, then `AV <aid> <0|1>` whenever `IsShownNow()` changes, checked once per second.
  - **(d) Trigger zones:** each `LL_TriggerComponent.GetAll()` entry that is an `LL_TriggerZoneBase` writes `Z <zid> <attacker|-> <defender|-> "<GetZoneStatLabel()>" <points from GetZoneOutline>`, plus `ZH` with `GetZoneHolderFaction()`.
  - Depends on T023, T024, T025.
- [X] T027 [US1] Trigger hooks:
  - **`Lite Lobby/scripts/game/Triggers/LL_TriggerComponent.c`:** in `Fire()`, right after the latch and outside the statistics branch (lines 122-145), call `LL_ReplayRecorderComponent.RecordTrigger_S(this)`, which writes `TR "<GetStatKey()>" "<label>"`.
  - **`LL_TriggerZoneCapture.c`:** next to `NotifyZoneCaptured` (lines 169-171), record `ZC <zid> <side>`.
  - **`LL_TriggerZoneContest.c`:** next to `NotifyZoneOwnerChanged` (lines 300-302), record `ZH <zid> <holder|->`, including neutral.
  - Each call is null-safe and does nothing without a recording. Depends on T026.
- [X] T028 [US1] Recorder hooks for game flow in `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c`: public `RecordFreeze_S(float remaining)`, `RecordFreezeEnd_S()`, `RecordHardFreeze_S(float remaining)` and `RecordHardFreezeEnd_S()`, which write the `FLOW` lines of the contract with seconds rounded. `RecordHardFreeze_S` also calls `writer.SetClockHeld(true)` and `RecordHardFreezeEnd_S` calls `SetClockHeld(false)` (T004, FR-015). Each does nothing without a recording.
  - **Done:** `RecordHardFreeze_S(int seconds)`, because the hard freeze counts whole seconds; `RecordTrigger_S(statKey, label)`.
- [X] T029 [US1] Edit `Lite Lobby/scripts/game/Core/LL_GameModeCoop.c`. Each call is null-safe (research R16).
  - **Done:** `AdjustHardFreeze_S` restarts through `StartHardFreeze_S` and `EndFreezeTime_S` ends through `OnFreezeTimeEnded_S`, so those hooks cover both; the stop is T005's `OnGameEnd`.
  - **Freeze:**
    - `ScheduleFreezeCountdown_S` calls `RecordFreeze_S(m_fFreezeTimeRemaining)`;
    - `OnFreezeTimeEnded_S` calls `RecordFreezeEnd_S()`;
    - `AdjustFreezeTime_S` and `EndFreezeTime_S` call `RecordFreeze_S` with the new remainder or `RecordFreezeEnd_S`.
  - **Hard freeze:**
    - `StartHardFreeze_S` and `AdjustHardFreeze_S` call `RecordHardFreeze_S` with the new remainder;
    - `EndHardFreeze_S` calls `RecordHardFreezeEnd_S()`.
  - **Server stop:** `OnGameEnd` calls `OnServerStop_S()`.
  - Depends on T028.
- [X] T030 [US1] Snapshot completeness in `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c`: `WriteSnapshot_S()` writes, in order:
  - `SNAP`;
  - every `W`, `GROUP`, `N`, `S`, `O`, `D` and `C`;
  - every live entity's `E` and `P`;
  - `L u` for every unconscious character;
  - `V` for every occupied seat;
  - every `M+`, then `A` and `AV`, then `Z` and `ZH`;
  - `FLOW freeze` or `FLOW hardfreeze` with the remainder while one runs;
  - `SNAP-END`.

  It is called at segment start and whenever 3000 tenths of mission time have passed since the last snapshot.

  The snapshot is built over consecutive ticks:
  - `writer.BeginSnapshot()`, then the snapshot lines, at most 100 entities' `E`/`P`/`L`/`V` lines per tick, then `writer.EndSnapshot()`;
  - the dictionaries, the roster, markers, areas and zones go in the first tick;
  - records produced meanwhile are held by the writer and follow `SNAP-END` (T004, research R3);
  - an entity that appears or disappears while the snapshot is being built is written after `SNAP-END`.

  Depends on T011 to T029.
- [ ] T031 [US1] Operator: in Workbench, in the demo world (`Lite Lobby/Missions/LobbyDemo.conf`), do three things, then run quickstart Scenario 2 on the dedicated server and hand back the `.llr`, the statistics final file and the log:
  - place one weapon crate with an M72 or RPG-7 with PG-7VM and PG-7VL, a grenade launcher and hand grenades;
  - set a 30 s hide delay on the NeutralZone map area;
  - allow unconsciousness in the demo game mode's health settings.

**Checkpoint**: User Story 1 passes Scenario 2; a server owner without the service already has one complete file per game.

---

## Phase 4: User Story 2 - A crash does not lose the game (Priority: P1)

**Goal**: FR-018, FR-019, FR-022: crash-safe files and a crash-resume that continues the recording in segment `n + 1`.

**Independent Test**: quickstart Scenario 3.

- [X] T032 [US2] Create `Lite Lobby/scripts/game/Persistence/LL_ReplayData.c` in the shape of `Lite Lobby/scripts/game/Persistence/LL_MarkerData.c` (research R15, data-model "Continuation record"):
  - **`class LL_ReplayContinuation`:**
    - `string session`, `int segment`, `int snapshotT`, `int nextEid`, `int nextPref`, `int nextWid`;
    - `ref array<ref LL_ReplayEntityPair> entities` (`string persistentId`, `int eid`);
    - `ref array<ref LL_ReplayPlayerPair> players` (`string identity`, `int pref`);
    - `ref array<ref LL_ReplayWeaponEntry> weapons` (`string name`, `string type`, `int wid`).
  - **`class LL_ReplayData : PersistentState {}`.**
  - **`class LL_ReplaySerializer : ScriptedStateSerializer`:**
    - `static const int VERSION = 1` and `override static typename GetTargetType()`;
    - `Serialize` asks `LL_ReplayRecorderComponent.GetInstance().CaptureContinuation_S()` and returns `ESerializeResult.DEFAULT` when it is null; otherwise it writes `"version"`, then `"replay"` with the type discriminator off;
    - `Deserialize`: an unknown version logs a warning and returns true; otherwise it reads the continuation and calls `SetPendingContinuation_S`.
- [X] T033 [US2] Edit `Lite Lobby/Configs/Systems/Persistence/LL_Persistence.conf`:
  - **Done:** Workbench loads it (operator, 2026-10-31): `State: LL_ReplayData` under ScriptedStates → Lobby, and `LL_ReplayData` enabled in Persistent States.
  - add `StatePersistenceConfig "{<fresh 16-hex id>}" { SelfDelete 0 Collection "{43DB609DE0025271}" Serializer LL_ReplaySerializer "{<fresh 16-hex id>}" {} }` inside the `Lobby` configuration group;
  - add `LL_ReplayData "{<fresh 16-hex id>}" {}` inside `PersistentStates +{ }`;
  - change nothing else (contracts/config-files.md).
  - Operator: open the file in Workbench and confirm it loads.
- [X] T034 [US2] Continuation in `Lite Lobby/scripts/game/Replay/LL_ReplayRecorderComponent.c` and `LL_ReplayEntities.c` (research R15).
  - **Done:** the static `ResumeAfterSnapshot_S()` is called from `CompleteResume_S`.
  - **`LL_ReplayContinuation CaptureContinuation_S()`:**
    - returns null unless recording;
    - `snapshotT` is the current mission time in tenths;
    - entity pairs use `PersistenceSystem.GetInstance().GetId(entity)` as `Lite Lobby/scripts/game/Persistence/LL_SessionData.c:160` does, skipping entities without one;
    - also the player and weapon maps and the next numbers.
  - **`void SetPendingContinuation_S(LL_ReplayContinuation c)`.**
  - **`void ResumeRecording_S()`:**
    - with a pending continuation:
      - restore the numbers and the player and weapon maps;
      - keep the entity pairs as a lookup: on the first sampling pass after the resume, each character or vehicle not yet in the table is looked up by `PersistenceSystem.GetInstance().GetId(entity)`, gets its old `eid` when found, and a fresh one otherwise;
      - open segment `c.segment + 1` with the header plus `H resumed <c.snapshotT>`, write a snapshot, start the calls, state `rec`;
    - without one: start a new recording as at GAME.
  - **Edit `Lite Lobby/scripts/game/Core/LL_GameModeCoop.c`:** in `CompleteResume_S`, next to `stats.ResumeRecording_S` (lines 1478-1480, after the clock restore at 1446), call the recorder's `ResumeRecording_S()`. Confirm the status file reads `resume` from server start while `WillResume()` (T005).
- [ ] T035 [US2] Operator: quickstart Scenario 3:
  - kill the server mid-game and inspect the file;
  - resume and expect `status.txt` to read `resume` then `rec`, segment 2 with `H resumed` and a snapshot;
  - stop gracefully and expect `STOP shutdown`;
  - start plainly and expect a new session.

**Checkpoint**: User Stories 1 and 2 pass; a crashed and resumed game is one recording in two segment files.

---

## Phase 5: User Story 3 - Recordings and statistics leave the server by themselves (Priority: P2)

**Goal**: FR-028 to FR-038: the host service, independent of the game code; it needs only the contracts and can start right after Phase 1.

**Independent Test**: `go test ./...` in CI (quickstart Scenario 4 step 1); host checks and the website run later (steps 2 to 5).

- [X] T036 [P] [US3] Create `uplink/internal/config/config.go` and `config_test.go` (contracts/config-files.md):
  - **Done:** also refuses a website address with a user, query or fragment and trims a trailing `/`; creates `StateDir` and checks it is writable; refuses an explicit 0 for an optional number.
  - a `Config` struct with the nine fields;
  - `Load(path string) (Config, error)`:
    - refuse a file whose permission bits allow group or other access (`mode&0o077 != 0`) on non-Windows builds;
    - JSON decode with `DisallowUnknownFields`;
    - defaults `RateCapKbit` 8000, `StoppedWaitMinutes` 30, `RetentionDays` 14;
    - validate: `Website` starts with `https://`; `ServerId` matches `^[a-z0-9-]{1,64}$`; `Secret` is at least 32 characters; `Profile` is a folder; numeric ranges per the contract.
  - One test per rule.
- [X] T037 [P] [US3] Create `uplink/internal/state/state.go` and `state_test.go` (data-model "Service state"):
  - **Done:** `Save` syncs the temporary file before the rename; `Forget` drops a game after retention.
  - `State` with `Files map[string]FileRec` (kind, session, name, committed time) and `Games map[string]GameRec` (how it ended, segment identities, identity-file identity, posted statistics identity, confirmed time, failure count, last error);
  - `Load(dir)`;
  - `Save()` writes `state.json.tmp` and then `os.Rename`.
  - The test round-trips and survives a missing file.
- [X] T038 [P] [US3] Create `uplink/internal/scan/scan.go` and `scan_test.go` (research R17, contracts/recording-format.md "Status file", data-model lifecycle).
  - **Done:** rule order: an unreadable status or `resume` holds every game; `rec` for the same session holds it until the status and the game's files are both older than the wait; then `ended`, `stopped`, `abandoned`. The session's statistics files count as the game's writes; a status time before 2001 is a half-written line and is retried.
  - **`ReadStatus(profile)`:** parses `LL_Replays/status.txt`; a malformed line returns "retry".
  - **`ListRecordings(profile)`:** groups `LL_Replays/<session>-<n>.llr` by session, with segment numbers and modification times, the presence of `END debrief` in the last segment's final lines, and the identity file.
  - **`LatestStats(profile, session)`:** picks `LL_GameStats/<session>-approved.json`, then `-final`, then `-live`.
  - **`GamesOver(now, status, recordings, wait)`:**
    - nothing is over while the status says `resume`;
    - `rec` with the same session is running;
    - an end record means `ended`;
    - a status written after the game's last write naming another session means `stopped`;
    - status and files older than `wait` mean `abandoned`.
  - Table tests for every case.
- [X] T039 [P] [US3] Create `uplink/internal/send/limiter.go` and `limiter_test.go`: a token bucket in bytes per second wrapping an `io.Reader`, with a burst of at most one second of the cap and an injectable clock. The test asserts no 10-second window exceeds the cap (research R25).
  - **Done:** the bucket refills at 90% of the cap with a 0.1 s burst, so no 1-second window exceeds the cap.
- [X] T040 [US3] Create `uplink/internal/send/client.go` and `client_test.go` per contracts/upload-protocol.md.
  - **Done:** the signed path is relative to the base address; each request gets 60 s plus the cap's time for its body; no redirects, no proxy; an extra failure kind `local` covers unreadable files and an unsavable state.
  - **Signing:** `sign(secret, method, path, ts, bodySHA)`, with the contract's test vector as a test.
  - **Headers:** `X-LL-Server`, `X-LL-Timestamp`, `X-LL-Content-SHA256`, `X-LL-Signature`.
  - **Calls:** `GetFile`, `PutPiece`, `Commit` and `PostGame`.
  - **Encoding:** gzip at best compression with zeroed header time and name; the content identity is the SHA-256 of the uncompressed file, alongside the encoded SHA-256 and size; pieces are at most 1,500,000 bytes.
  - **Limits and errors:** 60 s per request; errors classified as auth, missing, invalid, rate-limited (with `Retry-After`) and transient.
  - **Test receiver:** an `httptest.Server` that checks the signature and the 300 s window before reading the body, enforces the length limits, stores pieces idempotently, joins and checks them on commit, and answers a game post naming an uncommitted file with `409`.
- [X] T040b [US3] Create `uplink/internal/send/doc.go`, the package documentation of `ll-upload/1`, so the protocol is documented in the code that speaks it (constitution VII, FR-037). It says, as briefly as a site can implement from:
  - the endpoints `GET /files/{sha256}`, `PUT /files/{sha256}/pieces/{index}`, `POST /files/{sha256}` and `POST /games/{session}`;
  - the four `X-LL-*` headers and the canonical signing string;
  - the 300-second window and the 1,500,000-byte piece limit;
  - the commit and game JSON shapes and the answers;
  - a closing pointer to `specs/005-replay-recording/contracts/upload-protocol.md` for the full reference.

  Depends on T040.
- [X] T041 [US3] Create `uplink/internal/send/uploader.go` and `uploader_test.go`.
  - **Done:** one upload path: a confirmed game whose set (`over`, segments, identity file, statistics) changes is committed and posted again in full, so a segment written after a resume that followed an `abandoned` post is sent (FR-029); a confirmed segment missing locally blocks a re-post; a file refused twice is held, in memory, until it changes.
  - **`UploadGame`:** for each of the game's segments, its identity file and its latest statistics file:
    - skip it if committed in the state;
    - otherwise `GetFile`, send the missing pieces through the limiter, and commit; on a `409` send the listed pieces, and on a `422` resend once, then mark the file failed;
    - then `PostGame`; on a `409` send the listed files and post again;
    - a `2xx` marks the game confirmed.
  - **Backoff:** per game, from 10 s to 30 min with jitter; `Retry-After` is honoured.
  - **Statistics updates:** for a confirmed game whose latest statistics identity differs from the posted one, upload it and post the game again.
  - **Tests:** a receiver outage then recovery (no duplicate commits); `429` honoured; resend after `422`; a changed statistics file gives a new identity and a re-post, while unchanged content is never committed twice (FR-032, US4).
- [X] T042 [P] [US3] Create `uplink/internal/metrics/metrics.go` and `metrics_test.go`:
  - **Done:** every series carries `server="<ServerId>"`, so two instances can share one text-file folder.
  - write `ll_uplink_last_success_timestamp_seconds`, `ll_uplink_pending_files`, `ll_uplink_pending_bytes`, `ll_uplink_failures_total{kind="…"}` and `ll_uplink_confirmed_games_total` in Prometheus text format to `MetricsFile` through a temporary file and a rename;
  - do nothing when it is unset (research R26).
- [X] T043 [US3] Create `uplink/cmd/ll-uplink/main.go` and `uplink/internal/scan/retention.go` with `retention_test.go`.
  - **Done:** retention counts from the latest confirmation and keeps every file of a game that still has an unconfirmed segment, a changed identity file or a newer statistics file.
  - **Flags:** `-config` and `-version` (`var version = "dev"`).
  - **Startup:** load the config and the state.
  - **Every 15 s:**
    - scan;
    - upload the games that are over and unconfirmed;
    - process statistics updates;
    - retention: delete the local segment, identity and statistics files of games confirmed more than `RetentionDays` ago, and nothing else;
    - write the metrics.
  - **Logging:** `log/slog` to standard output, never the secret or a request header.
  - **Shutdown:** on SIGINT or SIGTERM, finish the current request and save the state.
  - **No listening socket.**
  - **Retention test:** only confirmed games older than the retention time lose their files.
- [X] T044 [US3] Create `uplink/README.md` (decision D2, 2026-10-31).
  - **Done:** the service-manager example is a systemd template unit.
  - **Opening:** says who needs it (a server owner whose website implements `ll-upload/1`) and when (after turning recording on).
  - **Build:** `go build`, or the release binary with its `.sha256`.
  - **Config:** link to `specs/005-replay-recording/contracts/config-files.md`.
  - **Running:** a generic service-manager unit with restart always and one instance per game server.
  - **Metrics and retention.**
  - **Nothing else.**
- [ ] T045 [US3] Operator: quickstart Scenario 4. Step 1 now (CI green on the pull request). Steps 2 to 5 on the test host once the receiving website implements the protocol: game delivered within 2 minutes, nothing while running, the abandoned and resumed cases, `ss -ltnp`, a clean log, metrics.

**Checkpoint**: the service passes its tests in CI and, against a conforming receiver, delivers finished games only.

---

## Phase 6: User Story 4 - Watching a replay costs the servers nothing (Priority: P2)

**Goal**: FR-021, FR-032 and the receiver guarantees: public files carry no identity, and sent files never change.

**Independent Test**: quickstart Scenario 5.

- [X] T046 [US4] Audit `Lite Lobby/scripts/game/Replay/*.c`:
  - **Done:** identities reach only `AddIdentity`; names come from `LL_LobbyManager.GetPlayerName`, with the engine's player name until the lobby has one; nothing reads chat or voice.
  - identity keys reach only `LL_ReplayWriter.AddIdentity`;
  - player names come only from `LL_LobbyManager.GetPlayerName`;
  - no `P`/`E`/`N` line carries an identity or a network address;
  - no chat message or voice data is read or written anywhere in the recorder (FR-016).
  - Fix any leak found.
- [ ] T047 [US4] Operator: quickstart Scenario 5 on the recordings of Scenarios 2 and 3. Search for the GUID pattern, IP addresses and the secret; check the service state for an identity committed twice with different content.

**Checkpoint**: what a receiver publishes from a recording carries no account identity, and its files are content-addressed.

---

## Phase 7: User Story 5 - The admin can see that the game is being recorded (Priority: P3)

**Goal**: FR-039, FR-040.

**Independent Test**: quickstart Scenario 6.

- [X] T048 [US5] Edit `Lite Lobby/scripts/game/Core/LL_LobbyPlayerComponent.c` in the `/verify` pattern (contracts/admin-command.md, research R20).
  - **Registration:** register `"replay"` in `RegisterChatCommands` (line 1286) and remove it in `UnregisterChatCommands` (line 1309).
  - **Client:** `OnChatCmd_Replay(SCR_ChatPanel panel, string data)` → `RequireAdmin()` → `AskReplayStatus()`.
  - **Server:** `[RplRpc(RplChannel.Reliable, RplRcver.Server)] protected void RpcAsk_ReplayStatus()` starts with `if (!SCR_Global.IsAdmin(GetPlayerId())) return;`, reads the recorder's status getters, and calls `Rpc(RpcDo_ReplayStatus, state, session, segment, seconds, kilobytes, lastWriteAgo)`.
  - **Owner reply:** `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] protected void RpcDo_ReplayStatus(int state, string session, int segment, int seconds, int kilobytes, int lastWriteAgo)` maps the state to its key and calls `ShowCommandHelp(WidgetManager.Translate(key, …))`, with minutes and megabytes rounded to one decimal.
  - **Help:** add `#LL-Command_HelpReplay` to the second `/help` message in `OnChatCmd_Help` (line 1335).
- [X] T049 [US5] Edit `Lite Lobby/Language/ll_localization.st`:
  - add `LL-Replay_NotEnabled`, `LL-Replay_Ready`, `LL-Replay_Recording`, `LL-Replay_Ended`, `LL-Replay_Failed` and `LL-Command_HelpReplay`;
  - each with a fresh GUID, single-line English from contracts/admin-command.md, and native Russian in the community's terms («реплей»);
  - then run `tools/generate-runtime-locales.ps1`.
- [ ] T050 [US5] Operator: quickstart Scenario 6 (ready, recording, ended, in English and Russian; a non-admin; the read-only profile failure).

**Checkpoint**: all five user stories pass their quickstart scenarios.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T051 [P] Add the changelog entry to `docs/src/content/docs/changelog.mdx` and `docs/src/content/docs/ru/changelog.mdx`, under the Workshop version that ships the feature. One plain line each:
  - **Done:** under `1.1.0`, the version to confirm; `npm run validate` passed.
  - `LL_ReplayRecorderComponent` (records every game to `$profile:LL_Replays/` once `$profile:LL_Replays.json` enables it);
  - `/replay` (admin, recording status);
  - `ll-uplink` (sends finished recordings and statistics to a configured website).

  Run `npm run validate` in `docs/`.
- [X] T052 Apply decision D1 (operator, 2026-10-31: format version only). Done: FR-004 no longer names the addon version; FR-020 raises the format version whenever a record's meaning changes; the decision is in the spec's Clarifications, research R18 and the `LLR` row of `contracts/recording-format.md`.
- [X] T053 Constitution pass over every new and edited file:
  - **Done:** game side: two dead methods removed, two in-body comments moved to their members, the identity-file comment added; Go: `gofmt` and `go vet` clean; no other project named.
  - `LL_` prefixes;
  - WHY-only comments with no narration;
  - no other project or website named;
  - vanilla parameter names in overrides;
  - no dead code or leftover spike logging beyond the T007 summary;
  - `gofmt` and `go vet` clean.
- [ ] T054 Operator: quickstart Scenario 7 (the same fight with recording off and on).
- [ ] T055 Operator: quickstart Scenario 8 (scale) on the event server with 127 players and at least 100 AI, with recording on and off. The agent records the measurements in research R21; a failed criterion stays failed until it passes or the operator changes it.
- [X] T056 Write the handover in the "Changes:" format (constitution, Documentation), listing every Workbench step, the `.conf` file to check, the config files to place and the open decisions.
  - **Done:** handover given in the chat, 2026-10-31.
- [ ] T057 After this feature ships, propose the map-zone rotation fix (research R12, decision D3 of 2026-10-31) as its own change through the Spec Kit process; nothing in this feature changes the map zones' points.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none.
- **Foundational (Phase 2)**: depends on T001 (T002 only confirms it); blocks User Stories 1, 2 and 5.
- **User Story 1 (Phase 3)**: depends on Phase 2.
- **User Story 2 (Phase 4)**: depends on User Story 1's entity maps and snapshot (T011 to T030).
- **User Story 3 (Phase 5)**: depends only on T003 and the contracts; runs in parallel with Phases 2 to 4.
- **User Story 4 (Phase 6)**: depends on User Stories 1 and 3.
- **User Story 5 (Phase 7)**: depends on Phase 2 (the status getters of T005).
- **Polish (Phase 8)**: depends on the stories being shipped.

### Within the game-side stories

- T004 → T005 and T006; T006 → T011, T012, T020, T021.
- T013 → T014, T015; T015 → T016, T017; T018 gates T019.
- T023, T024, T025 → T026 → T027; T028 → T029; T011 to T029 → T030.
- T032 → T033 → T034.

### Parallel Opportunities

- T001 and T003 in Setup.
- T023, T024 and T025 (three different existing files).
- The whole service phase against the game phases. Within it, T036, T037, T038, T039 and T042 together; T040 → T040b and T041 → T043.
- T048 and T049 with User Story 1 once Phase 2 is done.
- T051 at any time after the user-facing names are fixed.

## Parallel Example: User Story 3

```text
Task: "Create uplink/internal/config/config.go and config_test.go"
Task: "Create uplink/internal/state/state.go and state_test.go"
Task: "Create uplink/internal/scan/scan.go and scan_test.go"
Task: "Create uplink/internal/send/limiter.go and limiter_test.go"
Task: "Create uplink/internal/metrics/metrics.go and metrics_test.go"
```

## Parallel Example: User Story 1

```text
Task: "Edit Lite Lobby/scripts/game/Markers/LL_ManualMarker.c: registry and getters"
Task: "Edit Lite Lobby/scripts/game/Zones/LL_MapZoneComponent.c: registry, getters, IsShownNow"
Task: "Edit Lite Lobby/scripts/game/Triggers/LL_TriggerZoneBase.c: GetZoneOutline"
```

## Implementation Strategy

### MVP (Phases 1 to 3)

Setup, the foundation, and User Story 1: every game recorded to one readable file per
game, validated by Scenario 2. This alone serves a server owner who runs only the addon.

### Then

1. User Story 2 (crash-resume segments), Scenario 3.
2. User Story 5 (`/replay`), Scenario 6. It is small and helps every later test.
3. User Story 3 (the service), built in parallel from the start; its host run waits for the receiving website.
4. User Story 4 checks, Scenario 5.
5. Polish, the on/off check and the scale run.

### Gates that stop the work

- T010 (S4, S5): a heading offset or append failure is fixed in the writer before User Story 1.
- T018 (S1, S2, S3, S6): each failure takes its fallback (T019, a different knock-out read, a coarser detonation point) and is recorded in research.md before the dependent task is closed.
- T055: the scale criteria; a failure returns to the operator with the measured numbers.

## Notes

- [P] tasks touch different files and depend on no unfinished task.
- No commit, push or tag without the operator's permission for that change (constitution, Development Workflow).
- Every spike outcome is written into research.md, whatever it is.
