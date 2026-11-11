# Feature Specification: Shared Map Drawing

**Feature Branch**: Not created

**Created**: 2026-10-31

**Status**: Accepted with operator constraints, 2026-10-31

**Input**: Integrate the existing map drawing functionality into the addon.

## Current State

The briefing map already includes the standard drawing component and tool menu, a custom marker dialog, and a custom cursor. Tool-menu and drawing-button lifetime guards handle switching between map screens. Marker placement, editing and removal already enforce faction membership, destroyed-slot restrictions and hard freeze. The spectator map is read-only. Marker delivery is filtered by faction on the server.

The requested drawing tools comprise freehand brush, straight line, circle, continuous anchor line, partial eraser, color selection, author inspection and faction history. They must become part of the addon and coexist with its marker dialog, description-panel scrolling and map-screen lifecycle.

## Clarifications

### Session 2026-10-31

- Operator accepted retaining the vanilla marker synchronization transport. Left Shift shows marker ages with drawing off; with drawing enabled, either Shift temporarily erases and marker ages are suppressed.

- Operator direction: Keep the transition gradual and minimize changes to the supplied drawing implementation; adapt its presentation to the existing lobby interface.
- The drawing document, geometry, erasing, spatial indexing, revisions and history grouping remain behaviorally unchanged. Changes are limited to required naming, integration, permission checks, localization and presentation.
## User Scenarios & Testing

### User Story 1 - Draw and revise a plan (Priority: P1)

A player opens the briefing or in-game map, selects a drawing tool and draws a plan without leaving the map.

**Why this priority**: Drawing is the requested capability and must work alongside the existing map controls.

**Independent Test**: In the demo mission, draw with each tool, partially erase every resulting shape, pan and zoom, then close and reopen the map.

**Acceptance Scenarios**:

1. **Given** drawing is permitted, **When** the player opens the drawing window, **Then** brush, line, circle, anchor line, eraser and color controls are available beside the existing tools.
2. **Given** each drawing tool, **When** the player completes its gesture, **Then** the shape remains attached to its map position through pan, zoom and map reopening.
3. **Given** existing shapes, **When** the player erases across them, **Then** only the intersected sections disappear and surviving sections retain their color and author.
4. **Given** drawing is selected, **When** the player clicks or double-clicks the map, **Then** the marker placement/edit dialog stays closed; after disabling drawing, its existing behavior returns.
5. **Given** repeated switches between briefing, game and spectator maps, **When** a map opens, **Then** tools appear once, no detached windows remain, and pan, zoom and description scrolling still work.

### User Story 2 - Share a faction plan safely (Priority: P1)

Players see the same faction drawing, including when joining late or reconnecting, while opponents do not receive it.

**Why this priority**: A shared map must protect faction information and preserve the lobby's permission rules.

**Independent Test**: Use a dedicated demo server with two factions and a late-joining client; exercise drawing, erasing, faction switching, death and hard freeze.

**Acceptance Scenarios**:

1. **Given** two players in one faction, **When** either draws or erases, **Then** both see the same completed result.
2. **Given** an opposing player, **When** a faction changes its drawing, **Then** the opposing player receives neither that drawing nor its history.
3. **Given** a faction drawing with erased sections, **When** a player joins or reconnects, **Then** the player receives its complete current state before editing becomes available.
4. **Given** faction switching, **When** the switch completes, **Then** the previous faction's drawing and pending history disappear and the new faction's current drawing loads.
5. **Given** a spectator, destroyed slot, player without a faction or active hard freeze, **When** drawing or erasing is attempted, **Then** editing is refused, including requests sent directly to the server.

### User Story 3 - Inspect changes (Priority: P2)

A player identifies the author of a drawing and opens a recent faction history to understand changes.

**Why this priority**: These are part of the requested drawing functionality and help players coordinate edits.

**Independent Test**: Have two faction members draw and erase one another's strokes, then inspect authors and reopen history after more changes.

**Acceptance Scenarios**:

1. **Given** a stored stroke, **When** the player hovers it, **Then** its author is shown.
2. **Given** successful drawing and erasing, **When** history opens, **Then** the player sees a complete newest-first snapshot of recent faction changes, including the affected author for erasing.
3. **Given** an open history, **When** further edits occur or records expire, **Then** that snapshot stays unchanged until history is reopened.
4. **Given** history or the mission-description panel under the cursor, **When** the wheel is used, **Then** the hovered list scrolls without zooming the map.

### Edge Cases

- Loss of editing permission ends the active gesture and hides editing windows; permission recovery preserves tool selection while that map stays open.
- Closing the map disables the selected tool and clears any pending anchor; stored drawing survives.
- A circle click without radius creates no shape; a single brush click creates a dot.
- Pan, zoom or tool changes finish valid current geometry without leaving a stuck drag.
- Text entry suppresses drawing shortcuts; releasing temporary modifier tools restores the previous tool and enabled state.
- Missing faction, drawing services or initial synchronization keeps editing disabled.
- The respawn screen never enables drawing. A spectator may view only information permitted by existing faction rules.
- Duplicate or out-of-order updates, and edits arriving during initial loading, must not duplicate strokes or restore erased sections.
- Reaching the point limit rejects new geometry without corrupting existing strokes; erasing frees capacity.
- A mission restart clears drawings and history; saved-session restoration is outside this feature unless separately approved.

## Requirements

### Functional Requirements

- **FR-001**: Drawing MUST ship as part of the addon without an additional addon dependency or mission-specific setup beyond the existing map components.
- **FR-002**: Players MUST have brush, straight line, circle, continuous anchor line and partial eraser tools. Brush, line and circle use click/drag gestures; anchor lines advance through successive clicks.
- **FR-003**: Players MUST select red, blue, green, yellow, black or white, or a custom RGB color. Color changes affect new geometry only.
- **FR-004**: Controls MUST preserve shortcuts 1/2/3 for brush/line/circle, C for the color picker, Shift for temporary erasing and left Ctrl for temporary anchor lines. Shortcuts MUST be inactive during text entry or when editing is unavailable.
- **FR-005**: Brush width MUST default to 4 UI pixels and eraser radius to 16 UI pixels. Anchor segments MUST retain their creation width in map space, with flat ends, joined corners and 50 percent opacity.
- **FR-006**: Drawing and history windows MUST follow horizontal or vertical tool-menu orientation and remain within screen bounds. Map interaction closes these windows without disabling the selected tool; selecting another standard map tool disables drawing.
- **FR-007**: Drawing MUST coexist with the custom marker placement/edit dialog, native map gadgets, mission-description scrolling and repeated map-screen changes.
- **FR-008**: Editing permissions MUST match existing shared-marker restrictions on both client and server, with the spectator map and respawn screen read-only.
- **FR-009**: The server MUST determine the requesting player's faction and author identity, validate edits and deliver drawing data and history only to eligible players of that faction.
- **FR-010**: Late join, reconnect and faction switching MUST load a complete current faction drawing before editing is enabled. Data MUST survive map closing, player death and disconnect within the current mission.
- **FR-011**: Each faction drawing MUST be limited to 8192 stored points. Brush and eraser gestures MUST batch ongoing edits about every 100 ms; line and circle previews remain local until completion, and anchor lines submit one segment per completed click.
- **FR-012**: History MUST contain only applied changes, expire records after five minutes, group repeated actions by one player at ten-second intervals, and record tool changes immediately. Erasing different authors MUST create separate records, grouped by affected author identity.
- **FR-013**: Opening history MUST request a complete snapshot and display it only after all parts arrive. Closing or faction switching MUST discard pending responses. Further changes MUST require reopening to refresh.
- **FR-014**: All visible tool labels, history actions, tooltips and readiness messages MUST ship in English and Russian through the existing localization system.
- **FR-015**: The feature MUST be reproducible in the demo mission and validated on a dedicated server. Verification MUST include a session with 127 occupied slots before scale readiness is claimed.

- **FR-016**: Integration MUST preserve the supplied drawing algorithms, data model, operation ordering, limits, timing and gestures except where an existing lobby input binding or permission rule requires a documented local adaptation. An unrelated cleanup or redesign is outside scope.
- **FR-017**: Drawing windows MUST use the existing map dialog's dark background, orange accent, typography, spacing and enabled/hovered/selected/disabled states. The order and meaning of drawing tools MUST remain unchanged; an active tool MUST be identifiable beyond color alone.
- **FR-018**: Existing map shortcuts MUST keep their behavior when drawing is off. Left Shift shows marker ages with drawing off; while drawing is enabled, either Shift temporarily erases and marker ages are suppressed. Releasing Shift restores the previous drawing tool.
### Key Entities

- **Faction drawing**: Current mission's geometry for one faction, with a finite point budget and ordered changes.
- **Stroke**: Authored colored geometry, tool identity and width; partial erasing may split it into surviving fragments.
- **Local tool state**: Selected tool, enabled state, color, temporary modifier and pending gesture; never shared with other players.
- **History record**: Acting player, action, affected author for erasing, and expiry time within one faction.
- **History snapshot**: Complete frozen view requested by one eligible player.

## Success Criteria

### Measurable Outcomes

- **SC-001**: All five geometry/editing tools pass their demo acceptance scenarios with no displacement after pan, zoom or reopening.
- **SC-002**: Same-faction clients, including a late joiner, show identical completed geometry after the same edit sequence; opposing clients receive zero private drawing or history records.
- **SC-003**: Every forbidden editing case in Story 2 is refused on the server and leaves faction geometry unchanged.
- **SC-004**: Twenty consecutive cycles through briefing, game and spectator maps produce no duplicate tool buttons, orphaned windows, stuck gestures or broken marker dialogs.
- **SC-005**: History grouping at ten seconds, expiry at five minutes and frozen-snapshot behavior pass the stated scenarios in both supported languages.
- **SC-006**: A dedicated session with 127 occupied slots passes shared editing, late join and faction isolation scenarios without a script exception or synchronization failure.

- **SC-007**: A source comparison after normalizing naming and file placement shows no algorithm changes in geometry, document storage, spatial indexing, circle construction, anchor geometry, revision handling or history grouping; every remaining difference has an integration requirement.
- **SC-008**: In English and Russian at 1080p and 1440p with default and increased UI scale, drawing controls match the existing map dialog style and have no clipped text or ambiguous selected state.
## Assumptions

- The operator will review this specification before planning and implementation, as required by repository policy.
- The requested behavior includes every player-facing drawing tool and history feature described above; standalone demonstration assets and development-only test launchers are not player features.
- Existing shared-marker permission rules govern drawing. This feature does not introduce new phase restrictions or grant spectators editing rights.
- Drawing lifetime remains the current mission; adding drawing to saved-session recovery requires a separate scope decision.
- The operator performs Workbench asset review, compilation, in-game acceptance and dedicated-server scale validation. Planning must identify sanctioned vanilla references and list every asset requiring review.


