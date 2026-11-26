---

description: "Task list for Game Master Only Player Notices"
---

# Tasks: Game Master Only Player Notices

**Input**: Design documents from `/specs/006-gm-only-notifications/`

**Prerequisites**: plan.md, spec.md, research.md, quickstart.md

**Tests**: none requested; there is no automated test harness for game code. Validation
is the quickstart, run by the operator on a dedicated server.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository
root.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2)

## Phase 1: Setup

No setup: the feature adds one file.

---

## Phase 2: User Story 1 and User Story 2 (Priority: P1) 🎯 MVP

Both stories are the same change seen from two clients: a notice withheld from players
is a notice routed to Game Masters.

**Independent Test**: quickstart Scenarios 1 to 4.

- [X] T001 [US1] [US2] Create `Lite Lobby/scripts/game/Modded/LL_M_SCR_NotificationSenderComponent.c` with `modded class SCR_NotificationSenderComponent` overriding `OnPlayerRegistered(int playerId)`: without `LL_GameModeCoop.GetInstance()` call `super` and return; otherwise `SCR_NotificationsComponent.SendLocalUnlimitedEditor(ENotification.PLAYER_JOINED, playerId)` (FR-001, plan Design 1).
- [X] T002 [US1] [US2] In the same file override `OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)`: without a lobby game mode call `super` and return; otherwise copy the 1.9.0.37 body from `Arma-Reforger-Script-Diff/scripts/Game/GameMode/SCR_NotificationSenderComponent.c` unchanged except `SendToEveryone` becomes `SendToUnlimitedEditorPlayers` for `PLAYER_KICKED`, `PLAYER_BANNED_NO_DURATION`, `PLAYER_BANNED` and `PLAYER_LEFT`; `EDITOR_GM_LEFT` stays `SendToEveryone` (FR-002, FR-003, FR-004, FR-007, plan Design 2). Depends on T001.
- [X] T003 [US1] [US2] One file-level WHY comment: the notices are the Game Master's business in a lobby mission and Game Master only delivery saves one message per player per disconnect; the disconnect body is the game's, with the recipients changed, because `super` would broadcast first. Vanilla comments inside the copied body are not carried over (constitution VI).

**Checkpoint**: quickstart Scenarios 1 to 4 pass on a dedicated server.

---

## Phase 3: Polish & Cross-Cutting Concerns

- [X] T004 [P] Add the 1.1.2 entry to `docs/src/content/docs/changelog.mdx` and `docs/src/content/docs/ru/changelog.mdx`: one line, the join, leave, kick and ban notices are shown only to the Game Master; the Game Master left notice is still shown to everyone.
- [X] T005 Review the diff against constitution VI: WHY-only comment, `LL_M_` file name, vanilla parameter names, no other project named, no dead code; compare the copied body with the vanilla method line by line.
- [ ] T006 Operator: compile in Workbench and run quickstart Scenarios 1 to 5 on a dedicated server; record the result in the pull request.

---

## Dependencies & Execution Order

- T001 → T002 → T003 (same file).
- T004 is independent of T001 to T003.
- T005 after T001 to T004; T006 last.
