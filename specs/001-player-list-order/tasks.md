---

description: "Task list for Player List Order"
---

# Tasks: Player List Order

**Input**: Design documents from `/specs/001-player-list-order/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: none requested; there is no automated test harness for game code. Validation
is the quickstart, run by the operator.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository
root; all scripts live under `Lite Lobby/scripts/game/`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2)
- Include exact file paths in descriptions

## Phase 1: Setup

No setup: the feature touches three existing files and adds none.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the two accessors both stories sort on.

- [X] T001 [P] Add `static bool HasUnitTag(string name)` to `Lite Lobby/scripts/game/Core/LL_LobbyManager.c` directly above `FormatPlayerNameRich`: true when `name.IndexOf("[") == 0 && name.IndexOf("]") >= 2`; one WHY comment stating the rule is shared by colouring and ordering (FR-002, FR-008, research R5)
- [X] T002 Make `FormatPlayerNameRich` in `Lite Lobby/scripts/game/Core/LL_LobbyManager.c` return the name unchanged when `!HasUnitTag(name)` and remove its inline `IndexOf("[")` / `close < 2` checks; keep the `close` lookup and the colour markup exactly as they are; keep the existing "Render-time only" comment on the formatter (depends on T001)
- [X] T003 [P] Add `string GetPlayerName()` returning `m_sPlayerName` to `Lite Lobby/scripts/game/UI/LL_PlayerSelector.c` next to `GetPlayerId()` (data-model: the plain name, never the widget text)

**Checkpoint**: the addon compiles with no behaviour change.

---

## Phase 3: User Story 1 - Find a player in a full lobby (Priority: P1) 🎯 MVP

**Goal**: the panel reads tagged-first, then alphabetical ignoring case, identical on
every client.

**Independent Test**: quickstart Scenario 1 on a dedicated server with the six named
clients.

### Implementation for User Story 1

- [X] T004 [US1] Add `protected static bool PlayerOrderedBefore(LL_PlayerSelector a, LL_PlayerSelector b)` to `Lite Lobby/scripts/game/UI/LL_CoopLobby.c`: read both names via `GetPlayerName()`, evaluate `LL_LobbyManager.HasUnitTag` for each, return `unitA` when the flags differ, else `nameA.Compare(nameB, false) < 0`; WHY comment: `[` sorts after upper-case and before lower-case letters, so the tag rule is explicit (FR-001 to FR-003, research R2, R3)
- [X] T005 [US1] Add `protected void SortPlayerList()` to `Lite Lobby/scripts/game/UI/LL_CoopLobby.c` directly after `BuildPlayerList`: copy the handlers out of `m_mPlayers` into a local `array<LL_PlayerSelector>`, stable insertion sort with `PlayerOrderedBefore` (same shape as `LL_LobbyManager.SortSlots`), then `GetRootWidget().SetZOrder(i)` for each row with a null check; WHY comment: rows are reordered by Z order as the vanilla scoreboard does, so no row is recreated (FR-005, research R1, R4)
- [X] T006 [US1] Call `SortPlayerList()` in `BuildPlayerList` in `Lite Lobby/scripts/game/UI/LL_CoopLobby.c` once, after the `AddPlayer` loop and before `ApplyPlayerFilter()`; do not add a sort inside `AddPlayer` (FR-004 rebuild, FR-009)

**Checkpoint**: opening the lobby shows the ordered panel (quickstart Scenario 1 and 6).

---

## Phase 4: User Story 2 - The order survives roster changes (Priority: P2)

**Goal**: late joins, late names and rebuilds land rows in place; disconnects do not
move rows.

**Independent Test**: quickstart Scenarios 2 to 5 with the lobby kept open on one client.

### Implementation for User Story 2

- [X] T007 [US2] In `OnPlayerNameUpdated` in `Lite Lobby/scripts/game/UI/LL_CoopLobby.c`, call `SortPlayerList()` after `playerSel.UpdateName(name)` and before `ApplyPlayerFilter()`; this also covers a row created by the `AddPlayer` call at the top of the same method (FR-004 name update)
- [X] T008 [US2] In `OnPlayerConnectionChanged` in `Lite Lobby/scripts/game/UI/LL_CoopLobby.c`, when `connected && !m_mPlayers.Contains(playerId)` wrap the `AddPlayer(playerId)` in a block and call `SortPlayerList()` right after it; do not sort on the disconnect branch (FR-004 late join, FR-010)
- [X] T009 [US2] Confirm by reading `Lite Lobby/scripts/game/UI/LL_CoopLobby.c` that `OnPlayerRemoved`, `UpdatePlayerInList` and `OnPlayerReadyChanged` need no sort call (removal leaves order intact; ready and disconnect do not change the key) and record the result in the PR description

**Checkpoint**: both stories hold on a dedicated server (quickstart Scenarios 1 to 6).

---

## Phase 5: Polish & Cross-Cutting Concerns

- [X] T010 Review the diff of the three files against constitution VI: WHY-only comments, no leftover inline tag check, CRLF endings preserved, no name of any other project
- [ ] T011 Operator: compile in Workbench and run quickstart Scenarios 1, 2, 5 and 6 on a dedicated server; record the result in the PR
- [ ] T012 Operator: run quickstart Scenario 7 (127 players, traffic comparison) at the next full event and record the result before the feature is called done (SC-002, SC-004)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Foundational (Phase 2)**: T001 and T003 in parallel; T002 after T001. Blocks both stories.
- **User Story 1 (Phase 3)**: after Phase 2. T004 → T005 → T006, all in one file.
- **User Story 2 (Phase 4)**: after T005 exists. T007 and T008 touch different methods of
  the same file, run sequentially. T009 is a read-only check.
- **Polish (Phase 5)**: after both stories. T011 needs Workbench; T012 needs an event.

### Parallel Opportunities

- T001 ‖ T003 (different files).
- Nothing else: the remaining edits are in `LL_CoopLobby.c`.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 2, then Phase 3.
2. Compile; run quickstart Scenario 1.
3. The list is ordered on open; late joiners still append at the bottom until Phase 4.

### Incremental Delivery

1. Phase 4 adds the three call sites; compile; run Scenarios 2 to 5.
2. Phase 5 review, then the PR.

---

## Notes

- Never commit without the operator's permission (constitution, Development Workflow).
- No `.et`, `.layout` or `.meta` file is touched; there are no Workbench steps besides
  compiling.
