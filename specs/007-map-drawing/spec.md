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

### Session 2026-11-27 (after the first in-game test)

- Q: What happens to the width when the player zooms while drawing? → A: The line in progress
  keeps its width on the screen and adapts to the zoom; its ground width is fixed when it is
  finished.
- Q: How wide can a line get? → A: At most 50 m on the ground; a line finished further out is
  capped.
- Q: Is the colour picked for every line? → A: No. Enter sends the line in the colour last chosen
  in the insert-marker dialog, for a marker or a line. A double click while drawing opens the
  colour-only dialog to change the colour of the line in progress.
- Q: Does the line and its start point keep the interim look? → A: No. Their look is designed to
  match the lobby's markers in a Claude Design round; until then the interim look stays.
- Q: Which look did the design round settle on? → A: Option A: a semi-transparent band in the
  line's colour with a darker outline, as the marker glyphs have. The start point is the Dot
  marker itself, always identical to a placed Dot marker of that colour (same glyph, same size on
  the screen at every zoom, label in the same place), so the two never drift apart.

### Session 2026-11-27 (second in-game test)

- Q: How does the line in progress follow the zoom? → A: The whole line takes the width of the
  zoom at the player's latest click; between clicks it scales with the map like a finished
  line, and Enter sends the width of the latest click.
- Q: Do the larger insert-marker dialog icons ship with this feature? → A: Yes, in the same pull
  request: the dialog preview grows from 38 to 56 px and the list icons from 20 to 32 px.

### Session 2026-11-27 (freeze and undo)

- Q: When can lines be drawn? → A: Before the game and during its freeze time; once the freeze
  time ends no new line is accepted. Deleting lines stays allowed during the game.
- Q: What happens to a line in progress when the freeze time ends? → A: It is discarded and the
  player is told why.
- Q: How is a misplaced point taken back? → A: Backspace removes the last point; with only the
  start point left it cancels the line, as Escape does.

### Session 2026-11-27 (third in-game test)

- Q: Where does Delete act on a line? → A: Only on its start point, the Dot glyph, as a marker is
  deleted on its glyph; pointing at a segment deletes nothing.
- Q: When does a new colour show on the line in progress? → A: As soon as it is picked in the
  colour dialog, before OK; Cancel puts the previous colour back.

### Session 2026-11-27 (map cleanup)

- Q: Who may delete lines and markers? → A: Any player of the faction who may place markers may
  delete any of the faction's lines and placed markers, so a flood can be cleaned up by the
  players themselves. Always on in Lite Lobby missions.
- Q: Markers restored by a session resume belong to the server; can they be deleted? → A: Yes,
  by their faction, like any other marker.
- Q: Is the cleanup audited? → A: The server log gets one line for every line and marker placed
  and every one deleted: who did it, whose it was, the faction, and what it was.
- Q: Is this a separate specification? → A: No, it is part of this one: map cleanup covers lines
  and markers alike.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Draw a line and share it with the faction (Priority: P1)

A squad leader on the briefing map holds Ctrl and clicks: a Dot marker marks the start, in the
colour last chosen in the insert-marker dialog. Every further click adds a straight segment from the
previous point. Many close clicks make a curve, an arc or a circle. A double click opens the
colour list to change the colour of the line. Enter ends the line, and it appears on the map of
every player of the faction.

**Why this priority**: it is the feature. Phase lines, boundaries, routes and objective areas
are how a briefing is drawn.

**Independent Test**: demo world on a dedicated server, two clients slotted in one faction and
one in the other. The first client draws and confirms a line; the second sees it, the third
does not.

**Acceptance Scenarios**:

1. **Given** a player who may place markers, **When** they Ctrl + left click the map, **Then** the
   Dot marker glyph appears under the cursor, looking exactly like a placed Dot marker, in the
   colour they last confirmed (the first colour of the list until they confirm one).
2. **Given** a line in progress, **When** the player left clicks elsewhere, **Then** a straight
   segment joins the previous point to the click, and no marker dialog opens.
3. **Given** a line in progress, **When** the player double clicks, **Then** no point is added and
   the insert-marker dialog opens with only the colour list, Cancel and OK, preset to the line's
   colour.
4. **Given** that dialog is open, **When** the player picks another colour, **Then** the whole
   line, start point included, shows it at once; OK or Enter keeps it, remembers it for the
   next marker and line, and drawing continues. Cancel or Escape puts the previous colour back
   and drawing continues.
5. **Given** a line with at least two points, **When** the player presses Enter, **Then** the line
   is finished and sent in its colour, with no dialog.
6. **Given** a finished line, **Then** every player of the author's faction sees it within one
   second, on the briefing map, the game map and the spectator map, and no player of another
   faction or without a faction receives any part of it.
7. **Given** a line in progress, **When** the player presses Escape, **Then** the line is
   discarded and no other player ever received any of it.
8. **Given** a colour chosen in the insert-marker dialog while placing a marker, **When** the
   player starts a line, **Then** the line is drawn in that colour.

---

### User Story 2 - A line behaves like a map feature (Priority: P1)

A line finished zoomed out is a broad band on the ground, up to 50 m; one finished zoomed in is a
fine trace. While being drawn, a line keeps the same thickness on the screen whatever the zoom;
once finished it belongs to the map and scales with it like roads.

**Why this priority**: it is what makes the drawing readable. A width fixed on the screen would
make a phase line drawn for the whole map and a route drawn inside a village indistinguishable.

**Independent Test**: draw one line at the farthest zoom and one at the closest, then zoom
through the whole range and pan.

**Acceptance Scenarios**:

1. **Given** two lines finished at different zooms below the cap, **When** each is finished,
   **Then** both have the same width on the screen at that moment, and their widths on the ground
   differ by the zoom ratio.
2. **Given** a line in progress, **When** the player zooms in and clicks the next point, **Then**
   the whole line takes the width of the new zoom; zooming alone scales it with the map.
3. **Given** a line finished fully zoomed out, **Then** it is 50 m wide on the ground.
4. **Given** a finished line, **When** any player zooms or pans, **Then** the line stays on its
   ground positions and its width scales with the map: zooming in on a line drawn zoomed out
   shows a wide band, zooming out on a line drawn zoomed in shows a hairline. The start point
   keeps the size of a Dot marker on the screen.
5. **Given** a line, **When** any player looks at it at any zoom, **Then** its ground width is the
   same for every player: width belongs to the line, not to the viewer.
6. **Given** lines cross each other, roads or markers, **Then** the map and other lines show
   through them, and markers stay readable above them.

---

### User Story 3 - See who drew a line and clean up the map (Priority: P2)

Any player of a faction removes a line or marker of that faction that is no longer valid or
should not be there, whoever placed it, and can check who drew a line and when. If someone floods
the map, the players clean it up themselves, and the server log shows who placed and who removed
what.

**Why this priority**: a plan changes during a mission, lines and markers stay until removed, and
one player must not be able to bury a faction's map with nobody able to clear it.

**Independent Test**: two same-faction clients each draw a line and place a marker; each deletes
the other's; a client of the other faction tries to delete one by a forged request; the server
log is read afterwards.

**Acceptance Scenarios**:

1. **Given** a player who may place markers points at the start point of any line of their
   faction, theirs or not, **When** they press the marker delete key, **Then** the line
   disappears for every player of the faction. Pointing at a segment deletes nothing.
2. **Given** a player who may place markers points at any placed marker of their faction, theirs
   or not, a marker restored by a session resume included, **When** they press the marker delete
   key, **Then** the marker disappears for every player of the faction.
3. **Given** a player of the other faction sends a request to delete a line or marker they cannot
   see, **Then** the server refuses it and nothing changes.
4. **Given** the author disconnected and came back, **When** they delete their line, **Then** it
   is removed as before the disconnect.
5. **Given** lines on the map, **When** a player holds the marker-info key, **Then** next to each
   line's start dot the author's name and how long ago the line was drawn appear, worded as for
   markers, and they disappear when the key is released.
6. **Given** players placed and deleted lines and markers, **When** an admin reads the server log,
   **Then** each placement and each deletion is one line naming who did it, whose it was, the
   faction, and what it was.

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
- Enter while drawing finishes the line and does not open the chat; Enter in the colour dialog
  confirms the colour.
- Enter less than two seconds after the player's previous line tells the player to wait and
  keeps the line in progress.
- Dragging the map to pan while a line is in progress adds no point; zooming leaves the line's
  ground width as the latest click set it.
- A request sent to the server without the map screen (a modified client) with a colour not on
  the list, too many or too few points, points outside the map, a width outside 0.1 to 50 m,
  too soon after the previous line, or without the right to draw, is refused: nothing is stored
  and nothing is delivered.
- A line delivered twice to one player (the full set on slot take and a new line in the same
  moment) shows once; a deleted line never reappears.
- Start points overlap: one press of the delete key removes one line, the topmost one. A placed
  marker under the cursor is deleted first and the line is left alone. Markers overlap: one
  press removes one marker.
- A refusal is told on the map while it is open, also when the mission hides the chat.
- A player changes slot within their side: their lines are not cleared or sent again.
- A forged request deletes a line or marker of another faction, or a marker that is not a
  placed one: refused, nothing changes.
- Every machine stacks lines in the same order: the order the server accepted them.
- Lines end with the mission.
- The freeze time ends while a line is in progress: the line is discarded and the player is told;
  after that Ctrl + click starts nothing and says why. A line sent at that moment is refused by
  the server.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A player who may place markers MUST be able to start a line with Ctrl + left click
  on the briefing or game map. The rules are the markers' rules: a faction, a living slot
  character, not the spectator map, not during hard freeze. Lines MUST only be drawn before
  the game and during its freeze time: the server MUST refuse a line sent after the freeze time
  ends, and a line in progress at that moment MUST be discarded with a message. Deleting stays
  allowed.
- **FR-002**: While a line is in progress, each left click MUST add one point joined to the
  previous one by a straight segment. Holding or dragging the mouse MUST never add points: there
  is no freehand drawing.
- **FR-003**: While a line is in progress, marker placement by double click and marker editing by
  click MUST be suspended.
- **FR-004**: Enter MUST finish a line of two or more points and send it, with no dialog. Escape
  MUST discard a line in progress. Backspace MUST remove the last point, and with only the start
  point left cancel the line. A double click while a line is in progress MUST add no point
  and open the lobby's insert-marker dialog with only the colour list, Cancel and OK; drawing
  continues after it closes.
- **FR-005**: A line's colour MUST be one of the colours the insert-marker dialog offers. A line
  starts in the colour last confirmed in that dialog, for a marker or a line. A colour picked in
  the double-click dialog MUST show on the whole line in progress at once; OK MUST keep it and
  make it the remembered colour, Cancel MUST restore the previous one.
- **FR-006**: Each click of a line in progress MUST set the whole line's ground width from the
  zoom at that click, so that it is 2% of the screen at that moment, never more than 50 m;
  between clicks the line scales with the map. Enter MUST send the width of the latest click.
- **FR-007**: A line MUST be drawn as a semi-transparent band in its colour with a darker outline,
  above the map and beneath markers. Its start point MUST be drawn exactly as a placed Dot marker
  of the line's colour is drawn, from the same marker settings, so it has the Dot marker's glyph
  and size on the screen at every zoom, and its Left Shift label sits where a Dot marker's label
  sits.
- **FR-008**: A line MUST carry no text.
- **FR-009**: Nothing MUST be sent while a line is in progress. A confirmed line MUST be sent as
  one request, and the server MUST deliver it only to the players of the author's faction; a
  delivery to every player filtered on their machines is forbidden.
- **FR-010**: The server MUST determine the author's identity and faction itself, re-check the
  right to draw, and validate the point count, that every point is on the map, the colour and that
  the width is between 0.1 and 50 m. Any failed check MUST refuse the whole request.
- **FR-011**: The server MUST enforce 64 points per line, 20 lines per player and 150 lines per
  faction, and accept at most one new line from a player every second; the player's own map waits
  two seconds between lines, so delay on the network never refuses a line it allowed. A refused request
  MUST tell the player why, in their language.
- **FR-012**: Any player of a faction who may place markers MUST be able to delete any line of
  that faction by pointing at its start point and pressing the marker delete key, and any placed
  marker of that faction, a server-owned marker restored by a session resume included, the way
  markers are deleted today. The server MUST check that the requester may place markers and that
  the line or marker belongs to their faction (a faction-less marker counts as everyone's).
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
- **FR-020**: The server log MUST get one line for every line and marker placed and every one
  deleted, naming the player who did it, whose line or marker it was, the faction, and what it
  was (a line's id and point count, a marker's id, text and position).

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
- A line cannot be edited once sent; it is deleted and drawn again. A marker can still only be
  edited by its owner.
- Markers a Game Master placed from the editor, if the server owns them, are deleted like
  restored markers: the server cannot tell them apart.
- Markers and lines share the last confirmed colour.
- On the screen, a line in progress is about 2% of the screen height wide. The band is about 55%
  opaque; its outline is a 2 px edge in the line's colour at about half its brightness, as on the
  glyphs, drawn along the edges only so the map shows through the band. Joints between segments need not be smoothed. These values are
  tuned in game; the look itself is the design round's option A.
- If a mission replaces the marker icons and has no Dot glyph, the start point uses its first
  marker icon.
- The game's own pencil tool stays as it is: it draws for its user only and is not part of this
  feature.
- Drawing is a keyboard-and-mouse feature; the dialog keeps its controller navigation.
- Out of scope: arrow heads, filled areas, text along a line and a change history.
