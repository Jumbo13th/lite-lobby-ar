# Implementation Plan: Player List Order

**Branch**: `001-player-list-order` | **Date**: 2026-09-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/001-player-list-order/spec.md`

## Summary

Order the lobby player panel unit-tagged names first, then alphabetical ignoring case,
and keep that order through joins, name updates and rebuilds. The rows already exist;
they are reordered in place by Z order, the way the vanilla scoreboard sorts its rows,
using a stable insertion sort with an explicit comparator, the way the lobby manager
already sorts slots. The unit-tag test moves out of the name formatter into a shared
predicate so colouring and ordering agree. Nothing is replicated.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.8.0.10

**Primary Dependencies**: vanilla `Widget.SetZOrder`, `string.Compare(sample, caseSensitive)`,
the existing `LL_CoopLobby` / `LL_PlayerSelector` / `LL_LobbyManager` classes

**Storage**: N/A (no persisted or replicated state)

**Testing**: manual, in the demo world and on a dedicated server (see [quickstart.md](quickstart.md))

**Target Platform**: game client; the server is untouched

**Project Type**: game addon, UI change in one screen

**Performance Goals**: no visible hitch at 127 rows on open or on any roster event

**Constraints**: client-side only; rows reused, not recreated; no new attribute

**Scale/Scope**: 127 rows; three script files touched

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Row reordering follows `SCR_PlayerListMenu.SortByName` (rows in a `VerticalLayoutWidget` reordered with `SetZOrder`, `PlayerListMenu.layout` "Table"). Comparison uses `string.Compare(sample, caseSensitive)` from `Core/generated/Types/string.c`. The sort itself follows the addon's own `LL_LobbyManager.SortSlots` (stable insertion sort with a comparator). | PASS |
| II. Ask, Never Assume | One open decision (disconnected rows) was asked and answered in the spec. The `[` ordering was verified against the ASCII table, not assumed. | PASS |
| III. KISS | One comparator, one sort method, one predicate. No sort strategy object, no configuration. | PASS |
| IV. YAGNI | No attribute, no reverse order, no per-column sort; the spec asks for one fixed order. | PASS |
| V. Replication | Replicated values added: none. RPCs added: none. `RplSave`/`RplLoad`: unchanged. Visibility classification: local-only, derived from already-replicated names. | PASS |
| VI. Comments and Hygiene | Two WHY comments: why an explicit tag rule exists, why Z order instead of recreation. `LL_` naming kept; no dead code left (the formatter's inline check is replaced, not duplicated). | PASS |
| VII. Optional Integrations | Not touched. | PASS |
| Engine and Asset Constraints | No `.et`, `.layout`, `.acp` or `.meta` change. | PASS |
| User Interface | No new widget; the existing rows keep focus, sounds and search behaviour. In-game review at 127 rows is a quickstart step. | PASS |
| Localization | No new strings. | PASS |
| Testability | Reproducible in the demo world with several clients; dedicated server for the join/leave scenarios. | PASS |

Workbench steps for the operator: none. Asset files to double-check: none.

## Project Structure

### Documentation (this feature)

```text
specs/001-player-list-order/
├── plan.md              # This file
├── research.md          # Phase 0: ordering facts and vanilla patterns
├── data-model.md        # Phase 1: the derived ordering key
├── quickstart.md        # Phase 1: validation in the demo world
└── tasks.md             # Phase 2 (/speckit-tasks)
```

No `contracts/`: the feature exposes no interface to players, mission makers or other
systems.

### Source Code (repository root)

```text
Lite Lobby/scripts/game/
├── Core/
│   └── LL_LobbyManager.c      # HasUnitTag predicate; FormatPlayerNameRich uses it
└── UI/
    ├── LL_CoopLobby.c         # SortPlayerList + PlayerOrderedBefore; sort call sites
    └── LL_PlayerSelector.c    # GetPlayerName accessor for the comparator
```

**Structure Decision**: the change stays inside the three classes that already own the
name rule, the row and the panel. No new file.

## Design

1. `LL_LobbyManager.HasUnitTag(string)`: static, leading `[` and `]` at index 2 or
   later. `FormatPlayerNameRich` calls it instead of repeating the test (FR-008).
2. `LL_PlayerSelector.GetPlayerName()`: returns the stored plain name; the rich text
   in the widget is never compared (it carries markup).
3. `LL_CoopLobby.PlayerOrderedBefore(a, b)`: tagged before untagged; otherwise
   `nameA.Compare(nameB, false) < 0`. Equal names return false, which keeps the
   insertion sort stable (edge case "identical names").
4. `LL_CoopLobby.SortPlayerList()`: copies the row handlers out of the map, stable
   insertion sort with the comparator, then `root.SetZOrder(index)` per row (FR-005).
5. Call sites: once after the loop in `BuildPlayerList` (FR-004 rebuild), after
   `UpdateName` in `OnPlayerNameUpdated` (FR-004 name update), after `AddPlayer` in
   `OnPlayerConnectionChanged` when a row was created (FR-004 late join). `AddPlayer`
   itself stays unsorted so a bulk build sorts once.
6. Disconnect state is not consulted (FR-010). The search filter only toggles
   visibility, so it is unaffected (FR-009).

## Complexity Tracking

No violations to justify.
