# Implementation Plan: Map Line Drawing

**Branch**: `dizax` | **Date**: 2026-11-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/007-map-drawing/spec.md`

## Summary

Players draw polylines on the briefing and game maps: Ctrl + click starts a line in the colour
last chosen in the insert-marker dialog, clicks add straight segments, a double click opens
that dialog with only the colour list to recolour the line, Enter sends it. Lines are drawn on
one canvas per map screen as one polyline command each; a line in progress keeps its width on
the screen, and Enter fixes its ground width from the zoom, at most 50 m. The server validates every request, stores the
lines per faction, delivers each line only to that faction's players through
`LL_LobbyPlayerComponent` owner RPCs, re-delivers a faction's lines in paced batches when a
player's faction changes, and saves them for session resume. Nothing is sent while a line is
being drawn.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.9.0.37

**Primary Dependencies**: `SCR_MapEntity` (`GetCurrentZoom`, `WorldToScreen`,
`GetMapVisibleFrame`), `CanvasWidget` with `LineDrawCommand` and `PolygonDrawCommand`,
`SCR_MapUIBaseComponent`, `LL_InsertMarkerDialog`, `LL_MapMarkersUI`,
`LL_M_SCR_MapMarkerSyncComponent`, `LL_LobbyPlayerComponent`, `LL_LobbyManager`,
`SCR_PersistenceSystem` with `ScriptedStateSerializer` ([research.md](research.md))

**Storage**: server memory for the session; session resume through the persistence system

**Testing**: manual, demo world on a dedicated server ([quickstart.md](quickstart.md))

**Target Platform**: dedicated server (storage, validation, delivery) and every client (input,
drawing, cache)

**Project Type**: game addon feature

**Performance Goals**: no traffic while drawing; one request and one delivery per faction member
per line; a full faction (150 lines) reaches a player in under 4 s; map frame time +2 ms at most
with 150 lines (SC-005)

**Constraints**: faction-scoped data gated on the server; no `RplProp`, no `RplSave`; RPCs at 8
arguments or fewer; no prefab change

**Scale/Scope**: 127 players, 150 lines per faction, 64 points per line; 5 new script files,
7 modified script files, 4 configs, localization, changelog

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Canvas commands, the map projection, input contexts, array RPC arguments, world bounds and the persistence serializer are all used as in the official scripts or in working Lite Lobby code; each is cited in research R1–R12. | PASS |
| II. Ask, Never Assume | Line text, limits, resume and replays, admin powers, the full-faction rule and how keys are taught were the operator's answers (spec, Clarifications). The one engine unknown, the meaning of the input context `Flags`, is copied from a working context and checked in quickstart step 3. | PASS |
| III. KISS | One record type, one server store, one client cache, one map UI component, one persistent state. No new component on any prefab. | PASS |
| IV. YAGNI | No settings, no mission attributes, no undo, no hint UI, no replay recording. Limits are constants. | PASS |
| V. Replication | New replicated values: none. New RPCs: `RpcAsk_AddMapLine` (client → server, validated), `RpcAsk_RemoveMapLine` (client → server, validated), `RpcDo_OwnerAddMapLine`, `RpcDo_OwnerRemoveMapLine`, `RpcDo_OwnerResetMapLines`, `RpcDo_OwnerMapLineRefused` (server → one client). Visibility: faction-scoped, filtered on the server; the author identity never leaves the server; refusals are requester-only. Bulk: a faction's lines go as a paced, ordered batch after each faction change. All argument lists are 7 or fewer. Identity for ownership, limits and rate is the identity id. | PASS |
| VI. Comments and Hygiene | Comments only for the constraints: the context suppression, the per-frame projection, the pacing, the paired save/restore. New files `LL_`-prefixed; modded files keep vanilla parameter names. The branch's unused earlier files are deleted in the same change. | PASS |
| VII. Optional Integrations | Not touched. | PASS |
| Engine and Asset Constraints | No `.et` or `.acp` change; `ZoneCanvas.layout` is reused. One new layout, `UI/Map/MapLineNotice.layout`, with a hand-written `.meta` (GUID {9F3872243809E843}) and the edited `InsertMarkerDialog.layout`, both listed for the operator; `.conf` edits use fresh ids. | PASS |
| User Interface | The existing dialog, its components and its input handling; labels in the marker label style; reviewed in game at 1080p and 1440p (quickstart). | PASS |
| Localization | New keys `LL-MapLines_*`, `LL-CommsGuide_Lines*` and `LL-Keybind_MapLineStart` in `ll_localization.st`, English and Russian, runtime tables regenerated. | PASS |
| Documentation | Changelog entry in English and Russian under the next Workshop version; `ARCHITECTURE.md` replication contract and component list updated. | PASS |
| Testability | Reproducible in the demo world with four clients and an admin login; scale steps at 127 slots ([quickstart.md](quickstart.md)). | PASS |

## Project Structure

### Documentation (this feature)

```text
specs/007-map-drawing/
├── plan.md              # This file
├── research.md          # Phase 0
├── data-model.md        # Phase 1
├── contracts/
│   ├── network.md       # RPCs, checks, ordering, size
│   └── input.md         # actions, context, key behaviour
├── quickstart.md        # Phase 1: validation in the demo world
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2
```

### Source Code (repository root)

```text
Lite Lobby/scripts/game/Map/
├── LL_MapLine.c                 # new: line record, decimetre conversion, bounding box
├── LL_MapLineStore.c            # new: server store, checks, limits, delivery, paced sync
├── LL_MapLineCache.c            # new: client cache, ordered by id, change invoker
├── LL_MapLinesUI.c              # new: map UI component: input, line in progress, canvas, delete, labels
└── LL_MapMarkersUI.c            # ignore MapSelect while drawing or with Ctrl held; share the edit rule, last colour, age wording
Lite Lobby/scripts/game/Persistence/
└── LL_MapLineData.c             # new: persistent state and serializer
Lite Lobby/scripts/game/Core/
├── LL_LobbyPlayerComponent.c    # six RPCs
└── LL_LobbyManager.c            # owns the store on the authority; sync from SetPlayerEngineFaction_S
Lite Lobby/scripts/game/Modded/
├── LL_M_SCR_MapMarkerSyncComponent.c   # LL_CanEditSharedMarkers callable by the line store
└── LL_M_SCR_MapMarkerWidgetComponent.c # label style constants readable by the line labels
Lite Lobby/scripts/game/UI/
├── Map/LL_InsertMarkerDialog.c          # line mode: colour list only
└── Briefing/LL_MissionDescriptionUI.c   # Communications entry paragraph
Lite Lobby/Configs/
├── Map/LL_MapBriefing.conf                  # LL_MapLinesUI entry
├── System/chimeraInputCommon.conf           # three actions, LL_MapLineContext, LL_MapLineStart in LL_BriefingMapContext
├── System/keyBindingMenu.conf               # rows for the three line actions
└── Systems/Persistence/LL_Persistence.conf  # LL_MapLineData state and serializer
Lite Lobby/Language/
├── ll_localization.st           # new keys
└── ll_localization.*.conf       # regenerated by tools/generate-runtime-locales.ps1
docs/src/content/docs/
├── changelog.mdx
└── ru/changelog.mdx
../ARCHITECTURE.md               # workspace: replication contract rows, components list
```

**Deleted** (from the branch's first commit, unused by this design):
`Lite Lobby/scripts/game/Map/LL_MapPaint*.c` (24 files),
`Lite Lobby/scripts/WorkbenchGame/LL_MapPaintTests.c`,
`Lite Lobby/scripts/game/Modded/LL_M_SCR_CursorCustom.c`,
`Lite Lobby/scripts/game/Modded/LL_M_SCR_MapCursorModule.c`,
`Lite Lobby/scripts/game/Modded/LL_M_SCR_MapEntity.c`,
`Lite Lobby/scripts/game/Modded/LL_M_SCR_MapMarkersUI.c`,
`specs/005-map-drawing/`.

**Returned to their `main` content first** (modified by that commit):
`chimeraInputCommon.conf`, `ll_localization.st` and the 13 runtime tables,
`Map/LL_MapCursorModule.c`, `Map/LL_MapMarkersUI.c`, `Modded/LL_M_SCR_MapDrawingUI.c`,
`Modded/LL_M_SCR_MapMarkerManagerComponent.c`, `Modded/LL_M_SCR_MapMarkerSyncComponent.c`.

## Design

1. **Record** (`LL_MapLine`): fields per [data-model.md](data-model.md); converts world metres
   to decimetres and back; computes the ground bounding box.
2. **Server store** (`LL_MapLineStore`, created by `LL_LobbyManager` on the authority):
   - `Add_S(playerId, colour, width, points)`: the checks of
     [contracts/network.md](contracts/network.md) in order; on success stores the line with the
     next id and sends `RpcDo_OwnerAddMapLine` to every player whose engine faction is the line's,
     each with its own `own` flag; on failure `RpcDo_OwnerMapLineRefused`.
   - `Remove_S(playerId, lineId)`: checks, deletes, sends `RpcDo_OwnerRemoveMapLine` to the
     faction.
   - `StartSync_S(playerId, faction)`: replaces any pending job for the player; sends
     `RpcDo_OwnerResetMapLines`; with a faction, queues its line ids ascending. One 250 ms
     `CallLater` tick sends at most 10 per job and 100 in all, and stops when no job is left.
     Jobs of disconnected players are dropped.
   - Snapshot and restore for the serializer.
3. **Faction hook**: `LL_LobbyManager.SetPlayerEngineFaction_S` calls `StartSync_S` after
   `SetFaction_S`, for a faction and for none, next to the marker re-send.
4. **RPCs** on `LL_LobbyPlayerComponent`: the two asks forward to the store with the
   component's player id; the four owner RPCs feed the client cache or show the translated
   refusal with `ShowCommandHelp`.
5. **Client cache** (`LL_MapLineCache`): add (ignore known id), remove, reset; sorted by id;
   counts the faction's lines and the player's own for the client-side limit check; one change
   invoker for the UI.
6. **Map UI** (`LL_MapLinesUI`, added to `LL_MapBriefing.conf`, so it serves the briefing, game
   and spectator maps):
   - Map open: creates the canvas (`ZoneCanvas.layout`, Z order -450) in the map frame; map
     close: removes it, discards a line in progress, drops labels.
   - Start: on `MapSelect` with `LL_MapLineStart` held, if `LL_MapMarkersUI.LL_CanEditMarkers`
     allows and the cache says the limits allow (otherwise the reason is shown); takes the
     last confirmed colour.
   - In progress: `ActivateContext("LL_MapLineContext")` every frame; re-checks the edit rule
     every frame; clicks add points; the width follows the zoom; a double click opens the dialog
     in line mode; Enter sends `RpcAsk_AddMapLine` with the width of that moment (max 50 m) and
     discards the local line (the server's delivery draws it); Escape discards.
   - Dialog: created and driven as `LL_MapMarkersUI` does; OK recolours the line in progress and
     stores the colour as the last confirmed one; Cancel only closes it. Drawing continues.
   - Render: rebuilds the commands when the zoom, the pan or the cache changed, or a line is in
     progress; culls by bounding box; one origin projection per frame (research R2). Each line
     is an outline pass and a colour pass; its start point is a pooled image widget drawn as the
     Dot marker (research R1).
   - Delete: `MapMarkerDelete` listener per research R10, then `RpcAsk_RemoveMapLine`.
   - Labels: while `LL_MapMarkerInfo` is held, one text widget per visible start dot, styled
     with the marker label constants, repositioned every frame.
7. **Marker UI** (`LL_MapMarkersUI`): `OnInputMapSelect` returns early while a line is in
   progress or while `LL_MapLineStart` is held; `LL_CanEditMarkers` and the age wording become
   static and public; the last confirmed colour gets a static getter and setter.
8. **Dialog** (`LL_InsertMarkerDialog.InitLine`): hides `PreviewFrame`, `EditBoxRoot` and
   `TypeCombo`; focuses the colour list; the colour rows show the start glyph; confirm reports the colour.
9. **Persistence**: `LL_MapLineData` and `LL_MapLineSerializer` per research R12, plus their
   entries in `LL_Persistence.conf`.
10. **Guide and texts**: the Communications paragraph; refusal messages; the keybinding name.
11. **Changelog**: under the next Workshop version (the operator decides its number; 1.2.0
    follows the 1.1.0 precedent for an added feature).

## Workbench steps for the operator

1. Compile.
2. Open and save nothing, only check for errors: `Configs/Map/LL_MapBriefing.conf` (new
   `LL_MapLinesUI` entry), `Configs/System/chimeraInputCommon.conf` (actions `LL_MapLineStart`,
   `LL_MapLineFinish`, `LL_MapLineCancel`, `LL_MapLineUndo`; context `LL_MapLineContext`),
   `Configs/System/keyBindingMenu.conf`, `Configs/Systems/Persistence/LL_Persistence.conf`.
   Reload any of them if Workbench holds a stale buffer.
3. Open `UI/Map/MapLineNotice.layout` (new) and `UI/Map/InsertMarkerDialog.layout`; check that
   Workbench keeps the notice's GUID {9F3872243809E843}. If it assigns another, the
   `NOTICE_LAYOUT` constant in `LL_MapLinesUI.c` must be changed to it.
4. Run [quickstart.md](quickstart.md) on a dedicated server.

No prefab or asset import.

## Constitution Check (post-design)

Unchanged from the gate above. PASS.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| Paced sync queue on the server | A faction at its limit is about 75 KB per player; after a session resume every player retakes a slot at once | Sending everything at once, as markers do, multiplies that burst by every reconnecting player |
