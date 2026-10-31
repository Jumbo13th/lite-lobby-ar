# Tasks: Shared Map Drawing

**Input**: Accepted specification and plan in this directory.
**Verification**: Existing source checks and operator scenarios in quickstart.md; no new game-test framework.

## Phase 1: Setup

- [x] T001 Verify official map and widget APIs and accepted integration decisions in specs/005-map-drawing/research.md.
- [x] T002 Establish mechanically renamed source modules under Lite Lobby/scripts/game/Map/ and existing validation under scripts/WorkbenchGame/.

## Phase 2: Foundation

- [x] T003 Preserve document/geometry/replica algorithms in Lite Lobby/scripts/game/Map/LL_MapPaintDocument.c, LL_MapPaintGeometry.c, LL_MapPaintReplication.c and their existing helpers (FR-002, FR-005, FR-011, FR-016).

## Phase 3: User Story 1 - Draw and revise a plan

Goal: Existing map screens support drawing without changing their usual behavior.
Independent check: All five tools, partial erase, pan/zoom/reopen, shortcuts and twenty screen-switch cycles.

- [x] T004 [US1] Merge drawing/map/cursor lifecycle hooks into Lite Lobby/scripts/game/Modded/LL_M_SCR_MapDrawingUI.c, LL_M_SCR_MapEntity.c, LL_M_SCR_MapCursorModule.c and LL_M_SCR_CursorCustom.c (FR-001, FR-006, FR-007).
- [x] T005 [US1] Guard map-select, dragging and deletion in Lite Lobby/scripts/game/Map/LL_MapMarkersUI.c and history-hover zoom in LL_MapCursorModule.c (FR-007, FR-018).
- [x] T006 [US1] Merge drawing actions and context into Lite Lobby/Configs/System/chimeraInputCommon.conf and scope Shift erasing in scripts/game/Map/LL_MapPaintHotkeys.c (FR-004, FR-018).
- [x] T007 [US1] Apply existing lobby colors, typography, hover/selected/disabled styling and non-color selection indication in Lite Lobby/scripts/game/Map/LL_MapPaintSettings.c, LL_MapPaintPanel.c, LL_MapPaintWidgets.c and LL_MapPaintToolIcon.c (FR-003, FR-017).

## Phase 4: User Story 2 - Share a faction plan safely

Goal: Preserve faction delivery and enforce the existing lobby permissions.
Independent check: Dedicated demo server, two factions, direct denied requests and late join/reconnect.

- [x] T008 [US2] Merge drawing server state/transport into Lite Lobby/scripts/game/Modded/LL_M_SCR_MapMarkerManagerComponent.c and LL_M_SCR_MapMarkerSyncComponent.c without changing payload shapes (FR-009, FR-010, FR-011).
- [x] T009 [US2] Reuse the existing server marker edit gate with rejection acknowledgement and the existing client policy in Lite Lobby/scripts/game/Map/LL_MapPaintPermissions.c (FR-008).
- [x] T010 [US2] Merge idempotent marker delivery and safe iteration into Lite Lobby/scripts/game/Modded/LL_M_SCR_MapMarkerManagerComponent.c, retaining trusted delivery and faction checks (FR-007, FR-009).

## Phase 5: User Story 3 - Inspect changes

Goal: Preserve author inspection, grouping and frozen faction history.
Independent check: Two authors, different tools and erase targets, expiry and scroll isolation.

- [x] T011 [US3] Retain history grouping/snapshot behavior and apply lobby presentation to Lite Lobby/scripts/game/Map/LL_MapPaintHistory.c, LL_MapPaintHistoryPanel.c and LL_MapPaintAuthorTooltip.c (FR-012, FR-013, FR-017).
- [x] T012 [US3] Localize visible drawing/history/readiness strings in Lite Lobby/Language/ll_localization.st and at the drawing UI text boundaries; regenerate runtime locales with tools/generate-runtime-locales.ps1 (FR-014).

## Phase 6: Verification and operator acceptance

- [x] T013 Check normalized source parity, class references, edited resource structure and generated localization for Lite Lobby/scripts/game/Map/LL_MapPaint*.c and scripts/game/Modded/LL_M_SCR_*.c (FR-016; SC-007).
- [x] T014 Record implementation/asset review steps in specs/005-map-drawing/quickstart.md and update task completion without claiming runtime checks.
- [ ] T015 Operator reviews changed input/localization resources in Lite Lobby/addon.gproj and runs the existing LL_MapPaintTests Workbench validation (FR-001; SC-001).
- [ ] T016 Operator performs bilingual visual and twenty-cycle acceptance in Lite Lobby/Missions/LobbyDemo.conf, using specs/005-map-drawing/contracts/ui.md (FR-017, FR-018; SC-004, SC-005, SC-008).
- [ ] T017 Operator verifies authority, faction privacy, late join and history on a dedicated LobbyDemo server, including 127 occupied slots (FR-008 through FR-013, FR-015; SC-002, SC-003, SC-006).

- [x] T018 Compile Lite Lobby/addon.gproj in diagnostic Workbench; fix the misplaced tooltip helper in scripts/game/Map/LL_MapPaintPanel.c and confirm successful Game/WorkbenchGame compilation.

## Dependencies and Delivery

T001 → T002 → T003 → US1 → US2 → US3 → T013/T014 → operator acceptance.

US1 provides the first playable increment; faction sharing and history extend it. Tasks touching the same modded classes run sequentially. Presentation settings and locale source can be prepared independently after integration interfaces settle, but both must be checked before delivery. Use the existing demo and validation plugin rather than adding another test system.


