# Data Model: AI Spotting Markers Switch

No entity is added, stored or replicated. The feature adds one mission-maker attribute
and reads it on the server.

## Attribute (prefab, server-read)

| Field | Owner | Type | Default | Read by |
|-------|-------|------|---------|---------|
| `m_bAllowAiSpotReports` | `LL_GameModeCoop` (game-mode prefab, category "Lite Lobby") | bool | `0` (off) | `SCR_AIGroupPerception.MarkEnemyOnMap` override, via `LL_GameModeCoop.AllowAiSpotReports()` |

Rule: off means the override returns before the game's marking system is reached; on
means the vanilla call proceeds and the game's own per-faction setting decides.

## Inputs the feature reads

- The game-mode instance (`LL_GameModeCoop.GetInstance()`); null means a non-lobby
  scenario, treated as "on" so the game keeps its behaviour.

## What the feature deliberately ignores

- The target, the spotting group and its faction: all of that stays with the game's
  marking system when the call is allowed through.
- Game state: the switch applies in every state, since AI perception only produces
  targets once bodies exist and the game is running.
