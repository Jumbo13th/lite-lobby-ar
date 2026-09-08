# Research: Player List Order

## R1. How the vanilla game reorders a list of player rows

- **Decision**: reorder rows in place with `Widget.SetZOrder(index)`; rows stay children
  of the same `VerticalLayoutWidget`.
- **Rationale**: `SCR_PlayerListMenu.SortByName` (`scripts/Game/UI/HUD/SCR_PlayerListMenu.c`)
  sorts the names, then sets each row's Z order to its index. Its rows are created into
  the "Table" widget, a `VerticalLayoutWidgetClass` in
  `GameData/UI/layouts/Menus/PlayerList/PlayerListMenu.layout`. The lobby's `PlayersList`
  is also a `VerticalLayoutWidget`, so the same call orders it. Nothing is recreated,
  which is what FR-005 requires and what keeps a hover, a focus or an open context menu
  on a row alive across a re-sort.
- **Alternatives considered**: clearing the panel and re-adding rows in order (rejected:
  recreates 127 widgets per roster event and drops focus); `RemoveFromHierarchy` plus
  re-add of the moved row only (rejected: there is no insert-at-index on a layout
  widget in the official scripts).

## R2. How to compare names

- **Decision**: `string.Compare(sample, caseSensitive = false)` from
  `scripts/Core/generated/Types/string.c`, applied to the stored plain name.
- **Rationale**: it is the engine's own case-insensitive comparison and the only
  comparison primitive the official scripts expose on `string`. The plain name is kept
  by `LL_PlayerSelector` for the search filter already; the widget text carries rich
  markup for the tag colour and must not be compared.
- **Alternatives considered**: `array<string>.Sort()` as vanilla does (rejected: it is
  case-sensitive and cannot express the unit-first rule); lower-casing copies before
  comparing (rejected: `Compare` already folds case, and a copy per comparison is waste).
- **Known limit**: case folding is expected to cover Latin letters only. Non-Latin
  names compare by code point among themselves and sort after Latin names. The spec
  accepts this.

## R3. Why the unit-first rule must be explicit

- **Decision**: a separate predicate decides "has unit tag"; the comparator applies it
  before the alphabetical compare.
- **Rationale**: `[` is code point 91. Upper-case Latin letters are 65 to 90 and
  lower-case 97 to 122, so a plain compare places `[TT]Jumbo` after `Zed` and before
  `alice`. Case folding does not move the bracket. No string ordering yields
  "tagged first" without a rule.
- **Alternatives considered**: prefixing a sort key with a character below `[`
  (rejected: builds a string per comparison to encode one boolean).

## R4. Which sort

- **Decision**: stable insertion sort with a comparator, the pattern
  `LL_LobbyManager.SortSlots` already uses in this addon.
- **Rationale**: 127 rows, and after the first build every re-sort runs on an
  almost-sorted array where insertion sort is near linear. Stability gives the
  "identical names keep their relative order" edge case for free. The official
  scripts offer no comparator-based sort on `array<T>`.
- **Alternatives considered**: `array.Sort()` (no comparator); a hand-written quicksort
  (unstable, more code, no measurable gain at this size).

## R5. Where the tag rule lives

- **Decision**: `LL_LobbyManager.HasUnitTag(string)`, static, used by both
  `FormatPlayerNameRich` and the comparator.
- **Rationale**: FR-008. The formatter's inline test (`IndexOf("[") == 0` and a `]` at
  index 2 or later) becomes the single definition; the formatter keeps its behaviour
  exactly.
- **Alternatives considered**: a second copy of the test in the lobby UI (rejected: two
  rules drift).
