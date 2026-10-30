# Data Model: Session Resume After a Server Crash

The persistent records below are written by the server into the game's session save
and read back by the server on load; none of them is replicated. Clients learn about
a resume through the replication the addon already has (the slot table, player
names, the disconnected list, the hard-freeze scalars, the game's own elapsed clock)
plus the three once-set scalars this feature adds, listed under runtime state.

## Mission-maker attributes (game-mode prefab, category "Lite Lobby")

| Field | Type | Default | Read by |
|-------|------|---------|---------|
| `m_bSessionSaves` | bool | `0` | `OnGameStart` (enables or disables the save system, runs the start-time checks), the save phase gate |

The save cadence is not an attribute: it is the `autoSaveInterval` of the server
configuration's `persistence` block (default 10 minutes).

## Mission-maker attributes (deployment entity, category "Lite Lobby")

Amendment of 2026-10-11. `LL_RandomDeploymentComponent` sits on a world entity the
mission maker places once per deploying squad (prefab
`Prefabs/Spawn/LL_RandomDeployment.et`). Nothing here is replicated or saved.

| Field | Type | Default | Read by |
|-------|------|---------|---------|
| `m_sGroupEntityName` | string | empty | the start-time check, the draw, the body enumeration, the spawn-marker rule |
| `m_aPointNames` | array of string | empty | the start-time check and the draw; each is the entity name of a spawn point, only its position is read; the addon's `Prefabs/Spawn/LL_SpawnPoint.et` is the entity to place, and any other named entity works too |
| `m_bAtFreezeEnd` | bool | `1` | the game mode's two hooks and the spawn-marker rule; on = when freeze time ends, off = when the game phase starts |
| `m_fHeight` | float, metres | `0` | the placement; added above the ground position of every body, with or without a player (amendment 2026-10-21); `0` = on the ground |
| `m_bLinear` | bool | `0` | the positions; on = a line from the drawn point along its forward axis, the radius unused (amendment 2026-10-21) |
| `m_fLineSpacing` | float, metres | `40` | the line; mean distance between bodies |
| `m_fLineJitter` | float, metres | `10` | the line; random shift of each body along it, plus or minus |
| `m_fLineSpread` | float, metres | `12` | the line; random offset of each body across it, plus or minus |
| `m_fLineRise` | float, metres | `5` | the placement; extra height per body in a line, the first lowest |
| `m_fRadius` | float, metres | `10` | the positions; the dispersion radius around the drawn point |

Validation at the start of the game phase, logged, never fatal (FR-035): a squad
name that resolves to no squad makes the deployment inert; a point name that
resolves to no entity is dropped; a deployment left with no point is inert; a point
named by two deployments is reported and still used by both. An inert deployment
leaves its squad at the placed position.

## Mission-maker attributes (timed particle effect, category "Lite Lobby")

Amendment of 2026-10-21. `LL_TimedParticleComponent` sits on a particle effect
entity the mission maker places (the game's
`Prefabs/Particles/EnvironmentParticle_Base.et` with its effect path set). Nothing
here is replicated or saved.

| Field | Type | Default | Read by |
|-------|------|---------|---------|
| `m_bAtFreezeEnd` | bool | `1` | the tick; on = the effect starts when freeze time ends, off = when the game phase starts |
| `m_fBurnSeconds` | float, seconds | `900` | the tick; how long the effect plays on the mission clock |

## Mission-maker attribute (map zone, category "Lite Lobby")

Amendment of 2026-10-21, second part. On `LL_MapZoneComponent`, beside the existing
`m_bHideAfterFreezeTime`. Not replicated or saved.

| Field | Type | Default | Read by |
|-------|------|---------|---------|
| `m_fHideDelaySeconds` | float, seconds | `0` | the zone's visibility check; above `0`, on its own, how long after the end of freeze the zone stays on the map; `0` = off. `m_bHideAfterFreezeTime` on still hides the zone the moment freeze ends |

## Persistent states (server, written into the session save)

### `LL_SessionData` (serializer `LL_SessionSerializer`)

| Field | Type | Notes |
|-------|------|-------|
| `version` | int | `2`; raised from `1` by the amendment of 2026-10-11, which appends `freezeEndDeployed` |
| `state` | int | the lobby state at save time; only GAME is ever written |
| `freezeRemaining` | float | seconds; 0 when the freeze had ended |
| `hardFreezeRemaining` | float | the timed hard freeze's live remainder when one is running; the remainder a resume hold protects when a hold is active; 0 otherwise |
| `dayAdvance` | bool | the daylight setting from before any hold (the game mode's kept intent during a timed hard freeze, never the live disabled value) |
| `elapsedSeconds` | float | the game's own elapsed clock (`GetElapsedTime()`), which stands still on every machine while any hard freeze is active |
| `missionEndDuration` | int | the mission-end timer's configured seconds; 0 when the mission has no such timer (one positive-duration timer per mission is the supported configuration) |
| `missionEndStartedAt` | float | the clock reading at which its countdown began; -1 when not yet started; the timer fires from this and the clock, so it is the whole saved timer state |
| `stats` | `LL_StatsContinuation` | the recorder's continuation (session id, start stamp, winner, one-time flags, first-slot holders by identity key, players, events, zones, commanders); null when the recorder was not recording (amended 2026-09-19: replaces the per-snapshot statistics file) |
| `squads` | array of squad records | one per squad with slots, see below |
| `slots` | array of slot records | one per registered slot, see below |
| `freezeEndDeployed` | bool | amendment of 2026-10-11, last field of the record: the end-of-freeze deployments have run in this game; restored so a resume neither repeats nor skips them |

Squad record:

| Field | Type | Notes |
|-------|------|-------|
| `name` | string | the squad key: the squad entity's own name (`IEntity.GetName()`), unique per mission (FR-021), not the roster label, which is a callsign; for a squad without a name (spawned by a Game Master during the game) its persistence id (`PersistenceSystem.GetId`, a UUID string) (FR-050) |
| `frequency` | int | the radio frequency assigned at game start |

Slot record:

| Field | Type | Notes |
|-------|------|-------|
| `name` | string | the slot's display name, for the log and the refusal cause |
| `body` | UUID | `PersistenceSystem.GetId(slotEntity)`; empty (null UUID) when the body is not tracked, logged by slot name at save time, and a refusal cause on load |
| `squad` | string | the squad key, as in the squad record; the body is re-attached to the squad found by name, or by persistence id, on load when the game did not. Never empty: a slot whose squad no longer exists or is not tracked is not written (FR-050) |
| `holderKey` | string | the reconnect key (identity GUID, or the name form when enabled), read through the reverse key map so a placeholder holder's key is saved again; empty for an empty slot |
| `holderName` | string | display name, so the roster can grey it out before the player is back |
| `kia` | bool | slot damage state is destroyed |
| `locked` | bool | admin slot lock; never blocks the saved holder |
| `sortKey` | int | in-squad order, re-applied before the body registers |

Validation on load (all-or-nothing, spec FR-014): a save whose `version` is unknown,
a native load failure, a record whose body reference is empty, whose body never
becomes available within the load phase, whose named squad is not found (a squad keyed by persistence id that the load did not restore drops its slots instead), or whose
body registers no slot, and an `ACTIVE` state reached with no record at all, each
refuse the whole resume. Two kinds of record are dropped individually, each with a
warning, and their bodies stay plain characters: one whose squad is keyed by
persistence id and was not restored by the load (dropped at `ACTIVE`, FR-050), and
one with an empty `squad`, written before FR-050 (dropped before the record is
applied). A refusal disables saving
and the save types, logs its cause, purges nothing, and asks the game to close.

### `LL_TriggerData` (serializer `LL_TriggerSerializer`)

| Field | Type | Notes |
|-------|------|-------|
| `version` | int | `1` |
| `triggers` | map, stat key → trigger record | keyed by `LL_TriggerComponent.m_sStatKey` |

Trigger record: `started` (bool, the trigger's own countdown or evaluation was running, answered by the subclass: for the mission-end timer, its countdown tick was scheduled, not merely its freeze wait), `fired` (bool), plus,
only when started, the subclass state written by the trigger's own hook, fired or
not: nothing extra for the mission-end timer proper (its start reading lives in the
session record); `startedAt` (the clock reading its countdown began) for every timed
announcement, which inherits the timer's countdown, so any number of them resume
correctly; `held` and `captured` (zone capture, with
the derived defender flag), `progress` and `holder` (zone contest; the holder cannot
be derived from progress because ownership has hysteresis), `bothSeen` (supremacy).
A trigger that had not started initialises normally after the resume; a started one
keeps its restored fields through its first activation after the load instead of
re-initialising them. A trigger whose stat key is a per-load sequence rather than an
entity name is written with a warning; mission makers are told to name their trigger
entities.

### `LL_MarkerData` (serializer `LL_MarkerSerializer`)

| Field | Type | Notes |
|-------|------|-------|
| `version` | int | `1` |
| `markers` | array of marker records | every static marker with a marker id, disabled ones included |

Marker record mirrors `SCR_MapMarkerManagerComponent.RplSave`: `posX`, `posY`,
`flags`, `configId`, `factionFlags`, `rotation`, `type`, `colorEntry`, `iconEntry`,
`text`, `timestampVisible`, `timestamp`. The owner id is not written; restored
markers are server markers.

## Vanilla records neutralised

| Vanilla serializer | Modded as | Behaviour |
|--------------------|-----------|-----------|
| `SCR_PlayerReconnectDataSerializer` | `LL_M_SCR_PlayerReconnectDataSerializer` | writes nothing, reads nothing, never schedules the unclaimed-character deletion |
| `SCR_PlayerControllerSerializer` | `LL_M_SCR_PlayerControllerSerializer` | writes nothing, reads nothing; the lobby's reservation replaces it |

## Runtime state added to existing classes (not saved)

Server-only unless marked.

| Owner | Field | Purpose |
|-------|-------|---------|
| `LL_GameModeCoop` | `m_fHardFreezeRemaining < 0` | a hold with no countdown; existing replicated scalar, new meaning |
| `LL_GameModeCoop` | `m_fResumedSnapshotTime` | **replicated scalar (1 of 3 added)**; shown in the hold text; 0 when not resumed |
| `LL_GameModeCoop` | `m_iMissionEndDuration`, `m_fMissionEndStartedAt` | **replicated scalars (2 and 3 of 3 added)**, each set once and bumped once; the spectator countdown's inputs; restored from the session record on resume |
| `LL_GameModeCoop` | `m_fHeldElapsed`, `m_fLastSeenElapsed` (proxy) | the clock reading when the current hard freeze began; written back over the game's elapsed clock every frame while any hard freeze is active, on every machine (`EOnFrame` override); on a proxy a replicated value that differs from the one written last frame, seen before the base advances, is a correction and becomes the new anchor; set from `elapsedSeconds` on resume; declared with T019 |
| `LL_GameModeCoop` | `m_bFreezeTimerScheduled` + `ScheduleFreezeCountdown_S` / `CancelFreezeCountdown_S` | the only way the freeze countdown is scheduled or removed: fresh start, both hard-freeze releases, the admin adjustment, the explicit end and the normal expiry all use them |
| `LL_GameModeCoop` | `m_fResumeDeadline` | 0 until `ACTIVE` arms it (tick of `ACTIVE` plus 10 s); body operations read it each tick and have no timeout while it is 0 |
| `LL_GameModeCoop` | `m_bStartupBatchDispatched` | set at the end of the fresh-start entry and of the resume finaliser, cleared on leaving GAME; saving is allowed only with it set and no replacement pending |
| `LL_VoNChannelsManager` | `RefreshParking_S(playerId)` | public wrapper over the protected parking update, for the claim's delayed possession |
| `LL_GameModeCoop` | `OnNativeLoadResult_S(bool)` | declared in Phase 2 with the switch; a failure refuses at once and idempotently |
| `LL_GameModeCoop` | `LL_SaveWaiter` (helper object) | one-shot wait re-evaluated on the save manager's busy-state event and on the replacement count reaching zero; created with a context, the callback inserted into its ready invoker, then started (a script method cannot take a func argument); used by the deferred replacement, the debriefing discard and the stop-time save; unsubscribes before running its action |
| `LL_GameModeCoop` | `m_bStopSaveDone` | set when a shutdown-type save completes after the stop began; the addon requests none if set, and disables saving after its own completes |
| `LL_GameModeCoop` | `m_bResumePending` | true from world start until the resume finaliser has run; gates the fresh start, the playable registration retry and the recorder start |
| `LL_GameModeCoop` | `m_aPinnedVehicles` | aircraft pinned at finalisation, unpinned on release |
| `LL_GameModeCoop` | `m_fPreHoldHardFreezeRemaining`, `m_bPreHoldDayAdvance` | what a snapshot during a resume hold writes instead of the hold's own values; what the release restores; a timed hard freeze needs neither (its live remainder is saved) |
| `LL_GameModeCoop` | `StopVehiclesForHold_S()` | declared with the resume entry, filled by the hold phase; the finaliser calls it |
| `LL_LobbyManager` | `m_iPendingReplacements` | body replacements in flight; one acquisition before the spawn, one release per terminal exit (spawn failure, new body gone at finish, success, abandonment, hand-off failure); saving is disallowed synchronously while above zero and allowed by the release that reaches zero while in GAME, which also wakes the waiters; a replacement during a save waits through an `LL_SaveWaiter` |
| `LL_LobbyManager` | placeholder holder ids from 1,000,000 | stand-ins for absent holders, present in the name, disconnected, reverse-key and reservation maps; skipped by voice assignment; transferred by the resume claim on reconnect |
| `LL_LobbyManager` | `ClaimResumedSlot_S(playerId, key)` | consumes a reservation once, only when the reserved slot's holder is a placeholder; owns the transfer: placeholder removed, one assignment (`ApplyTakeSlot` + broadcast), faction set, then for an alive slot `PossessSlot_S` after the reconnect delay followed by one `UpdateParked_S`; KIA slot → spectator entry, slot kept; lock ignored; `TakeSlot_S` untouched; ordinary reservations untouched |
| `LL_LobbyPlayerComponent` | `EnterSpectatorNow` re-check | a pending spectator entry does nothing for a player who now controls a living body |
| `LL_PlayableComponent` | one bounded resume operation (agent, squad attach, registration with a valid network id, observed) | deadline read from the game mode each tick, none until armed; done = the manager holds a slot for this entity in the saved squad; a fallback id is a refusal; stops on refusal |
| `LL_M_SCR_PersistenceSystem` (modded) | `OnAfterLoad(bool success)` forwarded | the native load result reaches the game mode's resume state; a failure refuses |
| `LL_M_SCR_AIGroup` (modded) | `m_bLLResumeSpawnSuppressed` | local, not saved or replicated; once a map squad is identified as belonging to a restored world, suppresses its initial and queued member spawning beyond load completion; later dynamic squads are excluded |
| `LL_StatsManager` | `CaptureContinuation_S()`, `ResumeRecording_S(state)` | the first returns the embedded block (null while not recording); the second applies the saved session id and maps, then arms the recorder once (flag, assignment hook, live-write timer) without the sweep, the commander freeze or the immediate write; no block → a new recording, logged |
| `LL_TriggerMissionEndTimer` | fires from the mission clock | `m_fSecondsLeft` removed; the tick fires when the clock reaches the start reading plus the duration; a restored started timer resumes directly in the countdown, no activation poll or freeze wait |
| `LL_ZoneRestrictionComponent` (client) | countdown pause | the return countdown does not drain while the replicated hold flag is set |
| `LL_TriggerComponent` | static server-side trigger list; `m_bRestored` | what `LL_TriggerSerializer` iterates; the flag that skips re-initialisation on the first activation after a load (only for triggers that had started) |
| `LL_StatsManager` | first-slot holders by identity key (`map<string, string>`), identity resolution through the lobby's reconnect keys first | placeholders keep the saved identity's credit; the recorder is not restarted on a resume |

### Random squad deployment (amendment of 2026-10-11)

Server-only unless marked; nothing replicated.

| Owner | Field | Purpose |
|-------|-------|---------|
| `LL_RandomDeploymentComponent` | static list of the world's deployments | what the game mode's hooks and the spawn-marker rule iterate; filled at component init on the server, flushed on a world change |
| `LL_RandomDeploymentComponent` | the drawn point and whether it has been drawn | set once per game; read by the placement and, for a start-of-game deployment, by the game-phase spawn-marker snapshot |
| `LL_RandomDeploymentComponent` | pending spawn transforms, keyed by placed body | filled before the game-start body loop for slots with a connected player, taken by `RespawnSlotCharacter_S`, dropped at the end of `OnEnterGame_S` |
| `LL_GameModeCoop` | `m_bFreezeEndDeployed` | the end-of-freeze deployments have run; set on the first run, saved in the session record, never cleared within a game |
| `LL_LobbyPlayerComponent` | `Deploy_S(vector)` and `RpcDo_OwnerDeploy(vector)` | **the one message added**: server to the moved player, once per end-of-freeze deployment; the owner leaves a vehicle seat if seated (bounded retry), then runs the game's own player teleport |

### Timed particle effects (amendment of 2026-10-21)

Local to each machine that draws particles; nothing replicated or saved.

| Owner | Field | Purpose |
|-------|-------|---------|
| `LL_TimedParticleComponent` | the start reading on the mission clock | set when the effect lights: the game mode's freeze-end reading, or zero for the game phase |
| `LL_GameModeCoop` | `m_fFreezeEndSeenAt` | the clock at the first end of freeze this machine saw, -1 until then; set by `OnFreezeTimeEnded_S` on the server and by the countdown's `onRplName` callback on a client; read by `GetFreezeEndedAt()` after the mission-end timer's start |
| `LL_GameModeCoop` | `m_bFreezeSeenRunning` | a client saw the countdown above zero, so its arrival at zero is a real end of freeze and not a late join |
| `LL_TimedParticleComponent` | lit, and done | the effect burns once; after its stop the tick ends |

## Files written to disk

- The engine's session save, in the location the engine chooses for the server
  profile; not a Lite Lobby file.
- The statistics live, final and approved files, as today. No per-snapshot
  statistics file: the recorder's state is inside the engine's save (2026-09-19).
- With `m_bSessionSaves` off: nothing.
