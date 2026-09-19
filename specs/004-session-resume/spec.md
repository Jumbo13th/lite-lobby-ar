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
a save was loaded. The server configuration already schedules automatic saves every ten
minutes, keeps the last ten, and by default loads the newest on start-up; a launch
parameter loads a chosen one instead.

The same system leaves gaps that matter for a Lite Lobby mission. No entity keeps its
motion: a vehicle saved at speed comes back standing still, and a helicopter saved in
the air comes back in the air with no forward speed and no lift. Nothing that is towed,
slung or roped is reconnected. Fences, walls, trees, tyres, glass and other small
destructible objects have no record at all and come back intact. Vehicle wrecks, fires,
craters and debris are not saved. A dead body comes back dead but not in the pose it
fell in. Player-placed map markers have no record. The game's own reconnect bookkeeping
deletes a saved player character whose owner does not return within a minute after the
load, which contradicts the addon's rule that a slot is held for the whole game.

An earlier attempt to leave the game's save types enabled ended with every slot
doubled after a restart: the game put the saved squad members back, and the squads
placed in the mission spawned a fresh set on top. The mission guide has told makers
to untick the save types ever since. A resume must not repeat that, and the guide
step reverses when this feature ships.

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
- Q: Is the resume automatic or admin-confirmed? → A: superseded on 2026-09-18, see
  below.
- Q: When does the hold lift? → A: only when an admin releases it. There is no timer
  and no player-count rule.

### Session 2026-09-18

- Q: Who sets the automatic-save cadence? → A: the server owner, in the game's own
  server configuration. The addon adds no interval setting and no second save clock;
  it only decides in which phase saving is allowed.
- Q: How is "resume or start fresh" decided? → A: snapshots are always taken during
  the game phase; a restart is a fresh start by default; a resume is a deliberate
  choice made by whoever restarts the server, who names the newest snapshot or a
  specific one at launch. No admin chat command takes part in the decision.
- Q: Which admin commands does the feature add? → A: none. The hold is released
  with the existing hard-freeze command; ending a game is the existing advance to
  debriefing; freeing a slot is the existing kick.
- Q: What statistics does a resumed game carry? → A: the statistics as they were at
  the snapshot, never newer than the world.

### Session 2026-09-18, second pass (external design review)

The spec was reviewed against the game scripts before implementation. The answers
below are the operator's decisions; the four that were first recorded as assumptions
were confirmed by the operator the same day.

- Q: How does a restored body find its squad again? → A: the game saves squad
  membership for AI members only; a body a player controlled at the snapshot comes
  back with no squad. The slot record therefore names the squad, and the resume
  re-attaches every restored body to its squad before the slot registers.
- Q: What happens when a snapshot cannot be resumed (unknown record version, a slot
  whose body or squad is missing, no lobby record at all)? → A: the resume is
  refused as a whole. The server logs the cause, keeps every snapshot on
  disk for diagnosis and rollback, and shuts itself down so that whoever restarted it
  starts again plainly or names an older snapshot. A partly restored world is never
  played on, and nothing is purged by a refusal.
- Q: What if the statistics file named by the snapshot is missing or unreadable? →
  A: the world still resumes; the statistics recorder starts from
  what the snapshot carries in its own record (nothing, in that case) and the log and
  the next statistics publish say that the game's statistics are incomplete. World
  recovery outranks statistics.
- Q: Does a restored slot lock block the slot's own holder? → A: no. A lock keeps
  strangers out; the holder identity saved with the slot is always put back.
- Q: What does a reconnecting player who connected while the world was still loading
  get? → A: the same as anyone else. The resume finalisation replays the reservation
  claim for every player already connected, and a player whose identity is verified
  only later is claimed when it arrives.
- Q: Which identities may hold a crash reservation? → A: exactly the identities
  the existing reconnect rule accepts: the platform identity, or the
  display-name form where the mission enables it, with the name form's known limits
  (a renamed or duplicated name is not recognised).
- Q: What does the hold guarantee, precisely? → A: no connected player can move,
  fire, enter or leave a vehicle or use the map's placement tools; no character or
  vehicle, player or AI, can take damage; no vehicle engine can be started; every
  ground vehicle is stopped and braked where the engine offers a brake; every mission
  clock, trigger and zone timer stands still; daylight stands still. The game offers
  no way to pause its AI or its physics from script: mission AI may walk during the
  hold but cannot harm or be harmed, and a tracked vehicle on a slope may creep. Both
  are accepted limits, stated to admins in the hold text.
- Q: Which vehicle recipe holds a helicopter? → A: the game's own pilot-dropped
  recipe (wheel brake and autohover, engine left as it is); the forced engine stop is
  documented for cinematics and is not used. An airborne helicopter is additionally
  pinned, which remains the one spike.
- Q: If the launch parameter does not override the configuration file? → A: no
  addon fallback is built. The operator's alternative is the configuration value
  itself, flipped to true for that one start, which needs no code; the choice comes
  back to the operator with that option before anything else is considered.
- Q: How are statistics files kept and pruned? → A: a statistics file lives as long
  as the engine still lists the snapshot that names it; pruning follows the engine's
  own retention, never a count of the addon's own.
- Q: Which deployment path is the incident procedure? → A: the container launcher
  supplies the resume parameter for one start and drops it again;
  the exact entrypoint, signal, profile mount and stop budget are recorded in the
  contract after the planned-stop measurement, with margin, not equal to one measured
  exit.

### Session 2026-09-19, third pass (second external review)

Integration corrections to the mechanisms added the day before. All are technical
decisions with one right answer in the code; the one product question (a corpse
removed before the snapshot) follows existing behaviour.

- Q: How is a squad identified in the snapshot? → A: by the squad entity's own name,
  never by the roster label, which is the translated callsign when the squad has
  one. Every squad in a mission with session saves needs a unique entity name; the
  game mode checks this at game start and logs any squad without one, and the
  guide says so.
- Q: How does a returning holder get an alive slot back? → A: ownership transfers
  first, then the character is possessed without the free-slot acquisition step,
  keeping the slot-assignment event, the faction update and the controller-readiness
  delay the ordinary take provides. A reservation is consumed exactly once,
  whichever of the connect, identity-audit or finalisation paths claims it.
- Q: When can a restored body be attached to its squad? → A: only once its AI
  agent exists, which is not guaranteed at the moment the body is available. One
  bounded retry covers agent readiness, attachment and registration, and shares the
  finalisation deadline.
- Q: What if a registered body is not tracked by the game's persistence at save
  time? → A: the slot is written with an empty body reference, the save logs the
  slot by name, and a resume from that snapshot refuses with that cause, so the
  mission maker learns the body's prefab lacks persistence. The game mode also
  logs any untracked registered body at game start, before an event.
- Q: What happens to a slot whose corpse was removed before the snapshot? → A: what
  happens today: the slot left the roster with the body and is not in the
  snapshot. A squad whose bodies were all removed comes back with no slots.
- Q: Which clock is the mission clock? → A: the game's own elapsed time, which
  every machine advances and the server corrects on the wire every ten seconds. The
  lobby stops it on every machine while a hard freeze or a resume hold is active,
  so the spectator clock, the statistics recorder, the snapshot and the mission
  timers all read one value that never counts held time. No lobby timestamp is
  rebased and no value is added to the wire.
- Q: What does a snapshot during a timed hard freeze save? → A: the freeze's
  current remainder. A snapshot during a resume hold saves the remainder the hold
  was protecting. Either release schedules the ordinary freeze countdown exactly
  once.
- Q: What about a trigger that had not started when the snapshot was taken? → A:
  the record says so, and the trigger initialises normally after the resume; only
  a trigger that had started keeps its restored fields through its first
  activation.
- Q: Who pauses the freeze-zone countdown during the hold? → A: the client zone
  component, which reads the replicated hold flag; the server kill decision stays
  server-validated. A player restored outside their zone is enforced from their
  first entry, as any player who has not yet entered is today.
- Q: Can a returning player place markers during the hold? → A: no; the existing
  server marker gate and the map placement flow both check the hold.
- Q: What exactly does a refusal do? → A: disables saving and the save types first,
  so no shutdown save can turn the half-restored world into a newer snapshot, then
  logs the cause and asks the game to close. A container that restarts the process
  afterwards starts a fresh game, because the launcher passes the resume parameter
  once; that is acceptable, the refusal line in the log is what the operator reads.
- Q: How are statistics files pruned? → A: a file is deleted only after a
  successful listing of the engine's save points, when it is older than the oldest
  listed point by more than one minute, and never while a write is in flight. A
  file from an abandoned branch after a rollback stays until it is that old; a
  bounded leftover, accepted.
- Q: How does the statistics recorder handle absent holders? → A: it resolves
  every identity through the lobby's reconnect keys, placeholders included, so a
  holder absent through the publish keeps their credit; on a resume it loads the
  snapshot's file instead of starting a new recording, and the incomplete flag is
  saved in the file so it survives another resume.
- Q: When exactly is saving excluded around a body replacement? → A: one
  acquisition before the spawn and one release on each terminal exit: the spawn
  failure return, the finish's success, its abandonment after the retry limit and
  its hand-off failure; never on a retry.
- Q: How does a planned stop wait for a save in flight? → A: as the game's own
  exit flow does: wait for the completion callback of the running save, then
  request the stop-time snapshot, then let the process exit. A stop with a
  replacement pending lets the replacement finish first, then the queued save runs.
  The engine's part of the ordering is a dedicated-server gate.
- Q: Does a returning occupant of a vehicle that crept during the hold still meet
  the one-metre criterion? → A: the criterion excludes the accepted creep: it is
  measured against the vehicle's position at release.

### Session 2026-09-19, fourth pass (third external review)

- Q: Does time stand still during every hard freeze, or only during the resume
  hold? → A (operator): every hard freeze. One rule: a hard freeze means time
  stands still, for the mission clock, the freeze countdown, daylight and
  triggers, whether saves are on or off. The start-of-game hard freeze therefore
  no longer eats into the freeze time: a mission with a one-minute hard freeze and
  a ten-minute freeze is frozen for eleven minutes. FR-016 is amended, the
  attribute description and the guide say so.
- Q: How is the mission clock held? → A: not by subtracting; the value at the
  moment the hard freeze began is written back over the game's clock every frame
  while the freeze lasts, on every machine, so it cannot run backwards on an empty
  server and needs no knowledge of when the game would have advanced it.
- Q: What does a spectator see as the mission clock? → A (operator): the time
  until the mission ends, when the mission has a mission-end timer; it stands
  still during the freeze and every hard freeze, and drains otherwise. Spectators
  only; alive players see nothing new. A mission without a mission-end timer keeps
  the elapsed clock. Kept in this spec because the countdown is derived from the
  held clock and must survive a resume.
- Q: How does the mission-end countdown reach spectators? → A: two values set once
  on the server, the timer's configured duration and the clock reading at which it
  started (unset until then). Clients derive the remaining time from the replicated
  clock. Both are in the snapshot. This raises the feature's replicated additions
  to three scalars, all set once; FR-015 is amended.
- Q: Which trigger state is "started"? → A: for the mission-end timer, that its
  countdown is running, not that the freeze wait has been queued. A snapshot taken
  between the end of the hard freeze and the end of the freeze saves the timer as
  not started and it initialises normally after the resume.
- Q: When is a restored body "done"? → A: when it is registered in its saved squad
  with a valid network id. Agent readiness, attachment and registration are one
  operation on one absolute deadline; a fallback id is a refusal, and outstanding
  attempts stop when the resume is refused.
- Q: Who owns the assignment when a returning holder claims an alive slot? → A: the
  claim. It transfers ownership (one assignment event, one broadcast), updates the
  player's faction and possesses, without leaving any slot first. The ordinary take
  path is not changed at all: it stays synchronous, the reconnect caller keeps its
  own readiness delay, and the Game Master revive keeps its take-then-delete order.
- Q: How does the lobby learn that the native load failed? → A: through a modded
  persistence system class forwarding the game's own protected after-load event to
  the game mode, the addon's usual modded-class pattern; a failed load refuses
  before anything else runs.
- Q: What does a resume do to the statistics recorder? → A: it arms it: recording
  flag, assignment listener and periodic live write, exactly once, without the fresh
  participant sweep, the commander freeze or the immediate write. The same arming
  runs when the snapshot's file is missing.
- Q: Which exits release the replacement exclusion? → A: every path that ends the
  replacement after the spawn: the spawn-failure return, the finish finding its new
  body gone, the finish's success, its abandonment after the retry limit, its
  hand-off failure. Never the retry.
- Q: How does the planned stop wait? → A: through the save manager's busy-state
  event, once: when the manager is busy or a replacement is pending, wait for the
  busy state to clear and the replacement count to reach zero, then request the
  stop-time save exactly once and stop listening. The stop-time save's own
  completion never re-triggers the request.
- Q: When do the start-time checks run? → A: when the roster is complete, at the
  transition into the game phase, before the first save is allowed; not at world
  start when squads are still spawning.
- Q: What does the daylight getter return during an ordinary timed hard freeze? →
  A: the setting from before that freeze, which the game mode already keeps; never
  the live, already-disabled value.

### Session 2026-09-19, fifth pass (fourth external review)

Sequencing contracts around the previous corrections. The one product question
(how many mission-end timers FR-023 represents) follows the simplest reading.

- Q: How many mission-end timers does the spectator countdown represent? → A:
  one per mission, with a positive duration. The roster check logs a mission with
  more than one such timer or one with a zero duration; the first registered
  timer drives the display in that case, with the log line saying so.
- Q: Which clock does the mission-end timer itself fire from? → A: the mission
  clock, the same one spectators read. The timer fires when the clock reaches its
  start reading plus its duration, checked each tick; its remaining time is that
  difference. Display, saved state and firing share one basis, so an empty
  server, a fractional hold boundary or a delayed callback cannot separate them.
  The saved state is the start reading; a restored started timer keeps it.
- Q: How does a client adopt a clock correction while held? → A: before the game
  advances the clock in a frame, a client compares the replicated value with the
  one it last saw; a difference is a correction and becomes the new held anchor.
  Then the anchor is written back. A client that sees the hold flag before the
  value holds its local estimate until the value arrives. Native packet ordering
  stays a dedicated-server check.
- Q: Who schedules and cancels the freeze countdown? → A: one helper, used by
  the fresh start, both hard-freeze releases, the admin adjustment and the
  explicit end, with one flag that says whether the countdown is scheduled. No
  path schedules it directly.
- Q: Which reservations does the resume claim consume? → A: only those whose slot
  is currently held by a resume placeholder. An ordinary disconnected holder's
  reservation stays on the existing reconnect path with its lock and phase rules.
- Q: What happens after the claim's delayed possession? → A: the voice manager
  re-evaluates the player's parking, and any spectator entry still pending for
  that player is dropped because they now control a living body. Spectator entry
  always re-checks that condition when it fires.
- Q: When does the body operation's deadline start? → A: at ACTIVE. A body that
  becomes available earlier reads the deadline from the game mode on each tick
  and has no timeout until it is armed. Done means the manager holds a registered
  slot for that body in its saved squad, observed, not assumed from a call.
- Q: When is the first save allowed after a fresh start or a resume? → A: when the
  game phase has been entered and no body replacement is pending; the release of
  the last pending replacement is what allows it. The resume finaliser re-allows
  saving the same way at its end. Never at the return of the fresh-start entry,
  which only begins the replacements.
- Q: What does a native load failure do? → A: it refuses at once, on its own,
  through the same idempotent refusal; it does not wait for ACTIVE. A failure
  reported after finalisation still refuses.
- Q: What wakes a save waiter? → A: the save manager's busy-state change and the
  replacement count reaching zero, both; the waiter re-evaluates on either.
- Q: Which path owns the one stop-time snapshot? → A: whichever completes first.
  After the addon's stop-time save completes, saving is disabled so the game's own
  exit save cannot write a second; if a shutdown-type save completed before the
  addon's request ran, the addon does not request one.

### Session 2026-09-19, sixth pass (fifth external review, final)

Verdict "ready with the listed corrections"; the corrections are recorded here
and the specification goes to implementation.

- Q: When exactly is the first save of a fresh game allowed? → A: only after the
  fresh-start entry has dispatched every body replacement and finished its own
  setup, and no replacement is outstanding. A count of zero reached before the
  dispatch is complete allows nothing. A mission with nothing to replace, or one
  whose every spawn failed at once, is allowed at the end of that entry.
- Q: How does a restored, started mission-end timer resume? → A: directly in its
  countdown, with its saved start reading; it does not replay the activation poll
  or the freeze wait, so an overdue deadline fires within one evaluation of the
  release.
- Q: What does the daylight getter return during a resume hold? → A: the intent
  saved in the record, never the live setting the game's weather record restored;
  re-asserting the stopped daylight during a resume does not overwrite that intent.
- Q: Two named calls in the tasks did not exist or were not callable. → A: fixed
  in the tasks: a public parking refresh on the voice manager, and the existing
  slot lookup on the playable component for the registration observation.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The game continues where it stopped (Priority: P1)

A hundred-plus players are forty minutes into a mission when the server process dies.
The server owner starts the server again and asks it to continue from the newest
snapshot. Instead of slot selection, the mission comes up in the game phase with the
world as it was at that snapshot: buildings that were destroyed are ruins, vehicles
stand where they were, bodies lie where they fell, dropped weapons are on the ground,
doors are open or shut as they were, the clock and the weather are where they were,
and every player's own character is standing where it stood, with the same wounds and
the same kit, in the same squad. Players reconnect and are put back into their own
bodies. The game goes on. Exactly one copy of everything exists.

**Why this priority**: it is the whole feature. A multi-hour event with a hundred
people has no other way to survive a crash.

**Independent Test**: on a dedicated server running the demo world with a handful of
players, play for a few minutes, destroy a building, wound a character, park a vehicle,
drop a weapon, let a snapshot happen, kill the server process, start it again with the
resume parameter and reconnect everyone.

**Acceptance Scenarios**:

1. **Given** a game in progress with a snapshot taken, **When** the server is killed
   and started again with the resume parameter, **Then** the mission comes up in the
   game phase, not in slot selection, every slot shows the same occupant as before,
   and the roster has exactly as many slots as at the snapshot.
2. **Given** the resumed server, **When** a player who held a slot reconnects,
   **Then** they are placed into the same character, at the position, stance, health
   and inventory it had at the snapshot, without passing through slot selection or
   briefing.
3. **Given** a building destroyed before the snapshot, **When** the world is resumed,
   **Then** it is a ruin, and any building destroyed after the snapshot is intact.
4. **Given** vehicles in the world, **When** the world is resumed, **Then** each
   vehicle stands at its saved position with its saved damage, fuel and cargo, and any
   crew that was aboard at the snapshot is back in the same seats when they reconnect.
5. **Given** a player who was dead and spectating at the snapshot, **When** they
   reconnect, **Then** they are a spectator, their slot still shows KIA, and their body
   is in the world where it fell.
6. **Given** a trigger that had already fired before the snapshot, **When** the world
   is resumed, **Then** it does not fire again, and a mission-end countdown continues
   from its remaining time rather than restarting.
7. **Given** the freeze period was still running at the snapshot, **When** the world
   is resumed, **Then** the freeze continues with its remaining time and the freeze
   zones still apply.
8. **Given** unit statistics accumulated before the snapshot, **When** the game is
   resumed and later published, **Then** the numbers are those of the snapshot: kills
   before it are in, kills after it are not.
9. **Given** markers players had placed on the map before the snapshot, **When** the
   world is resumed, **Then** the markers are back, visible to the same factions.
10. **Given** a squad placed in the mission whose members were restored, **When** the
    world is resumed, **Then** the squad has its restored members only and no second
    set standing at its spawn point.
11. **Given** a squad whose every member was player-controlled at the snapshot,
    **When** the world is resumed, **Then** every one of its bodies is back in that
    squad, the roster lists the squad with the same slots and holders, and its radio
    frequency is the one in its briefing.
12. **Given** a player who connected while the world was still loading, **When** the
    resume finalises, **Then** they are put into their own body like a player who
    connected later.
13. **Given** a snapshot whose lobby record is missing, of an unknown version, or
    names a body or squad that did not come back, **When** the server is started
    with it, **Then** the server logs the cause, keeps every snapshot, and shuts down
    without entering the game phase.

---

### User Story 2 - Nobody gains from being first back (Priority: P1)

The server comes back before the players do. Reconnecting takes some people a minute
and others ten. During that window, the resumed world is held: no character can move,
shoot or take damage, no vehicle can be driven, and the map shows who is back. An
admin releases the hold when enough people are in, with the same command that ends any
hard freeze. Only then does play continue, and the mission's clocks continue from the
snapshot as if the wait had not happened. This is the same rule every hard freeze
follows from this feature on: while a hard freeze lasts, time stands still. A
spectator watching either kind of hold sees the mission-end countdown standing still
too, and sees it drain again when play resumes.

**Why this priority**: without it a resume is unfair by construction. The first player
back can walk up to a defenceless enemy body and shoot it, and a player still loading
their game loses a character they never had a chance to defend.

**Independent Test**: resume the demo world with two players on opposite sides,
reconnect one first, confirm they cannot move, shoot or damage the other's body,
reconnect the second, release the hold, confirm play works and the countdown resumed
from its saved value.

**Acceptance Scenarios**:

1. **Given** a resumed world with the hold active, **When** a player is back in their
   body, **Then** they cannot move, fire, enter a vehicle or damage anything, and a
   visible message tells them the game is resuming, from which time the snapshot is,
   and how many players are back.
6. **Given** a resumed world with the hold active, **When** an AI character fires at
   a restored body, drives a vehicle, or a vehicle stands on a slope, **Then** no
   character or vehicle takes damage, no engine can be started, and any creep of a
   braked or tracked vehicle is the only movement, logged at release as the
   distance moved.
7. **Given** the hold is active, **When** a second snapshot is taken during it and
   the server is resumed from that one, **Then** the saved clocks are those of the
   moment before the hold began, not of the wait, and daylight after the release
   follows the mission's setting from before the hold.
8. **Given** a mission with a one-minute hard freeze and a ten-minute freeze, saves
   on or off, **When** the game starts, **Then** the mission clock reads zero until
   the hard freeze ends, the freeze countdown starts only then, and the freeze
   ends eleven minutes after the game started.
9. **Given** an admin hard freeze in the middle of the game, **When** it lasts five
   minutes, **Then** the mission clock, the freeze countdown if any, daylight, the
   triggers and the spectators' mission-end countdown all read the same value at
   its end as at its start.
10. **Given** a spectator in a mission with a mission-end timer, **When** they look
    at the spectator clock, **Then** it shows the time until the mission ends, held
    still during the freeze and every hard freeze, draining otherwise, and the same
    value on every spectator's screen within a second; a mission without that
    timer shows the elapsed clock as today.
11. **Given** a resumed game, **When** a spectator reconnects during the hold,
    **Then** their countdown shows the snapshot's remaining mission time, standing
    still until the release.
2. **Given** the hold is active, **When** an admin releases it, **Then** every
   connected player can act at the same moment and a broadcast says the game is on.
3. **Given** the hold is active and no admin has released it, **When** any amount of
   time passes, **Then** the hold stays; nothing lifts it but an admin.
4. **Given** a player who has not reconnected when the hold lifts, **When** they
   connect later, **Then** their slot is still theirs and they are put back into their
   body, exactly as after an ordinary mid-game disconnect.
5. **Given** a mission-end countdown with 45 minutes left at the snapshot and a hold
   of ten minutes, **When** the hold is released, **Then** the countdown reads 45
   minutes, and the game clock, freeze clock and daylight resume from their saved
   values.

---

### User Story 3 - Snapshots always, resume on purpose (Priority: P2)

A mission maker switches session saves on for a mission. From then on the game phase
is snapshotted on the server's schedule without anyone doing anything, and the last
ten snapshots are kept. A restart is a fresh start, as today. When the people running
the event want to continue a game, whoever restarts the server names the newest
snapshot or a specific older one, and that is the resume. A mission that never touches
the setting behaves exactly as today: no snapshots, no resume.

**Why this priority**: it keeps the decision with the people who restart the server
and gives them rollback for free, without teaching admins anything new in chat.

**Independent Test**: in the demo world, enable saves, set a short interval in the
server configuration, watch snapshots appear in the log at that cadence and only in
the game phase, restart plainly and see slot selection, restart with the resume
parameter and see the resumed game, restart with an older snapshot named and see that
older world.

**Acceptance Scenarios**:

1. **Given** a mission with session saves off (the default), **When** the game runs
   and the server restarts with or without the resume parameter, **Then** no snapshot
   is ever taken and the mission starts at slot selection.
2. **Given** session saves on and an interval set in the server configuration,
   **When** the game phase runs, **Then** a snapshot is taken at that interval, each
   one logged with its time and duration, and none is taken during slot selection,
   briefing or debriefing.
3. **Given** snapshots exist, **When** the server restarts without the resume
   parameter, **Then** the mission starts at slot selection.
4. **Given** snapshots exist, **When** the server restarts with the resume parameter
   and no snapshot named, **Then** it continues from the newest one.
5. **Given** snapshots exist, **When** the server restarts with an older snapshot
   named, **Then** it continues from that one, and later snapshots continue from the
   new branch of play.
6. **Given** the mission reaches debriefing, **When** the game ends, **Then** the
   snapshots of that game are discarded, so a later "newest snapshot" never means a
   finished game.
7. **Given** the server is stopped on purpose during the game phase, **When** it is
   started again with the resume parameter, **Then** it resumes from a snapshot taken
   at the stop, not from the last scheduled one.
8. **Given** session saves on but the mission's save types unticked or its systems
   config unset, **When** the game phase starts, **Then** the server log says plainly
   that no snapshot can be taken and why.

---

### User Story 4 - The save is not felt by the players (Priority: P2)

A snapshot during a 127-player game is not something players notice. Nobody is
kicked by it, nobody rubber-bands, voice does not cut out, and the server log records
how long it took so the cadence can be tuned.

**Why this priority**: the addon exists because the previous lobbies flooded the
server at scale. A save that stalls the server for seconds would reintroduce the
kicks the addon was written to remove.

**Independent Test**: on a dedicated server at full player count, let snapshots run at
the server's cadence for an hour and read the per-save duration from the log and the
disconnect reasons from the server log.

**Acceptance Scenarios**:

1. **Given** a full server in the game phase, **When** a snapshot is taken, **Then**
   no player is disconnected because of it and players see no pause in movement, fire
   or voice.
2. **Given** a snapshot in progress, **When** a player disconnects, dies or enters a
   vehicle during it, **Then** the snapshot completes and the world state it captured
   is internally consistent.
3. **Given** a snapshot, **When** it is written, **Then** the statistics it carries
   describe the same moment as the world it carries.

---

### Edge Cases

Motion and vehicles:

- **Aircraft in the air at the snapshot.** The game restores it at its saved position
  with no motion. It is held motionless there for the whole hold and released with
  everything else, so its pilot can be back in the seat before it moves again. The
  release puts the aircraft into the game's own pilot-dropped state (autohover) so it
  holds altitude until the pilot takes the controls. The pin is a spike whose exit
  criteria include: the pin survives the hand-over of the aircraft to the pilot's
  machine and back, a release, a second hold, a helicopter with a passenger and no
  pilot, and one with a damaged engine. If the engine offers no way to hold an
  aircraft, the operator decides the fallback; the plan does not pick one.
- **Ground vehicles moving at the snapshot** come back standing still at the saved
  position; the loss of momentum is accepted.
- **A vehicle whose driver has not returned** stays parked with its engine off; a crew
  member who is back sits in their seat inside the hold like everyone else.
- **Tracked vehicles** have no persistent brake in the game; they are stopped and
  their engines refused, and a tracked vehicle on a slope may creep during the hold.
  Accepted; the release logs any vehicle that moved more than a metre.
- **A character saved in a seat whose vehicle is not there on resume** is expected to
  be placed on the ground at the vehicle's last position rather than lost. This is
  the engine's behaviour, **unverified** from script; the quickstart checks it and
  the outcome is recorded.
- **Towed, slung or roped loads** come back detached at their own saved positions; the
  connection is not restored.

The world:

- **Fences, walls, trees, tyres, glass, lamp posts and other small destructible
  objects** come back intact; the game keeps no record of them. Buildings and vehicles
  keep their damage; craters, fires, wrecks and debris do not come back.
- **A dead body** comes back dead, at its saved position, but the game does not keep
  the pose it fell in. Bodies never decay in a lobby mission today and still do not.
- **Game Master changes** made before the snapshot (placed, moved or deleted entities)
  come back; changes after it are lost like everything else. A multi-step edit in
  progress at the snapshot (a composition half placed) is captured as far as it got;
  the Game Master finishes or undoes it after the resume. Accepted.
- **Fires, projectiles and other transient effects** are not saved and do not exist
  after the load; nothing burns or flies during the hold.
- **Third-party vehicles and weapons** from other addons are saved as long as they are
  built on the game's own base prefabs; anything else is absent after the resume and
  is logged.
- **Time of day and weather** continue from the snapshot, not from the mission's start
  settings, and stand still during the hold.
- **Mission-maker markers, description entities and trackers** are placed in the
  mission and need no record; trackers pick their restored targets up again.
- **Freeze zones** are placed in the mission and come back with the world, but their
  enforcement state does not: the resume reconciles them against the restored freeze
  time. With freeze time left they confine the same players again; with none left
  they are unregistered and removed exactly as the end of a freeze does today, so a
  zone can never kill after the resume. The zone's return countdown pauses on the
  client while the hold is active. A player restored outside their zone is enforced
  from their first entry into it, as any player who has not yet entered is today;
  the countdown that was running at the snapshot is not restored.
- **Player-placed markers restored from a snapshot belong to the server**, not to the
  player who placed them: the faction gate and admin removal still apply, but the
  original placer can no longer delete their own pre-crash marker. Accepted.

Squads and slots:

- **Squads placed in the mission** do not spawn their members again while the snapshot
  is being applied; the restored members are re-attached to their squads by the
  resume itself, because the game re-attaches AI members only and a body a player
  controlled at the snapshot has no saved squad. A squad a Game Master places after
  the resume spawns normally.
- **Squads created by script rather than placed in the world** are outside the
  guard; a mission that spawns squads from script must do so from the game start
  hook the lobby exposes, which does not run on a resume. Stated in the guide.
- **A squad every member of which died and whose bodies were removed** before the
  snapshot has no slots left in the roster, today and after a resume; the squad
  comes back with no slots. A KIA slot whose corpse is still in the world comes
  back KIA with the corpse.
- **A registered body the game does not track** (a prefab without the persistence
  component) is written as a slot with no body; the save logs it and a resume from
  that snapshot refuses with the slot's name. The game mode logs such bodies at game
  start so the mission is fixed before an event.
- **Squad radio nets** assigned at game start are part of the snapshot, per squad,
  and are put back on the squads before anyone is possessed, so a squad keeps the
  frequency written in its briefing even when an earlier squad has disappeared.
- **Slot and squad locks** are part of the snapshot and come back as they were. A
  lock never keeps the slot's own saved holder out.
- **Squad spawn markers** exist only until the freeze ends; a resume with freeze time
  left shows them at the squads' positions at the snapshot.
- **A body mid-replacement at the snapshot** (a Game Master revive or a fresh-body
  swap in flight) must not leave a stray body in the snapshot: saving is disallowed
  before the replacement's first change and re-allowed after its last, on every exit
  path, and a replacement requested while a save is in flight waits for that save to
  finish.
- **A player whose slot was KIA** is given their slot back without being put into
  the corpse: the reservation transfers to them, the slot stays KIA, and they are a
  spectator. This is a transfer of ownership, distinct from taking a free slot.
- **Two snapshots with the same absent holder** carry the same reservation twice:
  a placeholder stands in for the absent holder and its identity key is saved with
  the slot again, so a second crash before they return loses nothing.
- **A snapshot older than the newest is chosen.** The world is that older moment.
  Players who slotted after it have no slot and join as mid-game joiners. Snapshots
  taken after the resume are newer than every older one, so "newest" thereafter means
  the new branch.

Players:

- **Players who were spectating without a slot** at the snapshot come back as
  spectators.
- **Players who never return** keep their body in the world and their slot, as after
  an ordinary disconnect; the game's own one-minute deletion of unclaimed characters
  after a load MUST NOT apply. An admin frees such a slot with the existing kick.
- **A new player** joining a resumed game is treated as a mid-game joiner: spectator,
  or into a free slot if the admin allows it, as today.
- **Admin identity** is checked again on reconnect; being an admin before the crash
  grants nothing after it. The website registration gate, where enabled, re-checks
  each reconnecting player as it does today.
- **Voice rooms** are rebuilt from the slots as players reconnect, as on any
  mid-game reconnect.
- **The hold with no admin back** stays held. This is the operator's choice; an
  event needs more than one admin.
- **Protection before the hold is visible.** Restored entities exist from the load,
  before the lobby has matched slots; the damage gate and the input lock are on from
  the first moment the game mode knows a snapshot is active, and the vehicle stop
  runs as soon as the world reports the load complete. Whether the engine moves
  anything between the load and that moment is **unverified**; the quickstart
  compares saved and held positions.

Saving and starting:

- **A snapshot during slot selection or briefing** is not taken; a scheduled save
  that falls there waits and lands at the start of the game phase. A crash before the
  game phase restarts the mission from slot selection. Slotting itself is not saved.
- **A snapshot during debriefing** is not taken; reaching debriefing discards the
  game's snapshots.
- **The hard freeze** at game start, if still running at the snapshot, resumes with
  its remaining time after the admin releases the resume hold, as an ordinary timed
  hard freeze; the freeze zones exist again and confine the same players; when
  that timed freeze ends, the ordinary freeze countdown runs on as it would have.
  A trigger whose countdown had not started at the snapshot (a mission-end
  countdown starts when the freeze ends, whether or not its wait was queued) starts
  normally afterwards with its configured values.
- **An empty resumed server** waiting for its first player, or a held server whose
  last player leaves, keeps the mission clock exactly where the hold found it: the
  hold writes the held value back rather than counting down, so nothing can run
  backwards while the game itself is not advancing time.
- **A mission with a finite game duration** set in its game-mode state component
  ends when the clock reaches it, as today; a restored clock already past it ends
  the mission on the first frame after the resume. Lobby missions do not set one;
  the guide says so.
- **A snapshot taken during a hold** (an ordinary hard freeze or a resume hold)
  saves the mission clock, which stood still, the freeze's current remainder (or
  the remainder a resume hold was protecting) and the daylight setting from before
  the hold, so no wait is ever counted as play and daylight is never left switched
  off by a resume.
- **A snapshot and a debriefing or a planned stop at the same moment**: the discard
  and the shutdown save wait for a save in flight to finish; a save is never cut
  short by the addon.
- **The server crashes during a snapshot.** The previous complete snapshot is the
  one resumed. That the engine rejects the half-written one is **unverified** from
  script; the quickstart kills the server inside a save and checks which one loads.
- **A planned stop.** The game's own shutdown save runs before the process exits; the
  container's stop grace period must be long enough for it, and this is verified,
  not assumed.
- **Two servers, same mission.** Snapshots belong to the server profile that wrote
  them; a different server starting the same mission starts fresh.
- **Mission or addon updated between the crash and the restart.** If the saved world
  no longer matches the mission or the addon versions, the game is expected to refuse
  the snapshot; the lobby's own record carries a version and refuses an unknown one.
  What the engine refuses is **unverified**; the quickstart changes the addon between
  a save and a resume and records the outcome.
- **A refused resume** never purges anything, never saves and never plays on a
  half-restored world: the server disables saving, logs the cause and shuts down.
  The snapshot list before and after a refusal is identical. A container that
  restarts the process starts a fresh game, since the launcher passes the resume
  parameter once.
- **The resume parameter with no snapshot to load** is a fresh start.
- **The resume parameter left in the container command** turns every later restart
  into a resume attempt; the incident procedure removes it after the one start.
- **Save types unticked or systems config unset** with the checkbox on: no snapshot
  can be taken; the log says so at game start so the mission maker can fix the
  mission.
- **Retention.** Ten snapshots at ten minutes is a hundred minutes of rollback; a
  longer event that wants deeper rollback raises the retention in the server
  configuration, and the statistics files follow that retention automatically.
- **The statistics file of a snapshot** is written before the world is saved and
  named in the snapshot; if the file cannot be written, the snapshot names none and
  a resume from it says so. If the world save fails after the file was written, the
  orphan file is pruned with the rest. Two snapshots in the same second get distinct
  names.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The game mode MUST offer a mission-maker setting that turns session
  saves on or off for the mission, off by default. The save cadence and retention are
  the game's own settings from the server configuration; the addon MUST NOT add a
  second clock or a second retention rule.
- **FR-002**: With saves on, automatic snapshots MUST be allowed only while the
  mission is in the game phase and MUST NOT be taken in any other phase; a scheduled
  save that falls in another phase waits for the game phase. No snapshot MUST be
  taken while a body replacement is pending: saving is disallowed synchronously
  before the replacement's first change and re-allowed after its last, and a
  replacement requested during a save waits for the save. The first save of a
  game phase, fresh or resumed, is allowed only once no replacement is pending.
- **FR-003**: A snapshot MUST capture the state the game itself can save for every
  tracked entity in the world: position, identity, hit-zone damage, inventory and
  equipment, seating, fuel, lights, magazines and attachments, armed mines, doors,
  ruined buildings, items on the ground, AI groups and their waypoints, entities
  placed by a Game Master, and time and weather.
- **FR-004**: A snapshot MUST additionally capture the addon's own game state: the
  slot table with each slot's body, squad, occupant identity, KIA status and lock,
  each squad's radio frequency, the mission clock (which stands still during any
  hard freeze, so it needs no correction), the remaining freeze time, the timed
  hard freeze's live remainder or the remainder a resume hold protects, the
  daylight setting from before any hold, the mission-end timer's duration and
  start reading, the started, fired and progress state of every trigger (remaining
  countdown, capture and contest progress and holder), player-placed map markers
  with their faction, the
  reservation of every slot whose player is disconnected (including one already
  standing in for an absent holder), and the unit statistics as of that same
  moment, including what the statistics recorder needs to continue: its session,
  its clock, first-slot holders by identity, and its one-time decisions.
- **FR-005**: A plain restart MUST be a fresh start regardless of existing snapshots.
  When the server is started with the game's own resume parameter, it MUST continue
  from the newest snapshot, or from the named one, in the game phase, without passing
  through slot selection or briefing, and MUST NOT spawn fresh bodies or restart the
  freeze. Before any slot registers, every restored body MUST be re-attached to the
  squad named in its slot record.
- **FR-006**: A reconnecting player whose identity held a slot at the snapshot MUST be
  given that slot back, whether they connect before, during or after the resume
  finalises: an alive slot puts them into its character through the existing
  identity-based reconnect rule; a KIA slot transfers to them without possession and
  they come back as a spectator with the slot still marked KIA; a locked slot is
  still theirs. The reservation of an absent holder MUST survive any number of
  further snapshots.
- **FR-007**: A resumed world MUST start in a hold, active from the moment the game
  mode knows a snapshot is loaded, in which no connected player can move, fire,
  enter or leave a vehicle or place markers, no character or vehicle can take damage,
  no engine can be started, every ground vehicle is stopped and braked where the game
  offers a brake, airborne aircraft are held in place, and no trigger, zone timer,
  mission timer, game clock or daylight advances, until an admin releases it with the
  existing hard-freeze release. Mission AI movement and slope creep of unbraked
  vehicles are accepted limits. There is no automatic release. The hold and its
  release MUST be announced to every connected player; the hold MUST show the
  snapshot's time and how many players are back. Time spent in a hold MUST NOT be
  counted by any clock, in play or in a later snapshot.
- **FR-008**: The addon MUST NOT delete a saved player character because its owner has
  not reconnected within any timeout; a slot stays held as it does after an ordinary
  disconnect during the game.
- **FR-009**: The feature MUST add no admin chat command. Releasing the hold, ending a
  game and freeing a slot use the controls that exist today.
- **FR-010**: Reaching debriefing MUST discard the game's snapshots so a later "newest
  snapshot" never resumes a finished game.
- **FR-011**: A deliberate server stop during the game phase MUST leave exactly one
  snapshot taken at the stop, through the game's own shutdown save or the addon's
  stop-time request, whichever completes first, never both; the feature MUST
  verify this on a dedicated server and document the stop grace period it needs.
- **FR-012**: A snapshot MUST never disconnect a player and MUST NOT stall the server
  long enough for players to notice; every snapshot MUST be logged with its time, type
  and duration.
- **FR-013**: A snapshot that did not complete MUST never be resumed from; the previous
  complete one is used. The engine's part of this MUST be verified on a dedicated
  server, not assumed.
- **FR-014**: A snapshot that no longer matches the mission or addon versions, or
  whose lobby record is missing, of an unknown version, names a body or squad that
  did not come back, or holds a slot whose body was untracked at save time, MUST be
  refused with a logged reason. A refusal MUST disable saving before anything else,
  MUST keep every snapshot and MUST NOT play on: the server shuts down so the
  operator restarts plainly or names an older snapshot.
- **FR-015**: Saving and resuming MUST add no remote call or join-in-progress
  payload, no replicated collection, and exactly three replicated scalars, each set
  once on the server: the snapshot's time for the hold text, the mission-end
  timer's duration, and the clock reading at which it started. The mission clock
  reuses the game's own replicated elapsed time. All save and load work is
  server-side, and a resume reaches clients only through the existing reconnect
  flow.
- **FR-016**: A mission with the setting off MUST behave as before this feature,
  with exactly two exceptions that apply to every mission: time stands still
  during every hard freeze (FR-022), and spectators see the mission-end countdown
  (FR-023). No snapshots, no files written, no resume, no log lines about saving.
- **FR-017**: A resume MUST NOT create a second copy of any squad member or vehicle:
  the restored world is the only world, and no squad placed in the mission spawns its
  members again on top of the restored ones. The slot count after a resume equals the
  slot count at the snapshot, and every slot belongs to the squad it belonged to.
- **FR-020**: A statistics file MUST be kept as long as the engine still lists a
  snapshot at least as old as it, and pruned only after a successful listing and
  never while a write is in flight; the addon MUST NOT apply a count of its own. A
  missing or unreadable statistics file MUST NOT refuse the resume; it MUST be
  logged and reported at the next publish, and that report MUST survive a further
  snapshot and resume.
- **FR-021**: Every squad in a mission with session saves MUST have a unique entity
  name; the game mode MUST log any squad without one, and any registered body the
  game does not track, when the roster is complete at the start of the game phase,
  before the first save is allowed.
- **FR-022**: While any hard freeze is active, the mission clock, the freeze
  countdown, daylight and every trigger MUST stand still on every machine, saves
  on or off; the clock MUST NOT run backwards on a server with no players; the
  freeze countdown MUST resume exactly once when the last hard freeze ends.
- **FR-023**: In a mission with one mission-end timer of positive duration, the
  spectator clock MUST show the time until the mission ends, derived on each
  client from the replicated clock and two once-set values, standing still
  whenever the clock does; the timer itself MUST fire from the same clock; a
  mission without that timer MUST keep the elapsed clock; more than one timer or
  a zero duration MUST be logged at the roster check; alive players see nothing
  new.
- **FR-018**: With the setting on but the mission unable to save (save types
  unticked, systems config unset), the server log MUST state the cause at game start.
- **FR-019**: The mission-making guide's instruction to untick the save types MUST be
  replaced by the instruction to leave them and to set the systems config, in both
  languages, as part of this feature.

### Key Entities

- **Snapshot**: one complete save point of the world, the addon's game state and the
  statistics, taken at a moment in the game phase on the server's schedule or at a
  planned stop; has a time and a duration; the last ten are kept.
- **Slot record**: what the addon writes per slot into a snapshot: the slot's
  character, its occupant's identity, KIA status, lock, and the squad it belongs to,
  by the squad's entity name.
- **Squad record**: per squad, its entity name and radio frequency.
- **Resume hold**: the protected period after a resume, from the first moment the
  game mode knows a snapshot is loaded until an admin releases it.
- **Refusal**: the outcome of a snapshot that cannot be resumed as a whole: logged,
  nothing purged, server shut down.
- **Mission-end countdown**: the time until the mission-end timer fires, shown to
  spectators; duration and start reading set once on the server, remaining time
  computed on each client from the held clock.
- **Resume choice**: made outside the game by whoever restarts the server: none (fresh
  start), newest snapshot, or a named snapshot.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After a forced server kill on a dedicated server with 127 slotted players
  in the game phase and a restart with the resume parameter, every player who
  reconnects within the hold is back in their own character within 1 metre of its
  saved position (for a vehicle occupant, of the vehicle's position at release, so
  the accepted creep is excluded), with the same health, inventory and seat, the
  mission is in the game phase with the same slot occupants, and the slot count
  equals the snapshot's; verified on one live event and in the demo world.
- **SC-002**: With saving allowed and each save succeeding, the play lost by a crash
  is never more than one server-configured save interval, and with the game's default
  interval no more than ten minutes; a save deferred by a phase or a replacement, or
  a failed save, extends this by exactly its own delay, which the log shows.
- **SC-003**: During a one-hour game at 127 players with saves at the game's default
  interval, zero players are disconnected by a snapshot and every snapshot's logged
  duration stays under one second.
- **SC-004**: In the demo world, a building destroyed, a vehicle damaged and refuelled,
  a weapon dropped, a mine armed, a door opened, a trigger fired and a kill recorded
  before the snapshot are all in the same state after the resume, and nothing done
  after the snapshot is, in one side-by-side check.
- **SC-005**: With the setting off, a server restart behaves exactly as before the
  feature and no save-related file or log line exists.
- **SC-006**: Network traffic per client during the game and at reconnect is not
  higher than before the feature beyond the three once-set replicated scalars,
  measured on a dedicated server against a baseline taken before the feature with
  the same player count.
- **SC-008**: In the demo world, a resume from a snapshot with a missing statistics
  file, a resume from a snapshot taken during a hold, a server killed inside a save,
  a resume after an addon change, and two consecutive resumes with one holder absent
  throughout each end in the state this spec names for them, one check each.
- **SC-009**: In the demo world with saves off, a one-minute hard freeze followed by
  a ten-minute freeze ends eleven minutes after the game started, the spectator
  countdown reads the same value at both ends of a five-minute admin hard freeze,
  and the value matches on two spectator screens within a second.
- **SC-007**: A plain restart of a mission with snapshots comes up at slot selection;
  a restart with the resume parameter comes up held in the game phase; one check each.

## Assumptions

- The mission maker opts in per mission; the addon ships with saves off because a
  snapshot changes what the server keeps on disk, which a server owner must choose.
- The game's server configuration schedules the snapshots (ten minutes by default),
  keeps ten, and is set not to load a snapshot on its own; the resume parameter of the
  game's own launch line names the snapshot to continue from. The addon decides only
  in which phase saving is allowed.
- Only the game phase is saved. Slot selection and briefing are short enough to redo,
  and saving them would add a second resume path for little value.
- Momentum is not restored for any vehicle; the loss is accepted for ground vehicles,
  and aircraft are held in place until the release.
- Small destructible objects, craters, fires, wrecks and body poses are not restored,
  because the game keeps no record of them; this is stated to players as a known limit
  of a resume, not fixed by this feature.
- The resume reuses the addon's existing identity-based reconnect and body
  re-possession flow for alive slots, and the existing hard-freeze mechanics for the
  hold, rather than introducing a second reconnect or a second freeze. KIA and locked
  slots need a reservation transfer the existing flow does not have. The hard freeze
  today locks characters on foot only; holding vehicles and aircraft is new ground
  for the plan, and the game offers no AI or physics pause from script.
- The game saves squad membership for AI members only; the lobby saves the squad per
  slot and re-attaches bodies itself. Squads are found by entity name, which this
  feature requires of every squad (FR-021); the roster label is a callsign and is
  not used for that.
- The mission clock is the game's own elapsed time, held on every machine while any
  hard freeze is active by writing the value from the freeze's start back over it;
  nothing about the clock is added to the wire. The spectator countdown is derived
  from it plus two once-set scalars.
- The Game Master's own placed entities are restored by the game; the addon only makes
  sure its configuration includes them.
- Snapshots belong to the server profile that wrote them; sharing them between
  machines is out of scope.
