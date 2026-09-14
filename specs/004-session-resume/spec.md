# Feature Specification: Session Resume After a Server Crash

**Feature Branch**: `004-session-resume`

**Created**: 2026-09-14

**Status**: Draft

**Input**: User description: "Design a persistence system based on the Conflict persistence system. Focus on the state of the world (broken things, destroyed buildings), positions of characters, moving helicopters, vehicles, etc. Find the other edge cases too."

## Current State

A Lite Lobby event runs once. When the dedicated server dies mid-game, as it did on
2026-09-13 with 128 players forty-five minutes into the fight, the mission is gone:
the server comes back at slot selection with an empty map, and a hundred people either
replay the opening or go home. Nothing the addon holds survives a restart. Slot
assignments, KIA status, the freeze clock, fired triggers, unit statistics, placed
markers and the reservation list all live in memory only.

The game itself has had a session-save system since 1.7.0.54. Conflict uses it: a
mission opts in through its header, a save point captures the whole world, and the game
can continue from that point. The game writes the state of every tracked entity itself:
where it is, what it is, its damage per hit zone, everything in its inventory and
pockets, who sits in which seat, fuel per tank, lights, magazines and attachments,
armed mines, open and locked doors, and the whole time and weather picture. Ruined
buildings are recorded as "gone, ruin in its place" and come back ruined. Item entities
dropped on the ground come back where they fell. AI groups, their waypoints and radio
nets come back. Entities placed by a Game Master come back. Conflict adds a few small
serializers of its own for the state only it knows, and skips its fresh-start logic when
a save was loaded.

The same system leaves gaps that matter for a Lite Lobby mission. No entity keeps its
motion: a vehicle saved at speed comes back standing still, and a helicopter saved in
the air comes back in the air with no forward speed and no lift. Nothing that is towed,
slung or roped is reconnected. Fences, walls, trees, tyres, glass and other small
destructible objects have no record at all and come back intact. Vehicle wrecks, fires,
craters and debris are not saved. A dead body comes back dead but not in the pose it
fell in. Player-placed map markers have no record. The game has no periodic autosave;
Conflict saves when a base changes hands. The game's own reconnect bookkeeping deletes
a saved player character whose owner does not return within a minute after the load,
which contradicts the addon's rule that a slot is held for the whole game. When a game
mode ends on a dedicated server, the game deletes the session's saves. And nothing in
the game scripts shows how a headless server picks a save to continue from at start-up;
that part is the engine's, and has to be established before the plan is written.

The addon already has the half of a resume that the engine does not provide. A player
who drops mid-game keeps their body in the world, their slot is held on their identity,
and on return they are put straight back into the same body. A resume is that flow
applied to everyone at once, after the world itself has been put back.

## Clarifications

### Session 2026-09-14

- Q: What happens to an aircraft that was airborne at the save point? → A: it is held
  motionless in place, like everything else in the hold, if the engine allows it; if
  the plan finds no way to hold an aircraft, the question comes back to the operator
  before a fallback is chosen.
- Q: Is the resume automatic or admin-confirmed? → A: the world is loaded automatically
  when the server starts with a saved session, and the game is then held until an admin
  releases it. The game has no scripted world pause, so "paused" means the hold below:
  characters locked, vehicles and aircraft motionless, mission logic suspended.
- Q: When does the hold lift? → A: only when an admin releases it. There is no timer
  and no player-count rule.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The game continues where it stopped (Priority: P1)

A hundred-plus players are forty minutes into a mission when the server process dies.
The server owner starts the server again. Instead of slot selection, the mission comes
up in the game phase with the world as it was at the last save point: buildings that
were destroyed are ruins, vehicles stand where they were, bodies lie where they fell,
dropped weapons are on the ground, doors are open or shut as they were, the clock and
the weather are where they were, and every player's own character is standing where it
stood, with the same wounds and the same kit. Players reconnect and are put back into
their own bodies. The game goes on.

**Why this priority**: it is the whole feature. A multi-hour event with a hundred
people has no other way to survive a crash.

**Independent Test**: on a dedicated server running the demo world with a handful of
players, play for a few minutes, destroy a building, wound a character, park a vehicle,
drop a weapon, let a save point happen, kill the server process, start it again and
reconnect everyone.

**Acceptance Scenarios**:

1. **Given** a game in progress with a save point taken, **When** the server is
   killed and started again, **Then** the mission comes up in the game phase, not in
   slot selection, and every slot shows the same occupant as before.
2. **Given** the resumed server, **When** a player who held a slot reconnects,
   **Then** they are placed into the same character, at the position, stance, health
   and inventory it had at the save point, without passing through slot selection or
   briefing.
3. **Given** a building destroyed before the save point, **When** the world is
   resumed, **Then** it is a ruin, and any building destroyed after the save point is
   intact.
4. **Given** vehicles in the world, **When** the world is resumed, **Then** each
   vehicle stands at its saved position with its saved damage, fuel and cargo, and any
   crew that was aboard at the save point is back in the same seats when they
   reconnect.
5. **Given** a player who was dead and spectating at the save point, **When** they
   reconnect, **Then** they are a spectator, their slot still shows KIA, and their body
   is in the world where it fell.
6. **Given** a trigger that had already fired before the save point, **When** the
   world is resumed, **Then** it does not fire again, and a mission-end countdown
   continues from its remaining time rather than restarting.
7. **Given** the freeze period was still running at the save point, **When** the world
   is resumed, **Then** the freeze continues with its remaining time and the freeze
   zones still apply.
8. **Given** unit statistics accumulated before the save point, **When** the game is
   resumed and later published, **Then** the pre-crash kills are in the numbers.
9. **Given** markers players had placed on the map before the save point, **When**
   the world is resumed, **Then** the markers are back, visible to the same factions.

---

### User Story 2 - Nobody gains from being first back (Priority: P1)

The server comes back before the players do. Reconnecting takes some people a minute
and others ten. During that window, the resumed world is held: no character can move,
shoot or take damage, no vehicle can be driven, and the map shows who is back. An
admin releases the hold when enough people are in. Only then does play continue.

**Why this priority**: without it a resume is unfair by construction. The first player
back can walk up to a defenceless enemy body and shoot it, and a player still loading
their game loses a character they never had a chance to defend.

**Independent Test**: resume the demo world with two players on opposite sides,
reconnect one first, confirm they cannot move, shoot or damage the other's body,
reconnect the second, release the hold, confirm play works.

**Acceptance Scenarios**:

1. **Given** a resumed world with the hold active, **When** a player is back in their
   body, **Then** they cannot move, fire, enter a vehicle or damage anything, and a
   visible message tells them the game is resuming and how many players are back.
2. **Given** the hold is active, **When** an admin releases it, **Then** every
   connected player can act at the same moment and a broadcast says the game is on.
3. **Given** the hold is active and no admin has released it, **When** any amount of
   time passes, **Then** the hold stays; nothing lifts it but an admin.
4. **Given** a player who has not reconnected when the hold lifts, **When** they
   connect later, **Then** their slot is still theirs and they are put back into their
   body, exactly as after an ordinary mid-game disconnect.

---

### User Story 3 - The server owner controls what is saved and when (Priority: P2)

A mission maker switches session saves on for a mission and sets how often the world
is saved. During the game, an admin can take a save point on demand with a chat
command, see when the last save point was taken, and, when the group decides not to
continue a broken game, discard the saved session so the next start is a fresh one. A
mission that never touches the setting behaves exactly as today: no saves, no resume.

**Why this priority**: a save costs server time and can lose up to one interval of
play; the people running the event must own that trade-off, and must be able to say
"start over".

**Independent Test**: in the demo world, enable saves with a short interval, watch
save points appear in the log at that cadence, take one with the command, discard the
session, restart and confirm the mission starts at slot selection.

**Acceptance Scenarios**:

1. **Given** a mission with session saves off (the default), **When** the game runs
   and the server restarts, **Then** no save point is ever taken and the mission
   starts at slot selection.
2. **Given** session saves on with an interval, **When** the game phase runs,
   **Then** a save point is taken at that interval, and each one is logged with its
   time and duration.
3. **Given** the game phase, **When** an admin issues the save command, **Then** a
   save point is taken now and a confirmation names the time; a non-admin gets a
   refusal.
4. **Given** a saved session exists, **When** an admin issues the discard command and
   confirms, **Then** the next server start is a fresh mission.
5. **Given** the mission reaches debriefing, **When** the game ends, **Then** the saved
   session is discarded so the next start is fresh.
6. **Given** the server is stopped on purpose during the game phase, **When** it is
   started again, **Then** it resumes from a save point taken at the stop, not from
   the last periodic one.

---

### User Story 4 - The save is not felt by the players (Priority: P2)

A save point during a 127-player game is not something players notice. Nobody is
kicked by it, nobody rubber-bands, voice does not cut out, and the server log records
how long it took so the interval can be tuned.

**Why this priority**: the addon exists because the previous lobbies flooded the
server at scale. A save that stalls the server for seconds would reintroduce the
kicks the addon was written to remove.

**Independent Test**: on a dedicated server at full player count, take save points at
the chosen interval for an hour and read the per-save duration from the log and the
disconnect reasons from the server log.

**Acceptance Scenarios**:

1. **Given** a full server in the game phase, **When** a save point is taken,
   **Then** no player is disconnected because of it and players see no pause in
   movement, fire or voice.
2. **Given** a save point in progress, **When** a player disconnects, dies or enters a
   vehicle during it, **Then** the save completes and the world state it captured is
   internally consistent.

---

### Edge Cases

- **Aircraft in the air at the save point.** The game restores it at its saved position
  with no motion. It is held motionless there for the whole hold, and released with
  everything else, so its pilot can be back in the seat before it moves again. If the
  engine offers no way to hold an aircraft, the operator decides the fallback; the
  plan does not pick one.
- **Ground vehicles moving at the save point** come back standing still at the saved
  position; the loss of momentum is accepted.
- **A vehicle whose driver has not returned** stays parked with its engine off; a
  crew member who is back sits in their seat inside the hold like everyone else.
- **Towed, slung or roped loads** come back detached at their own saved positions; the
  connection is not restored.
- **Fences, walls, trees, tyres, glass, lamp posts and other small destructible
  objects** come back intact; the game keeps no record of them. Buildings and vehicles
  keep their damage; craters, fires, wrecks and debris do not come back.
- **A dead body** comes back dead, at its saved position, but the game does not keep
  the pose it fell in.
- **Players who were spectating without a slot** at the save point come back as
  spectators.
- **Players who never return** keep their body in the world and their slot, as after
  an ordinary disconnect; the game's own one-minute deletion of unclaimed characters
  after a load MUST NOT apply.
- **A new player** joining a resumed game is treated as a mid-game joiner: spectator,
  or into a free slot if the admin allows it, as today.
- **The hard freeze** at game start, if still running at the save point, resumes with
  its remaining time inside the resume hold; the freeze zones exist again and confine
  the same players.
- **A save point during slot selection or briefing** is not taken; a crash before the
  game phase restarts the mission from slot selection. Slotting itself is not saved.
- **A save point during debriefing** is not taken; reaching debriefing discards the
  session.
- **The server crashes during a save point.** The previous complete save point remains
  valid and is the one resumed from; a half-written one is never loaded.
- **Two servers, same mission.** A saved session belongs to the server profile that
  wrote it; a different server starting the same mission starts fresh.
- **Mission or addon updated between the crash and the restart.** If the saved world
  no longer matches the mission (a slot missing, a prefab gone), the resume is refused
  with a logged reason and the mission starts fresh rather than half-loaded.
- **Third-party vehicles and weapons** from other addons are saved as long as they are
  built on the game's own base prefabs; anything else is absent after the resume and
  is logged.
- **Game Master changes** made before the save point (placed, moved or deleted
  entities) come back; changes after it are lost like everything else.
- **Squad radio nets** assigned at game start are restored, not reassigned, so a
  squad keeps the frequency written in its briefing.
- **Time of day and weather** continue from the save point, not from the mission's
  start settings.
- **Admin identity** is checked again on reconnect; being an admin before the crash
  grants nothing after it.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The game mode MUST offer a mission-maker setting that turns session
  saves on or off for the mission, off by default, and a save interval in minutes.
- **FR-002**: With saves on, the server MUST take a save point at the configured
  interval while the mission is in the game phase, and MUST NOT take one in any other
  phase.
- **FR-003**: A save point MUST capture the state the game itself can save for every
  tracked entity in the world: position, identity, hit-zone damage, inventory and
  equipment, seating, fuel, lights, magazines and attachments, armed mines, doors,
  ruined buildings, items on the ground, AI groups and their waypoints, entities
  placed by a Game Master, and time and weather.
- **FR-004**: A save point MUST additionally capture the addon's own game state: the
  slot table with each slot's occupant identity, KIA status and lock, the elapsed game
  time and remaining freeze and hard-freeze time, the fired state of every trigger and
  the remaining time of any mission-end countdown, unit statistics accumulated so far,
  player-placed map markers with their faction, and the reservation of every slot whose
  player is disconnected.
- **FR-005**: When the server starts a mission for which a valid saved session exists,
  it MUST continue from the newest complete save point, in the game phase, without
  passing through slot selection or briefing, and MUST NOT spawn fresh bodies or
  restart the freeze. The load itself needs no admin action; the hold that follows
  does.
- **FR-006**: A reconnecting player whose identity held a slot at the save point MUST
  be put back into that slot's character, using the same identity-based reconnect rule
  as an ordinary mid-game disconnect, and a player whose slot was KIA MUST come back as
  a spectator with the slot still marked KIA.
- **FR-007**: A resumed world MUST start in a hold in which no character can move,
  fire, enter a vehicle or take damage, no vehicle or aircraft moves, and no trigger
  or mission timer advances, until an admin releases it with a chat command the server
  validates as admin. There is no automatic release. The hold and its release MUST be
  announced to every connected player, and the hold MUST display how many players are
  back.
- **FR-008**: The addon MUST NOT delete a saved player character because its owner has
  not reconnected within any timeout; a slot stays held as it does after an ordinary
  disconnect during the game.
- **FR-009**: An admin MUST be able to take a save point on demand, see the time of
  the last save point, and discard the saved session, each through a chat command that
  the server validates as admin; discarding MUST require a confirmation.
- **FR-010**: Reaching debriefing MUST discard the saved session so the next start of
  the mission is fresh.
- **FR-011**: A deliberate server stop during the game phase MUST take a final save
  point before the process exits, so a planned restart resumes from the moment of the
  stop.
- **FR-012**: A save point MUST never disconnect a player and MUST NOT stall the
  server long enough for players to notice; every save point MUST be logged with its
  time, type and duration.
- **FR-013**: A save point that did not complete MUST never be resumed from; the
  previous complete one is used.
- **FR-014**: A saved session that no longer matches the mission MUST be refused with a
  logged reason, and the mission MUST start fresh.
- **FR-015**: Saving and resuming MUST add no replicated value, remote call or
  join-in-progress payload beyond what the addon already sends; all save and load work
  is server-side, and a resume reaches clients only through the existing reconnect
  flow.
- **FR-016**: A mission with the setting off MUST behave exactly as before this
  feature: no save points, no files written, no resume, no log lines about saving.

### Key Entities

- **Save point**: one complete snapshot of the world and the addon's game state, taken
  at a moment in the game phase; has a time, a type (periodic, admin, stop) and a
  duration; only complete ones are ever resumed from.
- **Saved session**: the set of save points for one mission on one server profile,
  from the game phase start until debriefing or an admin discard; at most one is
  resumable at a time.
- **Slot record**: what the addon writes per slot into a save point: the slot's
  character, its occupant's identity, KIA status, lock, and the group it belongs to.
- **Resume hold**: the protected period after a resume, from world load until an admin
  releases it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After a forced server kill on a dedicated server with 127 slotted
  players in the game phase, every player who reconnects within the hold is back in
  their own character within 1 metre of its saved position, with the same health,
  inventory and seat, and the mission is in the game phase with the same slot
  occupants; verified on one live event and in the demo world.
- **SC-002**: The play lost by a crash is never more than one save interval, and with
  the default interval no more than five minutes.
- **SC-003**: During a one-hour game at 127 players with saves at the default interval,
  zero players are disconnected by a save point and every save point's logged duration
  stays under one second.
- **SC-004**: In the demo world, a building destroyed, a vehicle damaged and refuelled,
  a weapon dropped, a mine armed, a door opened and a trigger fired before the save
  point are all in the same state after the resume, in one side-by-side check.
- **SC-005**: With the setting off, a server restart behaves exactly as before the
  feature and no save-related file or log line exists.
- **SC-006**: Network traffic per client during the game and at reconnect is not
  higher than before the feature, measured on a dedicated server.

## Assumptions

- The mission maker opts in per mission; the addon ships with saves off because a
  save changes what a server restart does, which a server owner must choose.
- The default save interval is five minutes; the mission maker can change it.
- Only the game phase is saved. Slot selection and briefing are short enough to redo,
  and saving them would add a second resume path for little value.
- Momentum is not restored for any vehicle; the loss is accepted for ground vehicles,
  and aircraft are held in place until the release.
- Small destructible objects, craters, fires, wrecks and body poses are not restored,
  because the game keeps no record of them; this is stated to players as a known
  limit of a resume, not fixed by this feature.
- The resume reuses the addon's existing identity-based reconnect and body
  re-possession flow, and the existing hard-freeze mechanics for the hold, rather than
  introducing a second reconnect or a second freeze. The hard freeze today locks
  characters on foot only; holding vehicles and aircraft is new ground for the plan.
- The Game Master's own placed entities are restored by the game; the addon only makes
  sure its configuration includes them.
- How a headless dedicated server selects the save to continue from is established in
  the planning phase; if the engine offers no way to do it, the addon performs the
  selection itself at mission start.
- A saved session is tied to the server profile that wrote it; sharing sessions
  between machines is out of scope.
