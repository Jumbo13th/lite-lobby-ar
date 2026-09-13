# Implementation Plan: AI Spotting Markers Switch

**Branch**: `003-ai-spotting-markers` | **Date**: 2026-09-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/003-ai-spotting-markers/spec.md`

## Summary

Add one game-mode checkbox, off by default, that decides whether AI groups may report
the enemies they identify as military map markers. The game's report pipeline has a
single entry on the server, the group-perception call that hands an identified target
to the marking system; a modded override of that call returns early when the game
mode says no and otherwise defers to the game unchanged. Nothing is replicated: with
the switch off the marker is never created, so nothing exists to broadcast, purge or
hand to a joining player.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.8.0.10 (verified unchanged in 1.8.0.13)

**Primary Dependencies**: vanilla `SCR_AIGroupPerception.MarkEnemyOnMap` (1.8, `scripts/Game/AI/Group/SCR_AIGroupPerception.c`), `SCR_AIEnemyMarkingSystem` (server-located game system, `scripts/Game/Systems/`), the existing `LL_GameModeCoop` attribute block and `GetInstance()`

**Storage**: N/A (one prefab attribute, read at call time on the server)

**Testing**: manual, in the demo world on a dedicated server (see [quickstart.md](quickstart.md))

**Target Platform**: dedicated server; clients are untouched

**Project Type**: game addon, one attribute and one modded class

**Performance Goals**: no measurable cost; with the switch off the server does strictly less work than the unmodified game

**Constraints**: server-side only; no replication; no chat command; game's per-faction setting stays authoritative

**Scale/Scope**: two script files (one new, one edited); no assets

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | The hook is the game's own 1.8 entry point `SCR_AIGroupPerception.MarkEnemyOnMap` (protected, one caller, `Managed` class). The addon already mods plain script classes the same way (`SCR_PlayerNamesFilterCache`, `SCR_AvailableActionsConditionData`). The attribute and getter follow the existing `m_bDisableChat` / `IsChatDisabled()` pair in `LL_GameModeCoop`. Consumers reach the game mode through `LL_GameModeCoop.GetInstance()` as `LL_M_SCR_ChatPanel` does. | PASS |
| II. Ask, Never Assume | The default was asked and answered in the spec. The choice of hook point was decided from the vanilla sources (research R1); the two alternatives are recorded with the reasons they lost. | PASS |
| III. KISS | One attribute, one getter, one override with an early return. No per-faction table, no runtime toggle, no chat command. | PASS |
| IV. YAGNI | The attribute has a reader (the override). No other knob; the game's own per-faction switch remains the place for finer control. | PASS |
| V. Replication | Replicated values added: none. RPCs added: none. `RplSave`/`RplLoad`: unchanged. Visibility classification: server-only decision; the payload it suppresses was a faction-scoped broadcast, so the wire carries less, never more. | PASS |
| VI. Comments and Hygiene | One WHY on the attribute (why off by default) and one on the override (why this call is the single gate and why a missing game mode defers to the game). New file `LL_M_SCR_AIGroupPerception.c` in `scripts/game/Modded/`; vanilla parameter name `target` kept. | PASS |
| VII. Optional Integrations | Not touched. | PASS |
| Engine and Asset Constraints | No `.et`, `.layout`, `.conf`, `.acp` or `.meta` change. The new attribute appears on the existing game-mode prefab with its script default; the operator ticks it only to test User Story 2. | PASS |
| User Interface | No screen change. The attribute description is Workbench text, not localized by rule. | PASS |
| Localization | No user-visible string. | PASS |
| Testability | Reproducible in the demo world: it already places one stock rifle squad per faction and the `LL_GameMode_Lobby` entity. Dedicated server for the join-in-progress scenario. | PASS |

Workbench steps for the operator: none for the default behaviour. For the User Story 2
check, tick the new checkbox on the `LL_GameMode_Lobby` entity in the demo world (or on
the prefab) and revert it afterwards. Asset files to double-check: none.

## Project Structure

### Documentation (this feature)

```text
specs/003-ai-spotting-markers/
├── plan.md              # This file
├── research.md          # Phase 0: where the game creates the markers, hook choice
├── data-model.md        # Phase 1: the one attribute
├── quickstart.md        # Phase 1: validation in the demo world
└── tasks.md             # Phase 2 (/speckit-tasks)
```

No `contracts/`: the feature exposes no interface beyond one prefab attribute, which
data-model.md describes.

### Source Code (repository root)

```text
Lite Lobby/scripts/game/
├── Core/
│   └── LL_GameModeCoop.c                  # m_bAllowAiSpotReports attribute + AllowAiSpotReports()
└── Modded/
    └── LL_M_SCR_AIGroupPerception.c       # new: MarkEnemyOnMap override
```

**Structure Decision**: the attribute joins the other mission policy attributes on the
game mode; the override lives with the other modded vanilla classes. No new system,
component or manager.

## Design

1. `LL_GameModeCoop`: `[Attribute("0", UIWidgets.CheckBox, ..., category: "Lite Lobby")]
   protected bool m_bAllowAiSpotReports;` placed after `m_bRemoveRedundantUnits` (the
   other AI-related policy), with the getter `bool AllowAiSpotReports()` in the policy
   getter block (FR-001, FR-002). The description states what the game does, that the
   lobby turns it off because the markers reveal player squads, and that a faction
   whose own settings forbid reports never reports (FR-008).
2. `LL_M_SCR_AIGroupPerception.c`: `modded class SCR_AIGroupPerception` overriding
   `protected void MarkEnemyOnMap(notnull SCR_AITargetInfo target)`. If
   `LL_GameModeCoop.GetInstance()` returns a game mode and its switch is off, return;
   otherwise `super.MarkEnemyOnMap(target)` (FR-003, FR-004). A missing game mode
   means a non-lobby scenario is running with the addon loaded, and the game keeps
   its behaviour there.
3. The faction gate, cooldown, lumping, lifetime and visibility stay inside the game's
   marking system, which the override never touches (FR-004, FR-005).
4. No other marker path is involved: player-placed, squad-leader, mission-maker and
   tracker markers never pass through group perception (FR-006).
5. Nothing is sent: the override runs on the server before a marker object exists
   (FR-007). Join-in-progress carries only markers that exist, so with the switch off
   it carries none of this kind.

## Constitution Check (post-design)

Unchanged from the gate above: no replication, no assets, one attribute with one
reader, one override that defers to the game. PASS.

## Complexity Tracking

No violations to justify.
