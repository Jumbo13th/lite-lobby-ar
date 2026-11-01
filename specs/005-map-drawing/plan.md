# Implementation Plan: Shared Map Drawing

**Branch**: `main` (unchanged) | **Date**: 2026-10-31 | **Spec**: [spec.md](spec.md)

**Status**: Accepted by the operator on 2026-10-31, including retained vanilla marker transport and the Shift rule.

## Summary

Port the supplied drawing implementation with its module boundaries and algorithms intact. Mechanically align names and file placement with this repository. Confine behavioral edits to the existing map lifecycle, lobby permissions, shared input bindings and localization. Restyle only presentation using the existing map dialog and briefing panel.

## Technical Context

**Language/Version**: Enforce Script; target Arma Reforger 1.8.0.10.
**Primary Dependencies**: Existing SCR_MapDrawingUI, SCR_MapEntity, SCR_MapCursorModule, SCR_MapToolMenuUI, SCR_MapMarkerManagerComponent and owned SCR_MapMarkerSyncComponent; existing lobby manager, slot data and localization.
**Storage**: Per-faction in-memory drawings and rolling history; local-only tool state. No saved-session extension.
**Testing**: Source comparison, existing drawing validation suites adapted only as needed, operator compilation and demo-world acceptance, dedicated-server verification.
**Target Platform**: Existing addon client and dedicated-server targets.
**Project Type**: Game addon, without another addon dependency.
**Performance Goals**: Preserve 8192-point budget, 32-point operation packets, approximately 100 ms continuous edit batching, revision ordering and five-minute history retention.
**Constraints**: No prefab editing, additional components, protocol redesign, geometry refactor or unrelated cleanup. No repository history changes.
**Scale/Scope**: Existing demo mission and 127 occupied slots for acceptance.

## Constitution Check

| Gate | Assessment |
| --- | --- |
| Official engine sources | Operator supplied the official script reference. Map lifecycle, tool registration, owned-controller marker RPCs, workspace and canvas APIs were checked. No new layout asset is needed: retain dynamic vanilla widgets and use existing lobby layouts as presentation references. The supplied snapshot has no version tag; compilation remains an operator check. |
| Accepted specification and plan | Specification and this plan accepted with the operator constraints on 2026-10-31. |
| Minimal implementation | Preserve existing module ownership and call order; direct changes at integration points rather than introducing an adapter framework. |
| Authority | Edits validated against the owned controller and existing server marker policy before applying geometry. No trusted faction/author identity supplied by the client. |
| Faction confidentiality | Drawings, deltas and snapshots: faction-scoped, server-filtered. History: requesting player's owner channel after server faction validation. |
| Local data | Gesture previews, selected tool, cursor, palette and tool windows: local-only. |
| Replication | Retain existing drawing packet fields, revision sequencing and snapshot assembly. Reuse the vanilla-owned marker synchronization component; no new replicated entity or component. See complexity justification below. |
| Repository conventions | Mechanical LL_ naming and modded file placement; English and Russian strings in the existing string table. No mass reformatting. |
| Assets | No .et edits. List every changed .conf/.meta/.layout for operator review; reuse current metadata for existing overrides. |
| UI | Follow existing map dialog plus sanctioned vanilla widget references once supplied; preserve icon semantics and gestures. |
| Verification | Source parity is checked separately from engine compilation, in-game visuals and dedicated-server acceptance. None of the latter can be claimed from a text review. |

## Project Structure

Feature documents live in `specs/005-map-drawing/`: specification, this plan, research, data model, UI contract and validation quickstart. The task list follows plan acceptance and the engine-source gate.

Production scripts stay under `Lite Lobby/scripts/game/`:

- `Map/LL_MapPaint*.c`: retain one corresponding module per supplied core/UI module.
- `Modded/LL_M_SCR_MapDrawingUI.c`: merge lifecycle hooks into the existing guards; no parallel competing SetDrawMode override.
- `Modded/LL_M_SCR_MapMarkerManagerComponent.c`: integrate drawing state, faction delivery and required replay safety without replacing the lobby's marker behavior.
- `Modded/LL_M_SCR_MapMarkerSyncComponent.c`: integrate drawing transport while retaining existing marker gates and disconnect behavior.
- `Modded/LL_M_SCR_MapEntity.c`, `LL_M_SCR_MapMarkersUI.c`, `LL_M_SCR_MapCursorModule.c`, `LL_M_SCR_CursorCustom.c`: only necessary supplied hooks, following existing naming conventions.
- Existing `Map/LL_MapMarkersUI.c` and `Map/LL_MapCursorModule.c`: preserve the custom dialog and zoom behavior while adding drawing guards at the actual subclass entry points.
- `Configs/System/chimeraInputCommon.conf`: merge drawing actions/context into the existing override, retain current metadata.
- `Language/ll_localization.st`: add drawing strings and regenerate runtime tables using `tools/generate-runtime-locales.ps1`.

No changes are planned to `addon.gproj` dependencies, demo prefabs, worlds, the resource database or unrelated lobby systems.

## Migration Boundaries

1. Preserve geometry, document storage, spatial index, brush sampling, line/circle construction, anchor geometry, rendering math, revision buffering and history grouping. Compare corresponding files after mechanical name normalization.
2. Rename CAM identifiers to LL identifiers mechanically. Preserve member roles and module boundaries. Rename server entry methods with the required _S suffix without changing packet shapes. Split modded classes into the prescribed existing files.
3. Merge lifecycle hooks around existing null/re-entrancy guards. Keep opening/closing order, avoid duplicate listeners/buttons and ensure all windows are removed on close.
4. Reuse current server edit permission checks on the owned synchronization component. Use the current client marker gate for normal map UI; keep spectator and respawn read-only. Preserve faction-scoped reads for eligible players.
5. Add paint-active guards to the custom marker dialog's map-select path, not only the vanilla quick-marker path. Reuse cursor restrictions for marker dragging and gadget interaction.
6. Add history-hover suppression directly to LL_MapCursorModule: its wheel overrides bypass the base implementation. Preserve its existing description-scroll suppression and zoom math.
7. Accepted shared-input rule: Left Shift retains marker-age display when drawing is off; while drawing is enabled, either Shift temporarily erases and marker ages are suppressed. Release restores the previous drawing tool. Apply this only in hotkey activation and the existing age-display gate. This is the operator-approved small integration difference.
8. Restyle the panel, color picker, history and author tooltip at their widget construction/settings points. Keep the existing tool list, hit testing, positioning, clipping and event flow. Do not add a generic theme service.
9. Add localized display strings at existing UI text boundaries; keep history grouping and server operation enums unchanged. Preserve player-provided names and drawing color values.

## Design Direction

Use [contracts/ui.md](contracts/ui.md) for colors, typography and interaction states. The references are `UI/Map/InsertMarkerDialog.layout` and `UI/Briefing/MissionDescriptionPanel.layout`. The dark panel and orange accent identify interface controls; chosen drawing colors remain the actual red/blue/green/yellow/black/white or custom color.

Keep the six-button toolbar compact and in its current order. Add the lobby-style outline and selected treatment without replacing canvas tool icons. Palette sliders and history remain familiar controls and respect existing scroll areas. UI construction may change only where standard component behavior is required by the repository and supported by the official references.

## Implementation Sequence

1. Resolve official source gate, review pinned script/data APIs and accept this plan.
2. Generate dependency-ordered tasks; analyze specification/plan/task coverage and constitution alignment once.
3. Import core modules with mechanical names and verify source parity.
4. Merge map and network hooks; verify single-player and faction synchronization before changing styling.
5. Apply appearance and localization changes only at presentation boundaries.
6. Run existing meaningful source validation where available; operator compiles and reviews all affected resources.
7. Execute [quickstart.md](quickstart.md), including the dedicated-server and scale checks. Report unfinished acceptance steps explicitly.

## Workbench Handover

The demo already configures SCR_MapDrawingUI, the tool menu and marker services; no new entity placement is planned. Verify this against the sanctioned sources before implementation.

The operator reviews `Configs/System/chimeraInputCommon.conf`, `Language/ll_localization.st`, generated language .conf files and any button/panel .layout/.meta introduced during implementation. Keep the exact final asset list in the handover. Reload stale resource buffers and fully restart Workbench after input-config changes, then compile and run LobbyDemo. No publication is authorized by this plan.

## Complexity Tracking

| Deviation | Why needed | Simpler alternative |
| --- | --- | --- |
| Retain drawing RPCs on existing vanilla marker synchronization instead of moving them to the lobby player component | The operator requests minimal source changes; this keeps the existing owner-routed protocol and adds no component or replicated entity. | Moving the whole protocol would increase changes and risk without improving the requested behavior. The operator explicitly accepted this limited exception to the single custom client/server channel principle on 2026-10-31. |

Post-design review: operator accepted transport exception and input rule; the official script reference is supplied. Implementation may proceed. Engine compilation, visual review and dedicated-server acceptance remain pending until performed.


