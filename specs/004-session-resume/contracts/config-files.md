# Contract: configuration files, the mission header, the server configuration

Two new config files, both hand-authored and listed for the operator to open in
Workbench. GUIDs inside them are read back from the `.meta` files Workbench writes.

## `Configs/Systems/LL_LobbySystems.conf`

A `SystemSettings` that inherits the game's base game-mode systems config and points
the persistence system at the lobby's persistence config:

```text
SystemSettings : "{1C60D2EDA2B468B8}Configs/Systems/BaseGameModeSystems.conf" {
 Systems {
  SCR_PersistenceSystem "<fresh id>" {
   SystemLocation Server
   SystemPoints 0x2000 0x10
   Config "{<LL_Persistence.conf GUID>}Configs/Systems/Persistence/LL_Persistence.conf"
  }
 }
}
```

This is the shape of the game's `MissionSystems.conf`.

## `Configs/Systems/Persistence/LL_Persistence.conf`

A `PersistenceSystemConfig` that inherits the game's editable-mission chain and
adds the lobby's collection and states:

- inherits `{5BA7C4643477E2D7}Configs/Systems/Persistence/EditableMission.conf`
- appends one `PersistenceCollection` named `LL_Lobby` on the session storage and
  adds it to the `WorldState` bundle (`+{` append, as `Mission.conf` does)
- appends `LL_SessionData`, `LL_TriggerData`, `LL_MarkerData` to `PersistentStates`
- adds three `StatePersistenceConfig` entries, each with `SelfDelete 0`, the
  `LL_Lobby` collection and its serializer (`LL_SessionSerializer`,
  `LL_TriggerSerializer`, `LL_MarkerSerializer`), in the same group structure as
  `Mission.conf` lines 58-88

## Mission header

For every mission that wants session saves:

- the **Systems Config** field (under World in the header's Workbench view) is set
  to `LL_LobbySystems.conf`
- the **Save Types** flags stay at their default (all four); the game-mode checkbox
  decides whether the mission saves

For the demo world this is done on `Missions/LobbyDemo.conf` by the operator. The
mission-making guide's step that unticks the four flags is replaced by these two
lines, in English and Russian (FR-019), plus one line telling makers who create
squads from script to do so from the lobby's game-start hook, which a resume does
not run.

At game start with the checkbox on, the game mode checks all three (save types,
systems config, the lobby collection in the persistence config) and logs the
missing one by name.

## Game-mode attribute

`m_bSessionSaves` (off) appears on the game-mode prefab with its script default; the
operator ticks it on the demo world's game-mode entity to test.

## Server configuration (`config.json`, `persistence` block)

| Key | Value | Why |
|-----|-------|-----|
| `loadSessionSave` | `false` | a plain restart is a fresh start |
| `autoSaveInterval` | default 10 (1 on the test server) | the snapshot cadence, minutes |
| `saveRetention` | default 10 | rollback depth in snapshots |
| `keepSessionSave` | default false | finished games leave no snapshots |
| `databases` / `storages` | test server only: the game's JSON preset for the session storage | readable snapshots while debugging |

## Launch line

- plain: fresh start
- `-loadSessionSave`: continue from the newest snapshot
- `-loadSessionSave <uuid>`: continue from the snapshot whose `meta-info.json`
  carries that UUID

The parameter is for one start. Left in the container command, every later restart
becomes a resume attempt.

If the quickstart's first scenario shows the parameter does not override
`loadSessionSave: false`, the fallback is the configuration value itself set to
`true` for that one start (newest snapshot only); the addon builds no chooser.

## Incident procedure (filled in after quickstart Scenario 4)

To be recorded here by the operator task that measures the planned stop, before
the feature is called done:

- the deployed entrypoint and how the launch parameter is added for one start and
  removed afterwards
- the stop signal the container sends and the stop budget: the measured exit time
  of a graceful stop at representative load, doubled, never the bare measurement
- the profile mount that holds the snapshots, and how a snapshot UUID is read from
  `meta-info.json` there
- the log lines that distinguish "resumed from <uuid>", "fresh start, no snapshot
  requested", "no snapshot found for the request" and "resume refused: <cause>",
  so a forgotten step is visible in the first minute
- what the launcher does after a refusal (the process exits by itself with the
  cause logged): the accepted outcome is either a stopped container or an
  automatic plain restart into a fresh game, since the resume parameter is passed
  once; record which one this deployment does

Until this section is filled, the procedure is the conceptual one above and the
feature is not accepted.
