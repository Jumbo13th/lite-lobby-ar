# Data Model: Session Resume After a Server Crash

Nothing here is replicated. Every record is written by the server into the game's
session save and read back by the server on load. Clients learn about a resume only
through the replication the addon already has: the slot table, player names, the
disconnected list, the hard-freeze scalars.

## Mission-maker attributes (game-mode prefab, category "Lite Lobby")

| Field | Type | Default | Read by |
|-------|------|---------|---------|
| `m_bSessionSaves` | bool | `0` | `OnGameStart` (enables or disables the save system), the save timer, the chat commands |
| `m_iSaveIntervalMinutes` | int | `5` | the save timer; values below 1 are treated as 1 |

## Persistent states (server, written into the session save)

### `LL_SessionData` (serializer `LL_SessionSerializer`)

| Field | Type | Notes |
|-------|------|-------|
| `version` | int | `1` |
| `state` | int | the lobby state at save time; only GAME is ever written |
| `freezeRemaining` | float | seconds; 0 when the freeze had ended |
| `hardFreezeRemaining` | float | seconds; 0 when no hard freeze ran |
| `elapsedSeconds` | float | game time since GAME started, for the HUD clock |
| `statsSession` | string | the statistics session id, so the live snapshot can be reloaded |
| `slots` | array of slot records | one per registered slot, see below |

Slot record:

| Field | Type | Notes |
|-------|------|-------|
| `body` | UUID | `PersistenceSystem.GetId(slotEntity)`; a slot whose body is not tracked is skipped and logged |
| `holderKey` | string | the reconnect key (identity GUID, or the name form when enabled); empty for an empty slot |
| `holderName` | string | display name, so the roster can grey it out before the player is back |
| `kia` | bool | slot damage state is destroyed |
| `locked` | bool | admin slot lock |
| `sortKey` | int | in-squad order, re-applied before the body registers |

Validation on load: a record whose body never becomes available within the load
phase is dropped with a log line; a record whose body registers no slot is dropped
the same way. A save whose `version` is unknown is refused (the whole resume is
refused, spec FR-014).

### `LL_TriggerData` (serializer `LL_TriggerSerializer`)

| Field | Type | Notes |
|-------|------|-------|
| `version` | int | `1` |
| `triggers` | map, stat key → trigger record | keyed by `LL_TriggerComponent.m_sStatKey` |

Trigger record: `fired` (bool) plus the subclass state written by the trigger's own
hook: `secondsLeft` (mission-end timer), `held` and `captured` (zone capture),
`progress` (zone contest), `bothSeen` (supremacy). A trigger whose stat key is a
per-load sequence rather than an entity name is written with a warning; mission
makers are told to name their trigger entities.

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

## Runtime state added to existing classes (server only, not saved)

| Owner | Field | Purpose |
|-------|-------|---------|
| `LL_GameModeCoop` | `m_fHardFreezeRemaining < 0` | a hold with no countdown; existing replicated scalar, new meaning |
| `LL_GameModeCoop` | `m_bResumePending` | true from world start until the resume finaliser has run; gates the fresh start and the playable registration retry |
| `LL_GameModeCoop` | `m_aPinnedVehicles` | aircraft pinned at finalisation, unpinned on release |
| `LL_LobbyManager` | placeholder holder ids from 1,000,000 | stand-ins for absent holders; cleared by the existing `RemovePlayer_S` on reconnect |
| `LL_PlayableComponent` | resume-aware retry in `RegisterWithManager` | waits for the AI group while a resume is pending |
| `LL_TriggerComponent` | static server-side trigger list | what `LL_TriggerSerializer` iterates |

## Files written to disk

- The engine's session save, in the location the engine chooses for the server
  profile; not a Lite Lobby file.
- The statistics live snapshot, already written today.
- With `m_bSessionSaves` off: nothing.
