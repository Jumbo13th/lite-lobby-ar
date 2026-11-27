---

description: "Task list for Map Line Drawing"
---

# Tasks: Map Line Drawing

**Input**: Design documents from `/specs/007-map-drawing/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: none requested; there is no automated test harness for game code. Validation is the
quickstart, run by the operator on a dedicated server.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root;
`LL/` stands for `Lite Lobby/`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1 to US4)

## Phase 1: Setup

**Purpose**: the branch carries only what this design uses.

- [X] T001 Delete the files of the branch's first commit that this design does not use: `LL/scripts/game/Map/LL_MapPaint*.c` (24 files), `LL/scripts/WorkbenchGame/LL_MapPaintTests.c`, `LL/scripts/game/Modded/LL_M_SCR_CursorCustom.c`, `LL/scripts/game/Modded/LL_M_SCR_MapCursorModule.c`, `LL/scripts/game/Modded/LL_M_SCR_MapEntity.c`, `LL/scripts/game/Modded/LL_M_SCR_MapMarkersUI.c`, `specs/005-map-drawing/` (plan, Deleted).
- [X] T002 Return to their `origin/main` content: `LL/Configs/System/chimeraInputCommon.conf`, `LL/Language/ll_localization.st`, the 13 `LL/Language/ll_localization.*.conf` tables, `LL/scripts/game/Map/LL_MapCursorModule.c`, `LL/scripts/game/Map/LL_MapMarkersUI.c`, `LL/scripts/game/Modded/LL_M_SCR_MapDrawingUI.c`, `LL/scripts/game/Modded/LL_M_SCR_MapMarkerManagerComponent.c`, `LL/scripts/game/Modded/LL_M_SCR_MapMarkerSyncComponent.c`; then `git diff origin/main --stat -- "Lite Lobby"` must list nothing. Depends on T001.

**Checkpoint**: the addon equals `main`; no reference to `LL_MapPaint` remains (`git grep LL_MapPaint`).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the record, the two stores, the shared rules, the input definitions and an empty map
component every story builds on.

- [X] T003 [P] Create `LL/scripts/game/Map/LL_MapLine.c`: class `LL_MapLine` with the client and server fields of data-model.md, constants `MAX_POINTS = 64`, `MAX_LINES_PER_PLAYER = 20`, `MAX_LINES_PER_FACTION = 150`, `MIN_INTERVAL_S = 2`, `MIN_WIDTH_M = 0.1`, `MAX_WIDTH_FRACTION = 0.05`; static helpers converting world metres to decimetre ints and back, and `ComputeBounds()` filling the ground bounding box (research R5).
- [X] T004 [P] Create `LL/scripts/game/Map/LL_MapLineCache.c`: client singleton `LL_MapLineCache` with `Add(LL_MapLine)` (ignores a known id, keeps the array sorted by id), `Remove(int id)`, `Reset()`, `GetLines()`, `CountFaction()`, `CountOwn()`, and one `ScriptInvoker` fired on every change (data-model.md, Map line (client)).
- [X] T005 Create `LL/scripts/game/Map/LL_MapLineStore.c`: server class `LL_MapLineStore` holding `ref array<ref LL_MapLine>` ordered by id, the next id, last accepted time per identity id; helpers `FindLine(id)`, `CountByAuthor(uid)`, `CountByFaction(key)`, and `GetFactionRecipients_S(FactionKey, out array<int>)` from `PlayerManager.GetPlayers` filtered by `SCR_FactionManager.SGetPlayerFaction`. Depends on T003.
- [X] T006 In `LL/scripts/game/Core/LL_LobbyManager.c` create the store on the authority in `OnPostInit` and expose `GetMapLineStore()` (plan Design 2). Depends on T005.
- [X] T007 [P] Make the shared rules callable without changing them: `LL_CanEditSharedMarkers` public in `LL/scripts/game/Modded/LL_M_SCR_MapMarkerSyncComponent.c`; in `LL/scripts/game/Map/LL_MapMarkersUI.c` make `LL_CanEditMarkers` and `LL_FormatMarkerAge` static and public and add a static getter and setter for `s_iLastColorEntry`; make the label style constants in `LL/scripts/game/Modded/LL_M_SCR_MapMarkerWidgetComponent.c` public static consts (research R7, R9, R11).
- [X] T008 [P] In `LL/Configs/System/chimeraInputCommon.conf` add action `LL_MapLineStart` (Left Ctrl, Right Ctrl) and reference it in `LL_BriefingMapContext`; add context `LL_MapLineContext` (Priority 60, Flags `0xc`) defining `LL_MapLineFinish` (Enter, numpad Enter) and `LL_MapLineCancel` (Escape) and referencing exactly the actions in contracts/input.md; fresh object ids. In `LL/Configs/System/keyBindingMenu.conf` add rows for the three line actions (`#LL-Keybind_MapLineStart`, `_MapLineFinish`, `_MapLineCancel`); add those keys in English and Russian to `LL/Language/ll_localization.st` and regenerate the runtime tables with `tools/generate-runtime-locales.ps1`.
- [X] T009 Create `LL/scripts/game/Map/LL_MapLinesUI.c`: `LL_MapLinesUI : SCR_MapUIBaseComponent` that on map open creates `UI/Map/ZoneCanvas.layout` in the map frame with Z order -450, subscribes to the cache invoker, and on map close removes the canvas and unsubscribes; add its entry to `LL/Configs/Map/LL_MapBriefing.conf` with a fresh id (plan Design 6). Depends on T004.

**Checkpoint**: compiles; the maps open and close with an empty canvas, nothing else changes.

---

## Phase 3: User Story 1 - Draw a line and share it with the faction (Priority: P1) 🎯 MVP

**Goal**: Ctrl + click, clicks, Enter, colour, OK; the faction sees the line.

**Independent Test**: quickstart steps 1, 2, 3 and the limit and rate parts of step 9.

- [X] T010 [US1] In `LL/scripts/game/Map/LL_MapLineStore.c` add `Add_S(int playerId, int colour, float width, array<int> points)`: the checks of contracts/network.md in that order (marker edit rule through the player's `SCR_MapMarkerSyncComponent.LL_CanEditSharedMarkers`; points count; world box from `GetGame().GetWorld().GetBoundBox`; colour count of the placed-marker entry; width range written to reject NaN; limits by identity and faction; 2 s rate by identity); on success store with the next id, the author's identity id and name, the faction key and `GetTimestamp()`, then deliver to the faction recipients with each recipient's `own` flag; on failure return the reason code (FR-009, FR-010, FR-011). Depends on T005, T007.
- [X] T011 [US1] In `LL/scripts/game/Core/LL_LobbyPlayerComponent.c` add `RpcAsk_AddMapLine(int colour, float width, array<int> points)` (forwards to the store, answers a refusal with `RpcDo_OwnerMapLineRefused`), `RpcDo_OwnerAddMapLine(int lineId, int colour, float width, int ageSeconds, string authorName, bool own, array<int> points)` (builds an `LL_MapLine` with `createdAt = GetServerTimestamp().PlusSeconds(-ageSeconds)` and adds it to the cache), `RpcDo_OwnerMapLineRefused(int reason)` (translated message through `ShowCommandHelp`), a server helper `SendMapLine_S(LL_MapLine, bool own)` and a client helper `AskAddMapLine(...)` (contracts/network.md). Depends on T004, T010.
- [X] T012 [P] [US1] Add `#LL-MapLines_RefusedNotAllowed`, `_RefusedPlayerLimit`, `_RefusedFactionLimit`, `_RefusedTooSoon`, `_RefusedInvalid`, `_PointLimit` and the dialog header `#LL-MapLines_DialogTitle` in English and Russian to `LL/Language/ll_localization.st`; regenerate the runtime tables.
- [X] T013 [P] [US1] In `LL/scripts/game/UI/Map/LL_InsertMarkerDialog.c` add `InitLine(notnull SCR_MapMarkerEntryPlaced config, int colorEntry)`: hides `PreviewFrame`, `EditBoxRoot` and `TypeCombo` from script, sets the header to `#LL-MapLines_DialogTitle`, focuses the colour list; confirm reports the colour as today (research R11).
- [X] T014 [US1] In `LL/scripts/game/Map/LL_MapLinesUI.c` implement the line in progress: `MapSelect` listener that starts a line when `LL_MapLineStart` is held, `LL_MapMarkersUI.LL_CanEditMarkers()` allows and the cache counts are under the limits (otherwise the matching message), adds a point on every later click (message once at 64), activates `LL_MapLineContext` every frame, re-checks the edit rule every frame, finishes on `LL_MapLineFinish` with 2+ points, discards on `LL_MapLineCancel`, on map close and on a lost right (contracts/input.md). The start dot and points are drawn in the last confirmed colour. Depends on T008, T009.
- [X] T015 [US1] In `LL/scripts/game/Map/LL_MapMarkersUI.c` make `OnInputMapSelect` return early while `LL_MapLinesUI` has a line in progress or `LL_MapLineStart` is held (FR-003). Depends on T014.
- [X] T016 [US1] In `LL/scripts/game/Map/LL_MapLinesUI.c` open the dialog on finish with `InitLine`, driven as in `LL_MapMarkersUI.CreateLLDialog` (one-frame gap, `MapMarkerEditContext` every frame, `MENU_ACTION_SELECT` and `MENU_ACTION_BACK` listeners, `HandleDialog`, outside double click cancels); OK stores the colour as the last confirmed one, sends `AskAddMapLine` and discards the local line; Cancel discards (FR-004, FR-005). Depends on T011, T013, T014.
- [X] T017 [US1] In `LL/scripts/game/Map/LL_MapLinesUI.c` render the cache: one `LineDrawCommand` per line and one 16-sided `PolygonDrawCommand` start dot, colour from the placed-marker entry with alpha 0.6, ascending id, plus the line in progress on top. Depends on T014.

**Checkpoint**: quickstart steps 1 to 3 pass; refusals of step 9 show their messages.

---

## Phase 4: User Story 2 - A line behaves like a map feature (Priority: P1)

**Goal**: width fixed on the ground from the zoom at the start, scaling with the map, cheap to draw.

**Independent Test**: quickstart step 4 and the frame-time part of step 11.

- [X] T018 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` set the width at the start to `0.02 × map frame height / GetCurrentZoom()`; draw `width × GetCurrentZoom()` pixels, at least 1; dot radius 0.75 × the drawn width (FR-006, research R2). Depends on T017.
- [X] T019 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` project once per frame (world origin through `WorldToScreen(..., true)`, then `origin + (x × ppu, -y × ppu)`), rebuild commands only when zoom, pan or cache changed or a line is in progress, and skip lines whose bounding box is outside `GetMapVisibleFrame` (SC-005, research R2). Depends on T018.

**Checkpoint**: quickstart step 4 passes.

---

## Phase 5: User Story 3 - See who drew a line and remove it (Priority: P2)

**Goal**: delete own lines, admins delete any, everyone sees who drew a line and when.

**Independent Test**: quickstart steps 5 and 6.

- [X] T020 [US3] In `LL/scripts/game/Map/LL_MapLineStore.c` add `Remove_S(int playerId, int lineId)` with the checks of contracts/network.md and delivery of the removal to the faction; in `LL/scripts/game/Core/LL_LobbyPlayerComponent.c` add `RpcAsk_RemoveMapLine(int lineId)` and `RpcDo_OwnerRemoveMapLine(int lineId)` (FR-012). Depends on T011.
- [X] T021 [US3] In `LL/scripts/game/Map/LL_MapLinesUI.c` listen to `MapMarkerDelete`: skip when one of the player's removable markers is under the cursor; otherwise find the topmost line the player may delete (`own` or `SCR_Global.IsAdmin()`) whose dot or a segment is within max(half drawn width, 6 px) of the cursor and ask its removal; refused on the spectator map and when `LL_CanEditMarkers` refuses (research R10). Depends on T019, T020.
- [X] T022 [US3] In `LL/scripts/game/Map/LL_MapLinesUI.c` poll `LL_MapMarkerInfo`: while held, one text widget per visible start dot with `authorName (age)`, age from `LL_MapMarkersUI.LL_FormatMarkerAge`, style from the marker label constants, `IGNORE_CURSOR | NOFOCUS`, repositioned every frame; removed on release and on map close (FR-017, research R9). Depends on T019.

**Checkpoint**: quickstart steps 5 and 6 pass.

---

## Phase 6: User Story 4 - Joining, reconnecting and changing sides (Priority: P1)

**Goal**: every player always holds exactly the lines of their current faction, also after a
session resume.

**Independent Test**: quickstart steps 7, 8 and 10, and the resume part of step 11.

- [X] T023 [US4] In `LL/scripts/game/Map/LL_MapLineStore.c` add `StartSync_S(int playerId, Faction faction)` and the paced tick (250 ms `CallLater`, 10 lines per job, 100 in total, stops when idle; jobs dropped for disconnected players; deleted lines skipped); in `LL/scripts/game/Core/LL_LobbyPlayerComponent.c` add `RpcDo_OwnerResetMapLines()` clearing the cache (FR-013, research R6). Depends on T011.
- [X] T024 [US4] In `LL/scripts/game/Core/LL_LobbyManager.c` call `StartSync_S` from `SetPlayerEngineFaction_S` after `SetFaction_S`, for a faction and for none, beside the marker re-send. Depends on T023.
- [X] T025 [P] [US4] Create `LL/scripts/game/Persistence/LL_MapLineData.c`: `LL_MapLineRecord`, `LL_MapLineData : PersistentState` holding pending records and restoring them into the store when the system turns `ACTIVE`, `LL_MapLineSerializer : ScriptedStateSerializer` version 1 writing every stored line with its age (research R12, data-model.md). Depends on T005.
- [X] T026 [US4] In `LL/scripts/game/Map/LL_MapLineStore.c` add the snapshot for the serializer and `Restore_S(records)`: insert with their ids, continue ids above the highest, start a sync for every player who has a faction (FR-015). Depends on T023, T025.
- [X] T027 [P] [US4] In `LL/Configs/Systems/Persistence/LL_Persistence.conf` add a `StatePersistenceConfig` with `LL_MapLineSerializer` in the `LL_Lobby` collection and `LL_MapLineData` under `PersistentStates`, fresh ids. Depends on T025.

**Checkpoint**: quickstart steps 7, 8 and 10 pass.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T028 [P] In `LL/scripts/game/UI/Briefing/LL_MissionDescriptionUI.c` add the map lines heading and paragraph to `BuildCommsGuideMarkup`, and their keys `#LL-CommsGuide_LinesHeader` and `#LL-CommsGuide_LinesBody` (keys in `<b>`: Ctrl + click, click, Enter, Escape, Delete, Left Shift) in English and Russian to `LL/Language/ll_localization.st`; regenerate the runtime tables (FR-018).
- [X] T029 [P] Add the changelog entry in `docs/src/content/docs/changelog.mdx` and `docs/src/content/docs/ru/changelog.mdx` under the next Workshop version: lines on the map (Ctrl + click, clicks, Enter, colour), shared with the faction, Left Shift shows who drew them, Delete removes your own, kept through a session resume.
- [X] T030 [P] Update `../ARCHITECTURE.md` (workspace): the replication contract lists the six map line RPCs as faction-scoped owner RPCs on `LL_LobbyPlayerComponent`, "all map drawing" leaves the Nothing row, and the components section gets the line store, cache and map UI.
- [X] T031 Review the whole diff against the constitution: WHY-only comments, `LL_` names, vanilla parameter names in overrides, no project or person named, no dead code, every `RplSave`/`RplLoad` untouched, every new conf id unique; `git grep -n -i "paint"` finds nothing of the earlier tool set.
- [X] T032 Operator: compile in Workbench and play the first version (2026-11-27); feedback recorded in spec Clarifications, session 2026-11-27.

---

## Phase 8: Amendments after the first in-game test (2026-11-27)

**Purpose**: the spec's 2026-11-27 clarifications: width follows the zoom while drawing and is
capped at 50 m, Enter sends without a dialog, a double click recolours, the look goes to a
Claude Design round.

- [X] T033 [US2] Replace `MAX_WIDTH_FRACTION` with `MAX_WIDTH_M = 50` in `LL/scripts/game/Map/LL_MapLine.c` and use it in `LL_MapLineStore.Add_S` (`LL/scripts/game/Map/LL_MapLineStore.c`) (FR-006, FR-010).
- [X] T034 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` compute the line in progress' width every frame from the current zoom (clamped to 0.1..50 m) and render it with that width; fix it on Enter (FR-006, research R2). Depends on T033.
- [X] T035 [US1] In `LL/scripts/game/Map/LL_MapLinesUI.c` make Enter send the line at once in its colour (2-second rule checked first, line kept with a message), and make a double click while drawing take back the first click's point and open the colour-only dialog, whose OK recolours the line in progress, stores the colour and continues drawing, and whose Cancel only closes it (FR-004, FR-005, research R11). Depends on T034.
- [X] T036 [P] Update the 1.2.0 changelog entry in `docs/src/content/docs/changelog.mdx` and `docs/src/content/docs/ru/changelog.mdx` and the Communications paragraph in `LL/Language/ll_localization.st` (regenerate the runtime tables) to the new keys: Enter sends, double click changes the colour, 50 m cap.
- [X] T037 Prepare the Claude Design round for the line and start-point look: canvas https://claude.ai/artifact/UrPweJHXK6EAw8wLQ8P7fQ (brief, current look, options A to C); the chosen look becomes its own tasks once picked.
- [X] T039 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` draw each line as an outline pass 3 px wider in its colour at 53% brightness and a colour pass, both at 75% alpha; drop the polygon start dot (FR-007, research R1).
- [X] T040 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` draw each shown line's start point, the line in progress included, as a pooled `ImageWidget` at Z -445: the Dot entry of the placed-marker config (quad `dot`, else the first icon) loaded with `LoadImageFromSet`, tinted with the line's colour, sized from the entry's `GetIconSize()` (56 when 0), centred on the start, hidden when culled (FR-007, research R1).
- [X] T041 [US3] In `LL/scripts/game/Map/LL_MapLinesUI.c` place the Left Shift labels at the marker label offset (half the icon size times the entry's label ink plus `LL_LABEL_GAP`, made readable in `LL/scripts/game/Modded/LL_M_SCR_MapMarkerWidgetComponent.c`) and hit-test the start point at the icon's size (research R9, R10).
- [X] T042 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` draw the band once at 55% alpha and its outline as a closed 2 px edge loop at 80% (operator, second test: nothing showed through the band) (FR-007, research R1).
- [X] T043 [US3] In `LL/scripts/game/Map/LL_MapLinesUI.c` skip null widgets under the cursor: deleting a marker crashed `IsOwnMarkerUnderCursor` (operator, second test).
- [X] T044 [US2] In `LL/scripts/game/Map/LL_MapLinesUI.c` set the line in progress' width from the zoom at each click that adds a point, draw it with that width between clicks and send it on Enter (FR-006, spec session 2026-11-27, second test).
- [X] T045 Enlarge the insert-marker dialog icons in this pull request (operator's call): `LL/UI/Map/InsertMarkerDialog.layout` preview 38 → 56 px, its frame 66 → 84, dialog 338 → 356; `LL/scripts/game/UI/Map/LL_InsertMarkerDialog.c` `DIALOG_HEIGHT` 356, `LINE_MODE_HIDDEN_HEIGHT` 188, list icons 32 px through the stock element's `IconSize` overrides; changelog line.
- [X] T046 [US1] Refuse new lines once the freeze time ends: `LL_MapLine.IsDrawingPhase` (`LL/scripts/game/Map/LL_MapLine.c`), refusal 7 in `LL_MapLineStore.Add_S`, Ctrl + click refused and a line in progress discarded with a message in `LL/scripts/game/Map/LL_MapLinesUI.c`; deletion unchanged (FR-001).
- [X] T047 [US1] Backspace removes the last point, or cancels with only the start left: action `LL_MapLineUndo` in `LL/Configs/System/chimeraInputCommon.conf` and `LL_MapLineContext`, its `keyBindingMenu.conf` row, handler in `LL/scripts/game/Map/LL_MapLinesUI.c` (FR-004).
- [X] T048 [P] Strings `LL-Keybind_MapLineUndo` and `LL-MapLines_RefusedFreezeOver`, the Communications paragraph and the 1.2.0 changelog in English and Russian; runtime tables regenerated.
- [X] T049 [US3] Delete acts only on a line's start point (`FindDeletableLineUnderCursor` in `LL/scripts/game/Map/LL_MapLinesUI.c`; the segment test removed); guide paragraph and changelog wording (FR-012, research R10).
- [X] T050 [US1] The colour dialog raises a colour-changed event (`LL/scripts/game/UI/Map/LL_InsertMarkerDialog.c`); the line in progress shows each pick at once and Cancel restores the previous colour (`LL/scripts/game/Map/LL_MapLinesUI.c`) (FR-005).
- [X] T051 [US3] Lines: any faction player who may place markers deletes any faction line (`Remove_S` in `LL/scripts/game/Map/LL_MapLineStore.c`, hit test in `LL/scripts/game/Map/LL_MapLinesUI.c`); a placed marker under the cursor goes first (FR-012).
- [X] T052 [US3] Markers: `m_bIsDeleteRestricted = false` and the faction and type gate with server-marker removal in `LL/scripts/game/Modded/LL_M_SCR_MapMarkerSyncComponent.c`; `RemoveOwnedMarker` override for server-owned markers in `LL/scripts/game/Map/LL_MapMarkersUI.c` (FR-012, research R15).
- [X] T053 [US3] `[LL_MapAudit]` lines: line accepted and deleted (`LL_MapLineStore.c`), marker accepted (`LL/scripts/game/Modded/LL_M_SCR_MapMarkerManagerComponent.c`), marker deleted (`LL_M_SCR_MapMarkerSyncComponent.c`) (FR-020, research R16).
- [X] T054 [P] Communications paragraph (any line or marker of your side can be deleted) and the 1.2.0 changelog in English and Russian; runtime tables regenerated.

## Phase 9: Review fixes (PR #12 review, 2026-11-27)

- [X] T055 [US1] Refusals shown in a footer-style panel at the bottom centre of the open map (`LL/UI/Map/MapLineNotice.layout` + `.meta`, `ShowNotice` in `LL/scripts/game/Map/LL_MapLinesUI.c`; `ShowMapLineRefused` in `LL/scripts/game/Core/LL_LobbyPlayerComponent.c` routes there, chat only with the map closed) (FR-011, research R17).
- [X] T056 [US3] One Delete press removes one marker: `OnInputMarkerDelete` override in `LL/scripts/game/Map/LL_MapMarkersUI.c` (research R17).
- [X] T057 [US3] One Delete handler for markers and lines: the marker UI's override passes a press with no marker under the cursor to `LL_MapLinesUI.TryDeleteLineUnderCursor`; the line UI has no Delete listener of its own (fixes the listen-host double deletion without timing).
- [X] T058 [US4] Faction changes settled per sync tick against the faction held (`StartSync_S`, `ApplySync_S`, `ForgetPlayer_S` in `LL/scripts/game/Map/LL_MapLineStore.c`; forgotten on disconnect in `LL_LobbyManager.OnPlayerDisconnected`); restore forces a fresh sync.
- [X] T059 [US4] `Restore_S` renumbers a saved line whose id is taken and returns the restored count (`LL_MapLineStore.c`, `LL/scripts/game/Persistence/LL_MapLineData.c`).
- [X] T060 [US1] Server line interval 1 s (`SERVER_MIN_INTERVAL_S` in `LL/scripts/game/Map/LL_MapLine.c`), client 2 s.
- [X] T061 [US1] The cursor-state check covers every click of a line in progress (`OnInputMapSelect` in `LL_MapLinesUI.c`).
- [ ] T038 Operator: compile and run quickstart steps 1 to 12 on a dedicated server.

---

## Dependencies & Execution Order

- Phase 1 (T001 → T002) before everything.
- Phase 2: T003, T004, T007, T008 in parallel; T005 after T003; T006 after T005; T009 after T004.
- US1 (Phase 3) needs Phase 2. Inside: T010 → T011; T012 and T013 in parallel with them; T014 after
  T008 and T009; T015 and T017 after T014; T016 after T011, T013 and T014.
- US2 (Phase 4) after T017.
- US3 (Phase 5) after US1 and US2: T020 after T011; T021 after T019 and T020; T022 after T019.
- US4 (Phase 6) after T011: T023 → T024; T025 after T005; T026 after T023 and T025; T027 after T025.
  US4 does not depend on US2 or US3.
- Polish after the stories; T032 last.

## Parallel Example: Phase 2

```text
T003 LL_MapLine.c   |   T004 LL_MapLineCache.c   |   T007 shared rules   |   T008 input configs
```

## Implementation Strategy

MVP is Phases 1 to 3: lines can be drawn, coloured and shared, with every server check. US2 makes
them read as map features and cheap to draw; US4 makes them reliable for late joiners and through
a resume; US3 adds removal and authorship. Each phase ends on its quickstart checkpoint. Nothing is
committed or pushed without the operator's word.
