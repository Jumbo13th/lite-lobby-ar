# Feature Specification: AI Spotting Markers Switch

**Feature Branch**: `003-ai-spotting-markers`

**Created**: 2026-09-13

**Status**: Draft

**Input**: User description: "After the 1.8 update, AI groups in a Lite Lobby mission automatically report the player squads they identify as timestamped military markers on the map. A trail of such markers builds up along a squad's route and players on both sides have seen it. Lite Lobby must be able to switch this game behaviour off for its missions; the operator chose an addon-side control over editing each mission's faction settings."

## Current State

Since game version 1.8 the game itself has AI groups report enemies. When an AI group
has identified an enemy character or vehicle and keeps it in sight, the game places a
military map marker of the reporting group's faction near the target, with the report
time and an "N minutes ago" age label, at a random offset that grows with the spotting
distance. A fresh report near a marker younger than half a minute is dropped; near an
older one it replaces it. Markers live for five and a half minutes. Nothing moves a
marker, so a squad on the march leaves a trail of them, and the trail refreshes as
long as any AI group keeps the squad in view. The game turns this on per faction, with
its stock army factions switched on and its irregular factions switched off.

Lite Lobby missions are built around hidden information: player-placed markers are
delivered only to the placing faction, mission-maker markers and trackers are
faction-gated, and the freeze-time and spectator rules exist so that nobody learns the
other side's position for free. The addon has no control over the game's AI reports:
nothing in the game-mode settings mentions them, and every faction cloned from a stock
army faction has them on. A mission that fields any AI group, whether as enemy forces,
as squad fill under a player leader, or as the reserve that the "remove unoccupied AI at
game start" option does not remove, reveals the players it sees to the reporting side.

The addon already exposes mission-wide policy from the game mode entity: chat, voice
popups, freeze protection, graphics minimums. A switch for AI reports belongs in that
same list.

## Clarifications

### Session 2026-09-13

- Q: Should a mission that never touches the setting have AI reports off or on? → A: off; existing missions lose the trail with no Workbench edit, cooperative missions opt in.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A squad's route stays secret (Priority: P1)

A mission maker builds a Lite Lobby mission with AI groups in it and leaves the AI
reporting switch at its default. During the game an AI group watches a player squad
cross open ground. No marker appears on anyone's map: not for the reporting side, not
for the squad's own side, not for a player who joins later.

**Why this priority**: it is the reported problem. The addon's whole information
discipline is undone by one trail of markers.

**Independent Test**: in the demo world, let one stock AI squad see a player of the
other faction for several minutes, then open the map on every client, including one
that joined mid-game.

**Acceptance Scenarios**:

1. **Given** the switch is at its default, an AI group is within sight of an enemy
   player and the game has been running for two minutes, **When** any player opens the
   map, **Then** no military marker with an age label is on it.
2. **Given** the same situation, **When** a new player joins and opens the map,
   **Then** no such marker is on it either.
3. **Given** the same situation, **When** the player being watched is in a vehicle,
   **Then** no vehicle marker appears.
4. **Given** the switch is at its default, **When** a player places a marker, a squad
   leader's marker shows, or a mission-maker marker or tracker is displayed, **Then**
   those behave exactly as before this feature.

---

### User Story 2 - A mission maker opts back into AI reports (Priority: P2)

A cooperative mission relies on friendly AI groups scouting for the players. The
mission maker turns the switch on. AI groups report identified enemies exactly the way
the unmodified game does, with the game's own timing, accuracy, lifetime and per-faction
visibility.

**Why this priority**: it keeps the choice with the mission maker instead of removing
a game feature for everyone.

**Independent Test**: in the demo world, turn the switch on, repeat the P1 setup and
watch markers appear on the reporting faction's map.

**Acceptance Scenarios**:

1. **Given** the switch is on and an AI group has identified an enemy player,
   **When** a player of the AI group's faction opens the map, **Then** a military
   marker with an age label is near the spotted player, as in the unmodified game.
2. **Given** the switch is on, **When** a player of the spotted faction opens the map,
   **Then** the marker is not shown to them, as the game itself intends.
3. **Given** the switch is on and a faction has the game's own per-faction report
   setting off, **When** its AI groups spot enemies, **Then** they still do not report;
   the addon switch never enables what the game forbids.

---

### Edge Cases

- A group that mixes a player leader with AI members is an AI group for reporting
  purposes; the switch governs it like any other.
- AI groups spawned after the game started, by the mission or by a Game Master, follow
  the switch the same way as groups placed in the world.
- The switch is read for the whole session; it is not changed mid-game and no chat
  command toggles it.
- With the switch off, no marker of this kind exists on the server, so there is
  nothing to purge on faction change, reconnect or join, and nothing reaches a
  spectator.
- Markers this feature suppresses are only the AI reports. Player-placed markers,
  squad-leader markers, mission-maker markers and trackers are out of its scope and
  keep their existing visibility rules.
- The game's own join-in-progress delivery of static markers is not gated by faction.
  With the switch on, a joining player of the spotted side may receive AI reports the
  live delivery would have withheld. That is the game's behaviour with or without this
  feature and is not changed here; it is noted so that a report of it is not mistaken
  for a defect of the switch.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The game mode MUST offer a mission-maker setting that turns the game's
  automatic AI enemy reports on the map on or off for the mission.
- **FR-002**: The setting MUST default to off, so a mission that never touches it
  shows no AI reports.
- **FR-003**: With the setting off, no AI group of any faction MUST place or refresh a
  report marker, and no such marker MUST reach any client at any stage of the mission,
  including players who join or reconnect mid-game.
- **FR-004**: With the setting on, AI reports MUST behave exactly as in the unmodified
  game: the same trigger, timing, accuracy, lifetime, symbol and per-faction visibility.
- **FR-005**: The setting MUST NOT enable reports for a faction whose own game
  settings forbid them.
- **FR-006**: The setting MUST affect only AI reports. Player-placed markers,
  squad-leader markers, mission-maker markers and trackers MUST be unaffected.
- **FR-007**: The decision MUST be taken on the server. The feature MUST add no
  replicated value, no remote procedure call and no change to the join-in-progress
  payload.
- **FR-008**: The setting's description in the mission editor MUST tell the mission
  maker what it suppresses and that the game's per-faction setting still applies.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With the setting off, a 10-minute session in which an AI group keeps
  continuous sight of enemy players ends with zero AI report markers on every client,
  including one that joined after the fifth minute.
- **SC-002**: With the setting on, the same session shows report markers on the
  reporting faction's map within the game's own report interval, and none on the
  spotted faction's map before any join.
- **SC-003**: Player-placed, squad-leader, mission-maker and tracker markers are shown
  in the same places and to the same players before and after the feature, in one
  side-by-side check on a dedicated server.
- **SC-004**: Network traffic per client during the game is not higher than before the
  feature, measured on a dedicated server; with the setting off it is lower or equal.

## Assumptions

- One mission-wide switch is enough. Per-faction control already exists in the game's
  faction settings and stays available to mission makers independently of this
  feature.
- The switch is a mission-maker setting on the game mode, next to the other policy
  settings, read for the whole session. Runtime toggling is not required.
- The feature only suppresses the game's reports; it does not change their timing,
  accuracy or lifetime when they are allowed.
- The demo world already contains one stock rifle squad per faction, which is enough
  to reproduce both stories; no new entities are needed there.
- The join-in-progress delivery of markers the game performs is outside this feature.
