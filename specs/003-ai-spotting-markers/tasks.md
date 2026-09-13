---

description: "Task list for AI Spotting Markers Switch"
---

# Tasks: AI Spotting Markers Switch

**Input**: Design documents from `/specs/003-ai-spotting-markers/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: none requested; there is no automated test harness for game code. Validation
is the quickstart, run by the operator on a dedicated server.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository
root; all scripts live under `Lite Lobby/scripts/game/`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2)
- Include exact file paths in descriptions

## Phase 1: Setup

No setup: the feature edits one existing file and adds one.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the attribute both stories read.

- [X] T001 Add `[Attribute("0", UIWidgets.CheckBox, "...", category: "Lite Lobby")] protected bool m_bAllowAiSpotReports;` to `Lite Lobby/scripts/game/Core/LL_GameModeCoop.c` directly below `m_bRemoveRedundantUnits` (line 42). Description text: "Let AI groups report the enemies they identify as timestamped military markers on their own faction's map (a game feature since 1.8). Off by default: in a lobby mission those markers reveal player squads to the other side. A faction whose own settings forbid AI reports never reports, even when this is on." (FR-001, FR-002, FR-008). Keep CRLF line endings and the existing attribute formatting.
- [X] T002 Add `bool AllowAiSpotReports()	{ return m_bAllowAiSpotReports; }` to `Lite Lobby/scripts/game/Core/LL_GameModeCoop.c` directly below `RemoveRedundantUnits()` (line 122), aligned like its neighbours (data-model, research R2). Depends on T001.

**Checkpoint**: the addon compiles; the checkbox shows on `LL_GameMode_Lobby.et` in Workbench, unticked.

---

## Phase 3: User Story 1 - A squad's route stays secret (Priority: P1) 🎯 MVP

**Goal**: with the switch at its default, no AI group places a report marker, so no
client ever receives one.

**Independent Test**: quickstart Scenarios 1 to 4 on a dedicated server.

### Implementation for User Story 1

- [X] T003 [US1] Create `Lite Lobby/scripts/game/Modded/LL_M_SCR_AIGroupPerception.c` with `modded class SCR_AIGroupPerception` overriding `protected void MarkEnemyOnMap(notnull SCR_AITargetInfo target)` (vanilla parameter name kept): get `LL_GameModeCoop.GetInstance()`; if the game mode exists and `!AllowAiSpotReports()`, return; otherwise `super.MarkEnemyOnMap(target)`. One file-level WHY comment: this call is the only entry into the game's enemy-marking system, so returning here means no marker object exists to broadcast, store for join-in-progress or expire; a missing lobby game mode leaves the game's behaviour untouched (FR-003, FR-004, FR-007, research R1, R4). Same header style as `LL_M_SCR_PlayerNamesFilterCache.c`; ASCII, no other addon named.
- [X] T004 [US1] Read `Arma-Reforger-Script-Diff/scripts/Game/AI/Group/SCR_AIGroupPerception.c` (1.8.0.13 tag) once more and confirm the override signature matches character for character, including `notnull`, and that `MarkEnemyOnMap` is still the only caller of `SCR_AIEnemyMarkingSystem.MarkTarget` (grep `MarkTarget(` across `scripts/Game`). Record the result in the PR description.

**Checkpoint**: with the switch off, no report marker appears on any client (quickstart Scenarios 1 to 4).

---

## Phase 4: User Story 2 - A mission maker opts back into AI reports (Priority: P2)

**Goal**: with the switch on, the game's reports behave exactly as unmodified.

**Independent Test**: quickstart Scenario 5.

### Implementation for User Story 2

- [X] T005 [US2] Confirm by reading `Lite Lobby/scripts/game/Modded/LL_M_SCR_AIGroupPerception.c` that the on-path is a single `super` call with no other logic, so cooldown, lumping, lifetime, inaccuracy and the per-faction gate remain the game's (FR-004, FR-005, research R3). No code change expected; record in the PR description.
- [ ] T006 [US2] Operator: tick the new checkbox on the `LL_GameMode_Lobby` entity in `Lite Lobby/worlds/Arland/LobbyDemo_Layers/a_systems.layer` through Workbench, run quickstart Scenario 5, then untick and save so the demo world ships with the default.

**Checkpoint**: both stories hold on a dedicated server (quickstart Scenarios 1 to 5).

---

## Phase 5: Polish & Cross-Cutting Concerns

- [X] T007 Review the diff of both files against constitution VI: WHY-only comments, `LL_M_` file naming, vanilla parameter name kept, CRLF preserved in `LL_GameModeCoop.c`, no name of any other project or person, no dead code.
- [ ] T008 Operator: compile in Workbench and run quickstart Scenarios 1, 2 and 5 on a dedicated server; record the result in the PR.
- [ ] T009 Operator: record quickstart Scenario 6 (traffic comparison with AI in sight of players) at the next full event before the feature is called done (SC-004).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Foundational (Phase 2)**: T001 → T002, same file. Blocks both stories.
- **User Story 1 (Phase 3)**: T003 after T002 (it calls the getter). T004 is read-only and can run any time.
- **User Story 2 (Phase 4)**: T005 after T003. T006 needs Workbench and a compiled addon.
- **Polish (Phase 5)**: after both stories. T008 needs Workbench; T009 needs an event.

### Parallel Opportunities

- T004 ‖ T001–T003 (read-only check in the reference repository).
- Nothing else: two source files, edited in order.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 2, then T003.
2. Compile; run quickstart Scenarios 1 and 2.
3. The trail is gone on every client; User Story 2 is already reachable by ticking the box, since the on-path is the vanilla call.

### Incremental Delivery

1. T005 and T006 confirm the on-path; Phase 5 review, then the PR.

---

## Notes

- Never commit without the operator's permission (constitution, Development Workflow).
- No `.et`, `.layout`, `.conf` or `.meta` file is touched by an agent. The only Workbench
  action besides compiling is the operator's temporary tick for Scenario 5.
