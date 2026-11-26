# Quickstart: Validating Map Line Drawing

Acceptance environment: the demo scenario (`Missions/LobbyDemo.conf`) on a dedicated server.
Clients: A and B slotted in one faction, C in the other, D connected without a slot. One of them
logged in as admin where a step says so.

## Prerequisites

1. Workbench compiles the addon with no script errors.
2. `LL_MapBriefing.conf`, `chimeraInputCommon.conf`, `keyBindingMenu.conf` and
   `LL_Persistence.conf` open in Workbench without errors (see plan, Workbench steps).
3. The server log shows no `[LL_Lobby]` line errors at start.

## Steps

1. **Draw and share (US1).** A places a marker and picks red in its dialog. Ctrl + click: a red
   Dot marker, identical to a placed one, and it keeps that size at every zoom. Click 20 points
   round an object, double click: no point is added and the dialog shows
   only Colour, Cancel and OK. Pick green, OK: the whole line turns green and drawing goes on.
   Two more clicks, Enter: the line is sent with no dialog. B sees it green within a second on
   the briefing map; C and D never receive it (client logs show no line deliveries). A's next
   marker dialog preselects green.
2. **Cancel paths (US1).** A starts a line and presses Escape: gone. A double clicks during a
   line and cancels the dialog: the line is unchanged and drawing goes on. A sends a line and
   presses Enter on the next one within 2 seconds: told to wait, the line stays. B never saw
   the cancelled lines.
3. **Keys stay ours (R3).** While A draws: Enter does not open the chat, Escape does not close
   the map, M does not close the map, push-to-talk still transmits to B, the mouse wheel zooms,
   a drag pans without adding a point, a double click opens no marker dialog.
4. **Width rule (US2).** A starts a line zoomed out, zooms in (the line scales with the map) and
   clicks the next point: the whole line takes the thinner width of the new zoom. A finishes one
   line fully zoomed out (50 m on the ground, thin on the screen at that zoom) and one zoomed in. Zoom in on the first: a band no wider than 50 m;
   zoom out on the second: a hairline. B sees the same ground widths at the same zoom as A.
   Lines lie under markers and the map shows through them.
5. **Who drew it (US3).** B holds Left Shift: "A's name (n minutes ago)" next to the start dot;
   released, it goes.
6. **Map cleanup (US3).** A points at the middle of a segment and presses Delete: nothing. B
   points at the start point of A's line and presses Delete: gone for A and B. B deletes A's
   marker the same way, and, after a session resume, a restored marker. With a marker on top of a
   line's start point, Delete removes only the marker. C (other faction) sends a forged removal
   for A's marker id: nothing changes. The server log has an `[LL_MapAudit]` line for every line
   and marker placed and deleted, naming who, whose and what.
7. **Slot changes (US4).** A draws three lines. D takes a slot in A's faction and opens the map:
   all three within 5 seconds. B switches to C's faction: A's lines leave B's map; C's lines
   appear. A disconnects and reconnects: A's lines are still there and A can delete them.
8. **Briefing to game (US4).** Lines drawn in the briefing are on the game map and on the
   spectator map (read-only: Ctrl + click there starts nothing).
9. **Refusals (edge cases).** A draws 20 lines; the 21st Ctrl + click shows the player-limit
   message. A clicks 64 points; the 65th shows the point-limit message. A confirms two lines
   within 2 seconds: the second is refused with the too-soon message. During hard freeze
   Ctrl + click starts nothing, and a dead A cannot draw or delete. A clicks three points and
   presses Backspace three times: two points go, the third press cancels the line. A draws a
   line while the freeze time runs out: it is discarded with the freeze message; after that
   Ctrl + click only shows the message, and A can still delete an own line.
10. **Session resume (FR-015).** With lines on both sides, kill the server process and resume the
    session: after retaking slots each faction has its lines, ages continue, authors can delete
    theirs.
11. **Scale (SC-005, SC-006).** Fill one faction to 150 lines (a test client drawing with
    several players). Pan and zoom: no stutter; the frame time on the map rises by no more than
    2 ms over an empty map. Run a 127-slot session with both factions at the limit through a
    session resume: no replication kick, no script error.
12. **Guide (FR-018).** The Communications entry of the briefing explains start, points, finish,
    cancel, delete and the info key, in English and Russian.
