# Feature Specification: Map Line Drawing

**Feature Branch**: `dizax`

**Created**: 2026-11-26

**Status**: Draft

**Input**: User description: "Players draw lines on the map. Ctrl + left click starts a line with
a dot, left clicks add straight segments, Enter finishes it and the marker dialog sets its
colour. A line's thickness is fixed by the zoom at the moment it is drawn and then scales with
the map. No freehand drawing. Performance and safety come first."

## Current State

Players share a plan on the map only with markers. A double left click on the briefing or
in-game map opens the lobby's insert-marker dialog: a description of up to 64 characters, a
marker type, a colour from the mission's list (blue, red, green and yellow unless the mission
sets its own), Cancel and OK. The dialog preselects the type and colour the player last
confirmed. A marker shows its author's name, and holding the marker-info key (Left Shift by
default) adds how long ago it was placed. A marker is shared with the placer's faction only, and
the server decides who receives it. Placing, moving and deleting need a faction and a living slot character, and are
refused on the spectator map and during hard freeze. Each player can have up to 100 markers.
Markers stay when their author disconnects, server admins can remove any of them, a session
resumed after a server crash restores them, and the replay recorder records them.

The game's map tool bar has a pencil: up to nine straight lines between two clicks, a fixed
width on the screen, kept on the drawing player's machine and shown to nobody else.

Nothing lets a player show another player a line. The feature still makes sense at 127 slots:
a line costs nothing while it is being drawn, then one request from its author and one delivery
to each player of the faction when it is confirmed.

## Clarifications

### Session 2026-11-26

- Q: Does a line carry a description from the dialog? → A: No. A line carries no text; for a line
  the dialog shows only the colour list, Cancel and OK.
- Q: What are the limits? → A: 64 points per line, 20 lines per player, 150 lines per faction.
- Q: Are lines restored after a server crash and recorded in replays? → A: Restored when the
  session is resumed; not recorded by the replay recorder.
- Q: What should a server admin be able to do with an unwanted line? → A: Every player who sees a
  line can hold the marker-info key to see, at its start dot, who drew it and how long ago, as
  with markers. Admins delete lines of the faction they are slotted in, as with markers.
- Q: What happens when a faction already has 150 lines? → A: The new line is refused and the
  player is told; authors or admins delete lines to make room. No line is ever removed
  automatically.
- Q: How do players learn the drawing keys? → A: A short paragraph in the generated
  Communications briefing entry, next to the voice and radio keys. No on-screen hint.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Draw a line and share it with the faction (Priority: P1)

A squad leader on the briefing map holds Ctrl and clicks: a dot marks the start. Every further
click adds a straight segment from the previous point. Many close clicks make a curve, an arc
or a circle. Enter ends the line and the insert-marker dialog opens; the leader picks a colour
and confirms. The line takes that colour and appears on the map of every player of the faction.

**Why this priority**: it is the feature. Phase lines, boundaries, routes and objective areas
are how a briefing is drawn.

**Independent Test**: demo world on a dedicated server, two clients slotted in one faction and
one in the other. The first client draws and confirms a line; the second sees it, the third
does not.

**Acceptance Scenarios**:

1. **Given** a player who may place markers, **When** they Ctrl + left click the map, **Then** a
   filled dot slightly wider than a line appears under the cursor, in the colour they last
   confirmed (the first colour of the list until they confirm one).
2. **Given** a line in progress, **When** the player left clicks elsewhere, **Then** a straight
   segment joins the previous point to the click, and no marker dialog opens, even for two
   quick clicks close together.
3. **Given** a line with at least two points, **When** the player presses Enter, **Then** point
   adding ends and the insert-marker dialog opens with only the colour list, Cancel and OK,
   preset to the last confirmed colour.
4. **Given** the dialog is open, **When** the player picks another colour and confirms with OK or
   Enter, **Then** the whole line, start dot included, takes that colour. Until then the line
   keeps the colour it was drawn in.
5. **Given** a confirmed line, **Then** every player of the author's faction sees it within one
   second, on the briefing map, the game map and the spectator map, and no player of another
   faction or without a faction receives any part of it.
6. **Given** the dialog is open, **When** the player cancels with Cancel or Escape, **Then** the
   line is discarded and no other player ever received any of it.
7. **Given** a line in progress, **When** the player presses Escape, **Then** the line is
   discarded.

---

### User Story 2 - A line behaves like a map feature (Priority: P1)

A line drawn zoomed out is a broad band on the ground; one drawn zoomed in is a fine trace. Both
look equally thick on the screen at the moment they are drawn, and after that they belong to
the map and scale with it like roads.

**Why this priority**: it is what makes the drawing readable. A width fixed on the screen would
make a phase line drawn for the whole map and a route drawn inside a village indistinguishable.

**Independent Test**: draw one line at the farthest zoom and one at the closest, then zoom
through the whole range and pan.

**Acceptance Scenarios**:

1. **Given** two lines drawn at different zooms, **When** each is drawn, **Then** both have the
   same width on the screen at that moment, and their widths on the ground differ by the zoom
   ratio.
2. **Given** a finished line, **When** any player zooms or pans, **Then** the line stays on its
   ground positions and its width scales with the map: zooming in on a line drawn zoomed out
   shows a wide band, zooming out on a line drawn zoomed in shows a hairline. The start dot
   follows the same rule.
3. **Given** a line, **When** any player looks at it at any zoom, **Then** its ground width is the
   same for every player: width belongs to the line, not to the viewer.
4. **Given** lines cross each other, roads or markers, **Then** the map and other lines show
   through them, and markers stay readable above them.

---

### User Story 3 - See who drew a line and remove it (Priority: P2)

A player removes a line they drew that is no longer valid. Any player can check who drew a line
and when. An admin removes a line that should not be there.

**Why this priority**: a plan changes during a mission, and lines stay until removed.

**Independent Test**: two same-faction clients each draw a line; each tries to delete both; then
an admin deletes one.

**Acceptance Scenarios**:

1. **Given** the author points at their own line, at any segment or at the start dot, **When**
   they press the marker delete key, **Then** the line disappears for every player of the
   faction.
2. **Given** a player points at a line someone else drew, **When** they press the marker delete
   key, **Then** nothing happens.
3. **Given** a server admin points at any line they can see, **When** they press the marker
   delete key, **Then** the line disappears for every player of the faction.
4. **Given** the author disconnected and came back, **When** they delete their line, **Then** it
   is removed as before the disconnect.
5. **Given** lines on the map, **When** a player holds the marker-info key, **Then** next to each
   line's start dot the author's name and how long ago the line was drawn appear, worded as for
   markers, and they disappear when the key is released.

---

### User Story 4 - Joining, reconnecting and changing sides (Priority: P1)

A player who takes a slot late, reconnects or moves to the other side sees exactly the lines of
the faction they are in now.

**Why this priority**: a briefing is drawn before most players are ready, and players reconnect
constantly; a plan that only some players see is worse than none.

**Independent Test**: one faction draws several lines; a new client joins and takes a slot in
it; a second client switches from that faction to the other one.

**Acceptance Scenarios**:

1. **Given** a faction has lines, **When** a player takes a slot in it (first join, reconnect or
   side change), **Then** that player's map shows all of the faction's current lines.
2. **Given** a player moves to the other faction, **Then** the previous faction's lines leave
   their map and that faction sends them nothing more.
3. **Given** an author disconnects, **Then** their lines stay on the faction's maps.
4. **Given** a line was drawn during the briefing, **When** the game starts, **Then** the line is
   still there.
5. **Given** the server crashed and the session is resumed, **When** players take their slots
   again, **Then** each faction's lines are back and their authors can still delete them.

### Edge Cases

- The player loses the right to draw while a line is in progress (death, hard freeze begins,
  slot left, map closed, spectator map): the line in progress is discarded; nothing was sent.
- Enter with only the start dot does nothing; Escape discards the dot.
- The line reaches its point limit: further clicks add nothing and the player is told; Enter
  still finishes the line.
- The player or the faction is at its line limit: Ctrl + left click tells the player and starts
  nothing; the server refuses such a request anyway. No line is ever removed to make room.
- Enter while drawing finishes the line and does not open the chat; Enter in the dialog
  confirms it.
- Dragging the map to pan while a line is in progress adds no point; zooming keeps the line's
  width as set when it started.
- A request sent to the server without the map screen (a modified client) with a colour not on
  the list, too many or too few points, points outside the map, a width outside the zoom range,
  too soon after the previous line, or without the right to draw, is refused: nothing is stored
  and nothing is delivered.
- A line delivered twice to one player (the full set on slot take and a new line in the same
  moment) shows once; a deleted line never reappears.
- Lines overlap: one press of the delete key removes one line, the topmost one the player may
  delete.
- Every machine stacks lines in the same order: the order the server accepted them.
- Lines end with the mission.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A player who may place markers MUST be able to start a line with Ctrl + left click
  on the briefing or game map. The rules are the markers' rules: a faction, a living slot
  character, not the spectator map, not during hard freeze.
- **FR-002**: While a line is in progress, each left click MUST add one point joined to the
  previous one by a straight segment. Holding or dragging the mouse MUST never add points: there
  is no freehand drawing.
- **FR-003**: While a line is in progress, marker placement by double click and marker editing by
  click MUST be suspended.
- **FR-004**: Enter MUST finish a line of two or more points and open the lobby's insert-marker
  dialog with only the colour list, Cancel and OK. Escape MUST discard a line in progress.
- **FR-005**: A line's colour MUST be one of the colours the insert-marker dialog offers. A line
  in progress MUST be shown in the last confirmed colour; the colour chosen in the dialog MUST
  apply to the whole line only on confirmation.
- **FR-006**: A line's ground width MUST be set when the line starts, from the current zoom, so
  that a fresh line has the same width on the screen at any zoom. After that the width MUST
  scale with the map. The start dot MUST be slightly wider than the line and follow the same
  rule.
- **FR-007**: Lines MUST be semi-transparent and drawn above the map and beneath markers.
- **FR-008**: A line MUST carry no text.
- **FR-009**: Nothing MUST be sent while a line is in progress. A confirmed line MUST be sent as
  one request, and the server MUST deliver it only to the players of the author's faction; a
  delivery to every player filtered on their machines is forbidden.
- **FR-010**: The server MUST determine the author's identity and faction itself, re-check the
  right to draw, and validate the point count, that every point is on the map, the colour and the
  width against the map's zoom range. Any failed check MUST refuse the whole request.
- **FR-011**: The server MUST enforce 64 points per line, 20 lines per player and 150 lines per
  faction, and accept at most one new line from a player every two seconds. A refused request
  MUST tell the player why, in their language.
- **FR-012**: The author and server admins MUST be able to delete a line by pointing at it and
  pressing the marker delete key; nobody else can. The server MUST check the right to delete. The
  author's right MUST survive a reconnect.
- **FR-013**: A player who takes a slot in a faction MUST receive that faction's current lines; a
  player who leaves a faction MUST lose them and receive nothing more from it. Lines MUST stack in
  the order the server accepted them on every machine.
- **FR-014**: Lines MUST last until deleted or until the mission ends, including when their author
  disconnects, dies or leaves the slot.
- **FR-015**: A session resumed after a server crash MUST restore every faction's lines with
  their authors, ages, colours, widths and order. The replay recorder does not record lines.
- **FR-016**: Every new player-facing text MUST ship in English and Russian.
- **FR-017**: Holding the marker-info key MUST show, next to the start dot of every line the
  player sees, the author's name and how long ago the line was drawn, worded as for markers. The
  name MUST still show after the author disconnects and after a session resume.
- **FR-018**: The generated Communications briefing entry MUST explain, next to the voice and
  radio keys, how to start, extend, finish, cancel and delete a line and how to see who drew it.
  The map shows no drawing hint of its own.
- **FR-019**: The feature MUST be reproducible in the demo world, verified on a dedicated server
  and measured at 127 occupied slots before it is called done.

### Key Entities

- **Map line**: one confirmed line of one faction. Author identity and name, faction, colour
  from the dialog's list, ground width, the ordered points (two to 64), the time and the order in
  which the server accepted it.
- **Line in progress**: the points, colour and width a player is drawing. Lives on that player's
  machine only and is never sent.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In the demo world, a confirmed line appears for every client of the author's faction
  within one second, and zero line data reaches a client of the other faction or without a
  faction.
- **SC-002**: Drawing a line of any length produces no network traffic until it is confirmed.
  Confirming or deleting a line produces one request from the player and one delivery to each
  player of the faction.
- **SC-003**: A player who takes a slot in a faction holding the maximum number of lines sees all
  of them within five seconds.
- **SC-004**: Every refusal listed under Edge Cases leaves the stored lines and every client's map
  unchanged, including requests that bypass the map screen.
- **SC-005**: With a faction at its line limit, panning and zooming the map shows no visible
  stutter, and the frame time on the map screen rises by no more than 2 ms against an empty map.
- **SC-006**: A dedicated-server session with 127 occupied slots and both factions at their line
  limits finishes with no replication kick and no script error caused by lines.

## Assumptions

- The right to draw and delete is the right to place and delete markers; this feature adds no
  new stage restrictions and gives spectators nothing to edit. Players without a faction, Game
  Masters without a slot included, neither draw nor see lines.
- A line cannot be edited; it is deleted and drawn again. There is no undo of the last point.
- Cancel in the dialog discards the line, as it discards a marker.
- Markers and lines share the last confirmed colour.
- On the screen, a fresh line is about 2% of the screen height wide, the start dot about one and
  a half times the line width, at about 60% opacity. Joints between segments need not be
  smoothed. These values are tuned in game during review.
- The game's own pencil tool stays as it is: it draws for its user only and is not part of this
  feature.
- Drawing is a keyboard-and-mouse feature; the dialog keeps its controller navigation.
- Out of scope: arrow heads, filled areas, text along a line and a change history.
