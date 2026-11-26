# Input Contract: Map Line Drawing

## Actions (`Configs/System/chimeraInputCommon.conf`)

| Action | Default keys | Context | Read as |
|--------|--------------|---------|---------|
| `LL_MapLineStart` | Left Ctrl, Right Ctrl | `LL_BriefingMapContext` | held, polled at `MapSelect` |
| `LL_MapLineFinish` | Enter, numpad Enter | `LL_MapLineContext` | DOWN |
| `LL_MapLineCancel` | Escape | `LL_MapLineContext` | DOWN |
| `LL_MapLineUndo` | Backspace | `LL_MapLineContext` | DOWN |

All four are listed in `keyBindingMenu.conf` under the lobby category, so the guide's "all keys can be rebound" stays true.

## Context `LL_MapLineContext`

Priority 60, Flags `0xc`. Activated every frame while a line is in progress, never otherwise.
Defines `LL_MapLineFinish`, `LL_MapLineCancel` and `LL_MapLineUndo`; references `MapSelect`, `MapWheelUp`,
`MapWheelDown`, `MapZoomIn`, `MapZoomOut`, `MapPanDrag`, `MapPanH`, `MapPanV`, `MapCursorX`,
`MapCursorY`, `MapDrag`, `LL_MapMarkerInfo`, `VONDirect`. Deliberately absent: `ChatToggle`, `MenuBack`,
`MenuSelect`, `GadgetMap`, `MapMarkerDelete`, `MapContextualMenu`, `MapQuickMarkerMenu`.

## Behaviour

| State | Input | Result |
|-------|-------|--------|
| idle, may draw, under the limits | Ctrl + click | line starts with its dot; drawing context active |
| idle, may draw, at a limit | Ctrl + click | nothing starts; the reason is shown |
| drawing | click | point added (ignored past 64 points, with a message once) |
| drawing | double click | the first click's point is taken back; dialog opens with the colour list only |
| drawing | drag | map pans, no point |
| drawing | zoom | the line scales with the map; the next click sets the width of this zoom |
| drawing, 2+ points | Enter | line sent in its colour with the width of the latest click (max 50 m); within 2 s of the previous line: message, line kept |
| drawing, 1 point | Enter | nothing |
| drawing | Escape | line discarded |
| drawing, 2+ points | Backspace | last point removed |
| drawing, 1 point | Backspace | line discarded |
| drawing | freeze time ends | line discarded, the player is told |
| idle, after the freeze time | Ctrl + click | nothing starts; the player is told |
| drawing | loses the right to draw, map closes | line discarded |
| dialog | OK or Enter (unless the colour list is open) | line recoloured; colour remembered for markers and lines; drawing continues |
| dialog | pick a colour | the line shows it at once |
| dialog | Cancel or Escape | previous colour restored; dialog closes; drawing continues |
| idle | Delete over a line's start point | topmost deletable line removed, unless a removable marker is under the cursor; over a segment nothing |
| any | hold Left Shift | author name and age at every visible start dot |

While a line is in progress, the marker UI ignores `MapSelect`, so no marker dialog opens.
