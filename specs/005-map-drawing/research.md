# Research: Shared Map Drawing

## Minimal-change boundary

Decision: Preserve supplied module ownership, geometry, document, spatial index, revision and history algorithms. Permit mechanical names and only required integration/presentation changes.
Rationale: The operator requested a gentle migration with little source churn.
Alternatives considered: Rewriting the drawing system or moving all networking into the lobby player component would create a larger change with additional regression risk.

## Existing map integration

Decision: Merge into the existing modded classes and retain their lifetime guards. Gate the custom LL_MapMarkersUI map-select path and the LL_MapCursorModule wheel overrides directly.
Rationale: Custom subclass overrides bypass vanilla quick-marker and wheel handlers. Hooks in the vanilla base alone cannot cover these paths.
Alternatives considered: Replacing the lobby subclasses would discard existing marker editing, descriptions and screen lifecycle fixes.

## Permission and delivery boundaries

Decision: Use the owned SCR_MapMarkerSyncComponent's current LL_CanEditSharedMarkers gate before applying drawing edits; mirror current client marker restrictions. Retain server-side faction filtering and owner-only history delivery.
Rationale: Existing map-marker permission code already covers hard freeze, faction membership and destroyed slots. Drawing must not create another permission policy.
Alternatives considered: A UI-only gate would leave direct client edit requests unrestricted; a broadcast with client filtering would expose faction data.

## Shared Shift binding

Decision accepted by the operator: Marker-age display works when drawing is off; Shift is temporary eraser while drawing is enabled. Scope changes to modifier entry and the existing age-overlay display gate.
Rationale: Configs/System/chimeraInputCommon.conf binds LL_MapMarkerInfo to Left Shift. Activating erasing when a player only wants marker ages would change existing map usage.
Alternatives considered: Rebinding existing shortcuts or retaining simultaneous behaviors would surprise current users. The original brush/line/circle and color keys remain unchanged.

## Appearance

Decision: Reuse the existing map dialog's near-black panel, orange accent, Roboto Condensed typography and standard button states at presentation construction points.
Rationale: The user specifically requested the lobby's design, while preserving the supplied drawing interactions.
Alternatives considered: A new visual language or a generic theming system is unnecessary.

## Official engine evidence

Operator supplied the official script tree. Verified SCR_MapDrawingUI lifecycle and SetDrawMode; SCR_MapToolMenuUI.RegisterToolMenuEntry; SCR_MapCursorModule restrictions/wheel/drag handlers; SCR_MapMarkerSyncComponent owner identity and Server RPC pattern; WorkspaceWidget dynamic creation and DPI conversion; Widget.AddHandler/SetColorInt; CanvasWidget.SetDrawCommands and CanvasWidgetBase sizing APIs. Preserve dynamically created native widgets and existing canvas icon shapes; the existing lobby layouts supply presentation values. No new prefab or layout asset is necessary. The snapshot carries no release tag; compile and validate against the operator's target engine before runtime acceptance.

The operator explicitly accepted retaining vanilla marker transport as the limited exception described in plan.md. No additional component is introduced. Rejected edit requests must acknowledge the preview token. Merge duplicate/native marker safety before the existing trusted-delivery gate so it remains null-safe.
