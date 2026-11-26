# Input Contract: Map Line Drawing

## Actions (`Configs/System/chimeraInputCommon.conf`)

| Action | Default keys | Context | Read as |
|--------|--------------|---------|---------|
| `LL_MapLineStart` | Left Ctrl, Right Ctrl | `LL_BriefingMapContext` | held, polled at `MapSelect` |
| `LL_MapLineFinish` | Enter, numpad Enter | `LL_MapLineContext` | DOWN |
| `LL_MapLineCancel` | Escape | `LL_MapLineContext` | DOWN |

All three are listed in `keyBindingMenu.conf` under the lobby category, so the guide's "all keys can be rebound" stays true.

## Context `LL_MapLineContext`

Priority 60, Flags `0xc`. Activated every frame while a line is in progress, never otherwise.
Defines `LL_MapLineFinish` and `LL_MapLineCancel`; references `MapSelect`, `MapWheelUp`,
`MapWheelDown`, `MapZoomIn`, `MapZoomOut`, `MapPanDrag`, `MapPanH`, `MapPanV`, `MapCursorX`,
`MapCursorY`, `MapDrag`, `LL_MapMarkerInfo`, `VONDirect`. Deliberately absent: `ChatToggle`, `MenuBack`,
`MenuSelect`, `GadgetMap`, `MapMarkerDelete`, `MapContextualMenu`, `MapQuickMarkerMenu`.

## Behaviour

| State | Input | Result |
|-------|-------|--------|
| idle, may draw, under the limits | Ctrl + click | line starts with its dot; drawing context active |
| idle, may draw, at a limit | Ctrl + click | nothing starts; the reason is shown |
| drawing | click | point added (ignored past 64 points, with a message once) |
| drawing | drag | map pans, no point |
| drawing, 2+ points | Enter | drawing context ends; dialog opens with the colour list only |
| drawing, 1 point | Enter | nothing |
| drawing | Escape | line discarded |
| drawing | loses the right to draw, map closes | line discarded |
| dialog | OK or Enter (unless the colour list is open) | line sent in the chosen colour; colour remembered for markers and lines; refused locally, dialog kept, within 2 s of the previous line |
| dialog | Cancel or Escape | line discarded |
| idle | Delete over a line | topmost deletable line removed, unless a removable marker is under the cursor |
| any | hold Left Shift | author name and age at every visible start dot |

While a line is in progress, the marker UI ignores `MapSelect`, so no marker dialog opens.
