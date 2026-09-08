# Feature Specification: Player List Order

**Feature Branch**: `001-player-list-order`

**Created**: 2026-09-08

**Status**: Draft

**Input**: User description: "Order the player list in the lobby (slot-selection screen) alphabetically by player name, with players who carry a unit tag listed first. A unit tag is a leading bracketed prefix in the player name, e.g. "[TT]Jumbo". Within each group (tagged / untagged) the order is alphabetical by the full name ignoring letter case, so members of one unit end up next to each other. The order must hold after a late join, after a name update and after a full list rebuild, and must not require recreating the row widgets. Client-side only; nothing new goes on the network. Engine finding to take into account: "[" is ASCII 91, which sorts after upper-case letters and before lower-case ones, so a plain string compare cannot produce the required order and an explicit unit-first rule is needed."

## Current State

The lobby screen has a player panel on the right listing every connected player with
their name, ready mark, faction colour and current role or squad. Rows are added in
the order the client learns about players: the roster snapshot on open, then each
late joiner as their name arrives. Nothing reorders them afterwards, so on a full
server the panel is 127 rows in effectively random order, different on every client,
and a squad leader looking for a specific person scrolls or uses the search box.

Unit tags already have meaning in the lobby: a name that starts with a bracketed
prefix (`[TT]Jumbo`) is drawn with the prefix in the accent colour. The tag rule is
implemented once, inside the name formatter, and nothing else uses it.

The roster itself (who is present, their names, ready flags, disconnect state) is
server-authoritative and replicated per player already. This feature changes only
how one client arranges rows it already has; it adds nothing to the roster and
nothing to the network.

## Clarifications

### Session 2026-09-08

- Q: Where should a player who has disconnected but still holds a reserved slot appear in the ordered list? → A: the row keeps its alphabetical place; only its colour changes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Find a player in a full lobby (Priority: P1)

A player opens the lobby on a full server and scans the player panel for a specific
person. The list reads top to bottom in alphabetical order by name, ignoring letter
case, so the eye lands on the right region at once. Players who wear a unit tag are
listed before those who do not, and because the tag is part of the name, the members
of one unit sit next to each other in a block.

**Why this priority**: it is the whole feature. Every other story only protects this
order from being disturbed.

**Independent Test**: join a server with a mix of tagged, untagged, upper-case and
lower-case names, open the lobby, read the panel top to bottom.

**Acceptance Scenarios**:

1. **Given** players `bravo`, `Alpha`, `[TT]Jumbo`, `[TT]Jidai`, `[ABC]Zed`, `charlie`
   are connected, **When** the lobby opens, **Then** the panel reads
   `[ABC]Zed`, `[TT]Jidai`, `[TT]Jumbo`, `Alpha`, `bravo`, `charlie`.
2. **Given** two clients on the same server, **When** both open the lobby, **Then**
   both panels show the same order.
3. **Given** the same players, **When** the panel is read, **Then** no untagged name
   appears above a tagged one, whatever its first letter or case.

---

### User Story 2 - The order survives roster changes (Priority: P2)

While the lobby is open, players keep joining, leaving, reconnecting and, on the
first moments after joining, receiving their real name. Every such change lands the
affected row in its correct alphabetical place instead of at the bottom.

**Why this priority**: without it the P1 order decays within minutes on a busy
server, exactly when it is needed most.

**Independent Test**: keep the lobby open on one client while others join, leave and
rejoin; the panel stays alphabetical throughout.

**Acceptance Scenarios**:

1. **Given** an open lobby in alphabetical order, **When** a player named `Mike`
   joins, **Then** `Mike` appears between the names alphabetically before and after
   it, not at the end.
2. **Given** a row that was created before its owner's name was known, **When** the
   name arrives, **Then** the row moves to the place that name dictates.
3. **Given** an open lobby, **When** the list is rebuilt (for example after the roster
   re-syncs), **Then** the rebuilt list is in the same order as before.
4. **Given** a player who disconnects and stays reserved, **When** the panel is read,
   **Then** the greyed row keeps its alphabetical place.

---

### Edge Cases

- A name that is only a bracket, or starts with `[` but has no closing bracket, or
  has nothing between the brackets (`[]Name`), is not a unit tag and sorts with the
  untagged names.
- Two players with identical names keep a stable relative order; the list does not
  flicker between the two on each update.
- A player whose name has not arrived yet (empty name) sorts before everything and
  moves once the name is known.
- Names in non-Latin scripts (Cyrillic is the common case) sort consistently among
  themselves and after Latin names; exact collation across scripts is not required.
- Filtering with the search box hides rows without changing the order of the rows
  that remain visible.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The lobby player panel MUST list players in ascending alphabetical order
  of their full name, comparing letters without regard to case.
- **FR-002**: Players whose name carries a unit tag MUST be listed before all players
  whose name does not. A unit tag is a name that begins with `[`, contains a `]`, and
  has at least one character between them.
- **FR-003**: Within the tagged group and within the untagged group, the order MUST
  be by the full name including the tag, so members of one unit form a contiguous
  block.
- **FR-004**: The order MUST be restored after each of: a player joining after the
  panel was built, a player's name being updated, a full rebuild of the panel.
- **FR-005**: Reordering MUST reuse the existing rows; rows are not destroyed and
  recreated to change their position.
- **FR-006**: The order MUST be identical on every client for the same roster.
- **FR-007**: The feature MUST add no replicated value, no remote procedure call and
  no change to the join-in-progress payload.
- **FR-008**: The tag rule used for ordering MUST be the same rule the name formatter
  uses to colour the tag, so a name is never coloured as a unit but sorted as a
  player, or the reverse.
- **FR-009**: The player search filter MUST keep working unchanged over the ordered
  list.
- **FR-010**: A player who disconnects but keeps a reserved slot MUST keep their
  alphabetical place; disconnect state does not move a row.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With 127 connected players, a tester given a name finds its row without
  using the search box in under 5 seconds.
- **SC-002**: With 127 connected players, opening the lobby and every later roster
  change shows no visible hitch attributable to the ordering.
- **SC-003**: Across three clients on one server, the panel order is identical on all
  three at any moment of a 10-minute session with joins, leaves and reconnects.
- **SC-004**: Network traffic per client during lobby is unchanged compared to the
  build before this feature, measured on a dedicated server.

## Assumptions

- Scope is the lobby player panel only. The spectator player list, the voice-room
  lists and the briefing roster keep their current order; ordering them is a separate
  feature if wanted.
- "Alphabetical" means the engine's case-insensitive string comparison. It folds
  Latin letters; non-Latin scripts fall back to code-point order, which is accepted.
- Disconnected-but-reserved players stay in their alphabetical place; they are not
  moved to the bottom.
- The unit-tag rule is the one the name formatter already applies: leading `[`, a
  `]` at position 2 or later. Tags are not normalised (`[tt]` and `[TT]` are the same
  unit only because the comparison ignores case).
- No mission-maker attribute is added; the order is not configurable.
