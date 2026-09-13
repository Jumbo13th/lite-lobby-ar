# Research: AI Spotting Markers Switch

## R1. Where the game creates the markers, and where to stop them

- **Decision**: override `SCR_AIGroupPerception.MarkEnemyOnMap` in a modded class and
  return early when the lobby game mode has the switch off.
- **Rationale**: 1.8.0.10 added `SCR_AIEnemyMarkingSystem`
  (`scripts/Game/Systems/SCR_AIEnemyMarkingSystem.c`, server-located, registered in
  `GameData/Configs/Systems/ChimeraSystemsConfig.conf`) and one caller:
  `SCR_AIGroupPerception.MarkEnemyOnMap`, invoked from `AddOrUpdateTarget` whenever a
  target that is already IDENTIFIED is perceived again as an enemy
  (`scripts/Game/AI/Group/SCR_AIGroupPerception.c`, lines 139 and 182 to 190). The
  method is `protected`, takes one `notnull SCR_AITargetInfo target`, and does nothing
  but look the system up and call `MarkTarget`. Group perception runs on the server
  with the AI, so the decision is taken where the constitution wants it. The class is
  `Managed`; the addon already mods such plain script classes
  (`LL_M_SCR_PlayerNamesFilterCache.c`, `LL_M_SCR_AvailableActionsConditionData.c`),
  so the mechanism is proven in this codebase. 1.8.0.13 changes neither file.
- **Alternatives considered**:
  - Modded `SCR_AIEnemyMarkingSystem.MarkTarget` (rejected: the addon has never modded
    a `GameSystem`; the class is registered by name in the systems config with a
    static `InitInfo`, and whether a modded system keeps that registration is not
    shown anywhere in the official sources. Same effect, unknown mechanism).
  - Modded `SCR_Faction.CanAIMarkTargets` (rejected: `SCR_Faction` is a
    `BaseContainerProps` config class; a modded one must repeat decorator and base or
    the faction list silently truncates, and it would have to reach the game mode from
    inside a config object. More risk for the same result).
  - Overriding `ChimeraSystemsConfig.conf` to drop the system (rejected: a global
    vanilla resource, forbidden by the constitution's asset rules).
  - Mission-side `m_bCanAIMarkTargets 0` on each faction config (declined by the
    operator; it remains available to mission makers who want per-faction control).

## R2. How the switch reaches the override

- **Decision**: `LL_GameModeCoop.GetInstance()` and a plain getter, the way
  `LL_M_SCR_ChatPanel.LL_ShouldShowChat` reads `IsChatDisabled()`.
- **Rationale**: the game mode is the one entity every policy attribute already lives
  on, and the static getter is the established access path. A null game mode means
  the addon is loaded under a scenario that does not use the lobby; deferring to the
  game there keeps the addon inert outside its own missions.
- **Alternatives considered**: caching the flag in the perception object at
  construction (rejected: the perception is created with the group, which may exist
  before the game mode is initialised; reading at call time costs one cast per
  report, which the game already throttles to one per group per target per cooldown).

## R3. What the switch must not change

- **Decision**: the override never calls the marking system itself; when the switch
  is on it calls the vanilla method and nothing else.
- **Rationale**: FR-004 and FR-005. The faction gate (`CanAIMarkTargets`), the 30 s
  per-group cooldown, the 150 m / 250 m lumping distances, the 330 s lifetime, the
  inaccuracy model and the faction flags all live in `MarkTarget`. Re-implementing any
  of them would be a second copy that drifts.

## R4. Why nothing is replicated

- **Decision**: no new replicated value, RPC or join-in-progress field.
- **Rationale**: `MarkTarget` builds the marker and calls
  `InsertStaticMarker(marker, false, true)`, which is what broadcasts it (through the
  addon's server-side faction gate in `LL_M_SCR_MapMarkerManagerComponent`) and what
  stores it for `RplSave`. Returning before that call means no marker object, no
  broadcast, no join-in-progress entry, no removal RPC 330 s later. Clients need no
  knowledge of the switch because they never receive anything to filter.

## R5. Demo world readiness

- **Decision**: no new entity in the demo world.
- **Rationale**: `worlds/Arland/LobbyDemo_Layers/US.layer` and `USSR.layer` already
  place `Group_US_RifleSquad.et` and `Group_USSR_RifleSquad.et`, and `a_systems.layer`
  places `LL_GameMode_Lobby` (`Prefabs/MP/Modes/Editor/LL_GameMode_Lobby.et`) and the
  `PerceptionManager`. Both stock factions have the game's per-faction report setting
  on, so the vanilla trail is reproducible there with the switch on.
