# Feature Specification: Game Master Only Player Notices

**Feature Branch**: `006-gm-only-notifications`

**Created**: 2026-11-07

**Status**: Draft

**Input**: User description: "Join, leave, kick and ban notifications are shown only to Game Masters; other players no longer see them."

## Current State

The game posts a notice in the notification feed whenever a player joins ("Player X
joined the game"), leaves ("Player X left"), is kicked ("Player X was kicked. Reason:
connection timeout") or is banned, with the reason and the ban duration. Every player
sees all of them, at every stage of a Lite Lobby mission: slot selection, briefing,
the game and spectator. A wave of connection-timeout kicks fills every player's feed
with one notice per kicked player, and every one of those notices is a message from
the server to each connected player.

The game already keeps some notices for Game Masters only, for example "player joined
a faction", and marks them in the feed as Game Master notices. A Game Master is a
player the game has given Game Master rights, whether or not the editor is open at the
time. Three notices are about Game Masters themselves and go to every player: "Game
Master left", "player became Game Master" and "player is no longer Game Master".

Lite Lobby sends no join or leave notices of its own. The lobby roster shows
disconnected players as greyed rows; that is lobby information, not a notice, and is
outside this feature.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Players' feed stays quiet (Priority: P1)

A player in a Lite Lobby mission without Game Master rights no longer sees who joined,
left, was kicked or was banned, in the lobby, on the briefing, in the game or in
spectator.

**Why this priority**: it is the request. The notices are noise for players, and a
timeout wave buries the feed.

**Independent Test**: on a dedicated server with two clients, neither of them a Game
Master, let a third client join, leave and get kicked; watch both feeds.

**Acceptance Scenarios**:

1. **Given** a player without Game Master rights is in the lobby, **When** another
   player joins, **Then** no "joined" notice appears in their feed.
2. **Given** the same player is in the game, **When** another player leaves,
   **Then** no "left" notice appears.
3. **Given** the same player is in spectator, **When** an admin kicks another player
   or the server drops one for a connection timeout, **Then** no "kicked" notice
   appears.
4. **Given** the same player, **When** another player is banned, temporarily or for
   the session, **Then** no "banned" notice appears.
5. **Given** the same player, **When** a Game Master leaves, **Then** the "Game Master
   left" notice still appears.

---

### User Story 2 - The Game Master still sees everything (Priority: P1)

A Game Master keeps every join, leave, kick and ban notice, with the same name, reason
and duration as before, marked as a Game Master notice.

**Why this priority**: the Game Master runs the session and needs to know who came,
who went and why; hiding the notices from players must not hide them from the Game
Master.

**Independent Test**: repeat the User Story 1 test with one more client that holds
Game Master rights, editor closed; watch its feed.

**Acceptance Scenarios**:

1. **Given** a Game Master with the editor closed, **When** a player joins, leaves,
   is kicked or is banned, **Then** the Game Master's feed shows the matching notice
   with the player's name, and for a kick or ban the reason and duration.
2. **Given** a player who receives Game Master rights mid-session, **When** another
   player leaves afterwards, **Then** the new Game Master sees the notice.
3. **Given** a Game Master whose rights are taken away, **When** another player leaves
   afterwards, **Then** they no longer see the notice.

---

### Edge Cases

- A Game Master who is kicked: every player sees "Game Master left"; only the
  remaining Game Masters see the kick notice and its reason.
- No Game Master on the server: nobody sees join, leave, kick or ban notices.
- A player who reconnects after a timeout produces a "joined" notice for Game Masters
  only, like any other join.
- A Game Master who joins late sees no notices for events before they joined, as in
  the game.
- With the addon loaded under a game mode that is not a Lite Lobby game mode, every
  notice behaves as in the unmodified game.
- Every other notice (kill feed, group, vote, editor and Game Master status notices)
  is unchanged.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: In a Lite Lobby mission, the "player joined" notice MUST be shown only to
  players holding Game Master rights at the moment of the join.
- **FR-002**: In a Lite Lobby mission, the "player left", "player kicked" and "player
  banned" notices (temporary and session bans) MUST be delivered only to players
  holding Game Master rights at the moment of the event.
- **FR-003**: The notices a Game Master receives MUST carry the same player name,
  reason and ban duration as the unmodified game's notices.
- **FR-004**: The "Game Master left", "player became Game Master" and "player is no
  longer Game Master" notices MUST still reach every player.
- **FR-005**: FR-001 and FR-002 MUST apply at every stage: slot selection, briefing,
  game, debriefing, and to spectators.
- **FR-006**: Without a Lite Lobby game mode, all notices MUST behave as in the
  unmodified game.
- **FR-007**: The feature MUST add no replicated value, no remote procedure call and no
  change to the join-in-progress payload. Left, kick and ban notices MUST be sent only
  to the Game Masters, not sent to everyone and hidden afterwards.
- **FR-008**: The behaviour MUST be always on in Lite Lobby missions, with no setting.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a session with one Game Master and two other players, a join, a leave
  and a kick leave zero notices in the two players' feeds and three in the Game
  Master's, with the same text as before the feature.
- **SC-002**: When a Game Master is kicked, every player's feed shows "Game Master
  left".
- **SC-003**: The server sends one message per Game Master for each leave, kick or ban
  instead of one per connected player: with 127 players and 2 Game Masters, 2 instead
  of 127.
- **SC-004**: In a scenario without a Lite Lobby game mode, join, leave and kick notices
  reach every player as before.

## Assumptions

- "Game Master" means the game's own Game Master rights, as the game uses them for its
  other Game Master only notices. A logged-in admin without Game Master rights is not
  included.
- The Game Master status notices stay public because players need to know when the
  Game Master leaves or changes.
- No mission maker has asked to keep the old behaviour, so there is no setting
  (constitution IV); one can be added by a later specification if a community needs it.
- The lobby roster's greyed rows for disconnected players are unchanged.
