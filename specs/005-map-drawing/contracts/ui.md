# Map Drawing UI Contract

For implementation and operator review after plan acceptance.

## Visual References

Use `Lite Lobby/UI/Map/InsertMarkerDialog.layout` and `Lite Lobby/UI/Briefing/MissionDescriptionPanel.layout`.

| Element | Presentation |
| --- | --- |
| Window background | Existing near-black dialog background: RGBA 0.008, 0.012, 0.016, 0.95. History must retain readable contrast without exposing map content through text. |
| Accent / selected outline | Existing orange: RGBA 0.761, 0.392, 0.078, 1. The selected tool also has a visible outline or persistent pressed treatment. |
| Text | Existing Roboto Condensed Regular; 16 px body, 18 px titles where needed; light neutral body text. |
| Spacing | Keep 42 px tool controls, 6 px gap and 8 px toolbar padding unless runtime layout proves an adjustment necessary. Reuse the dialog's 12–20 px inner spacing for larger windows. |
| Default / hover | Dark neutral control background; existing neutral hover brightness. No new blue selection palette. |
| Disabled | Existing disabled control treatment; no hover/pressed feedback implying edit availability. |
| Drawing colors | Preserve actual stroke colors and color swatches; orange control accents never recolor geometry. |

## Interaction

- Tool order remains brush, straight line, circle, anchor line, eraser, color. Existing canvas icons retain their shapes.
- Preserve toolbar orientation, map-relative placement, viewport bounds, mouse hit areas, gesture semantics and popup-close behavior.
- Add localized tool hints and loading/error/empty history text. Player names remain unchanged.
- Preserve click/drag drawing; native marker creation/editing is blocked while drawing is active and restored when drawing is disabled.
- Accepted Shift rule: marker ages while drawing is off; temporary erasing while drawing is enabled. Do not alter the existing Shift binding globally.
- History and mission description scroll without zooming; the map still zooms outside those windows.
- English and Russian must fit at 1080p/1440p and at default/increased UI scale. No color-only selected-state signal.

Appearance work is restricted to the drawing toolbar, palette, history and author tooltip. Existing lobby layouts serve as references and are not restyled by this feature.

