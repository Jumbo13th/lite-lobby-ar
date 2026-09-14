# Contract: configuration files and the mission header

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

- the native world-systems config field is set to `LL_LobbySystems.conf`
- the save-type flags keep their default (all), or at least manual, automatic and
  shutdown

For the demo world this is done on `Missions/LobbyDemo.conf` by the operator.

## Game-mode attributes

`m_bSessionSaves` (off) and `m_iSaveIntervalMinutes` (5) appear on the game-mode
prefab with their script defaults; the operator ticks the first one on the demo
world's game-mode entity to test.
