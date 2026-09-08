# Data Model: Player List Order

No entity is added, stored or replicated. The feature derives one ordering key per row
from data the client already holds.

## Ordering key (derived, never stored)

| Field | Source | Type | Rule |
|-------|--------|------|------|
| `hasUnitTag` | plain player name | bool | name begins with `[` and has a `]` at index 2 or later |
| `name` | plain player name held by the row (`LL_PlayerSelector`) | string | compared with the engine's case-insensitive string compare |

Order: rows with `hasUnitTag = true` first; within each group ascending by `name`;
equal keys keep their existing relative order.

## Inputs the feature reads

- The row map of the lobby panel (`player id → row handler`), already maintained by
  `LL_CoopLobby` for add, update and remove.
- The plain name on each row, already kept for the search filter.

## What the feature deliberately ignores

- Ready flag, faction, role, squad, disconnect state (FR-010), selection highlight.
- The rich-text name shown in the widget, which carries colour markup.
