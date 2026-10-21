# Feature Specification: Session Resume After a Server Crash

**Feature Branch**: `004-session-resume`

**Created**: 2026-09-14

**Status**: Draft

**Input**: User description: "Design a persistence system based on the Conflict persistence system. Focus on the state of the world (broken things, destroyed buildings), positions of characters, moving helicopters, vehicles, etc. Find the other edge cases too."

**Amended**: 2026-10-11, random squad deployment added on the operator's request (User Story 5, FR-024 to FR-035). It shares this branch and its pull request with session resume, and nothing else.

**Amended**: 2026-10-21, timed particle effects and the timed hiding of a map zone added on the operator's request (User Stories 6 and 7, FR-036 to FR-046). They share this branch with session resume and random deployment, and nothing else.

**Amended**: 2026-10-21, random deployment extended on the operator's request: a line drop, the air drop of bodies without a player, and an arrow on the spawn point (User Story 5, FR-028 revised, FR-047 to FR-049).

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

Random squad deployment, added by the amendment of 2026-10-11, starts from a different
gap. A squad begins the game exactly where the mission maker placed it, and each side
sees its own squads' start positions on the briefing map. A mission built on not
knowing where someone is (a downed aircrew to be found, a patrol scattered in the
night, a squad dropped from the air) has no uncertainty unless somebody moves those
players by hand once the game runs. The addon already owns the two moments a
deployment can hang on: the start of the game phase, when every slotted player is
given a fresh body where the placed one stood, and the end of freeze time. The game
has no parachute; a mission that drops a squad from the air equips it with one of its
own choosing.

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
- Q: What if the snapshot carries no statistics? → A: the world still resumes; the
  recorder starts a new recording and the log says so. World recovery outranks
  statistics. (Amended 2026-09-19 at implementation: the recorder's state travels
  inside the snapshot's own lobby record, so it can be neither newer nor older than
  the world and no separate statistics file exists.)
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
- Q: How are statistics files kept and pruned? → A: superseded 2026-09-19: there is
  no per-snapshot statistics file; the recorder's state is part of the snapshot and
  follows the engine's own retention.
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
- Q: How are statistics files pruned? → A: superseded 2026-09-19: nothing to prune,
  the statistics live inside the snapshot.
- Q: How does the statistics recorder handle absent holders? → A: it resolves
  every identity through the lobby's reconnect keys, placeholders included, so a
  holder absent through the publish keeps their credit; on a resume it continues
  from the state embedded in the snapshot instead of starting a new recording.
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
  timer drives the display in that case, with the log line saying so. The
  timed-message trigger inherits the timer's countdown but is an announcement, not
  a mission end: any number of them is supported, none is counted here, and each
  keeps its own start reading in the trigger record (real missions carry one
  mission-end timer and several timed announcements, 2026-09-19 review).
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

### Session 2026-10-11 (amendment: random squad deployment)

- Q: Where is random squad deployment specified? → A: in this specification, as an
  amendment; it is delivered on this branch and in this pull request.
- Q: How is a squad's random position chosen? → A: from spawn points the mission
  maker places and links to the squad; the server draws one at random. A random
  position inside an area is not part of the feature.
- Q: Which bodies of the squad go to the drawn point? → A: the whole squad, spread
  around the point rather than stacked on it.
- Q: How does a squad dropped from the air survive? → A: the mission equips it with
  a parachute that opens by itself when a body starts in mid-air. The addon only
  sets the height.
- Q: Does a squad with random deployment keep its squad spawn marker? → A: a squad
  that deploys at the end of freeze has no spawn marker at all (revised the same
  day; the first answer kept it on the placed position). A squad that deploys at
  the start of freeze keeps its marker like any other squad.
- Q: What happens to a member sitting in a vehicle at an end-of-freeze deployment?
  → A: taken out of the seat and deployed with the squad; the vehicle stays.
- Q: Does the mission-maker guide change? → A: not in this change.
- Q: Does the addon give the mission maker a spawn point to place? → A: yes, a
  prefab of its own, as it does for the flags of a zone trigger.

### Session 2026-10-21 (amendment: timed particle effects)

- Q: Where is the timed effect specified and delivered? → A: on this branch, in this
  specification.
- Q: From what moment does an effect's time count? → A: a setting per effect: the end
  of freeze time or the start of the game phase.
- Q: What does a player see who joins after an effect has started? → A: what is left
  of its time, with no value added to the network for it.
- Q: Does the addon ship a particle to time? → A: one, a red signal smoke that plays
  continuously.
- Q: How does a map zone hide some time after the start? → A: a delay setting on the
  zone; no new component.
- Q: From when does that delay count? → A: from the end of freeze time, whether or
  not the mission has a mission-end timer; timed effects count from the same moment.
- Q: What gets the delay? → A: map zones only.
- Q: Does the delay need the zone's existing hide setting ticked? → A: no; a delay
  above zero hides the zone on its own, and the existing setting keeps hiding it the
  moment freeze ends.

### Session 2026-10-21 (amendment: random deployment, line and air)

- Q: Do bodies without a player come down from the height too? → A: yes; the
  mission's parachute opens for them as well, as the operator saw in a test.
- Q: Can a squad drop in a line instead of around the point? → A: yes, a setting per
  deployment: one body after another from the drawn point along the point's arrow,
  each later one a little higher, as a stick leaves an aircraft.
- Q: How does a mission maker see where the line runs? → A: the addon's spawn point
  shows its arrow in the World Editor.

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

### User Story 5 - A squad starts where nobody can predict (Priority: P3)

A mission maker builds a search mission: an aircrew has gone down somewhere in a
wide area, one side must find and extract it, the other must get there first, and a
squad of paratroopers is dropped over the area to help. The maker places a handful
of spawn points for the aircrew and another handful for the paratroopers, links each
set to its squad, and sets when the squad goes there (as freeze time starts or as it
ends) and whether it arrives on the ground or in the air at a given height. In every
game the server draws one point per squad. The aircrew spends freeze time at its
placed position and, the moment freeze ends, stands scattered around a point neither
side could have known. The paratroopers appear in the air above theirs and come down
under their own parachutes.

**Why this priority**: it replaces moving players by hand during a running game,
which is the only way to run such a mission today. It is independent of session
resume and blocks nothing else.

**Independent Test**: in the demo world, link three spawn points to a named squad,
start the game several times and read where the squad stands after the configured
moment and which point the server log names.

**Acceptance Scenarios**:

1. **Given** a squad linked to several spawn points and set to deploy at the end of
   freeze, **When** freeze time ends, **Then** every living body of the squad is
   within the configured dispersion radius of one of those points, the same point
   for the whole squad, standing on the ground with no two bodies overlapping, and
   until that moment the squad was at its placed position under the normal freeze
   rules.
2. **Given** a squad set to deploy at the start of freeze, **When** the game phase
   begins, **Then** its players control bodies that are already at the drawn point,
   and no freeze zone around the squad's placed position treats them as having left
   it.
3. **Given** a squad set to deploy in the air at a height, **When** the deployment
   happens, **Then** every body of the squad, with or without a connected player, is
   at that height above the ground over its own position, holding nothing in its
   hands.
4. **Given** the same mission started ten times, **When** the drawn points are
   compared, **Then** the squad did not arrive at the same point every time, and the
   server log names the squad, the point and the number of bodies deployed each
   time.
5. **Given** a deployed squad, **When** a player reconnects into one of its slots or
   takes a free one, **Then** they are in that slot's body where it now stands, not
   at the squad's placed position.
6. **Given** an end-of-freeze deployment and a member sitting in a vehicle, **When**
   freeze ends, **Then** the member is out of the seat and with the squad, and the
   vehicle is where it was.
7. **Given** a mission with no spawn points, **When** it is played, **Then** nothing
   about it differs from before the amendment.
8. **Given** a squad with an end-of-freeze deployment, **When** any player or
   spectator opens the briefing map or the game map, **Then** that squad has no
   spawn marker, while the other squads of its side have theirs.
9. **Given** a deployment set to drop in a line, **When** it happens, **Then** the
   bodies are one after another from the drawn point along the point's arrow, about
   the set spacing apart, each later one higher by the set rise, and the radius plays
   no part.

---

### User Story 6 - A signal burns while it matters (Priority: P3)

A mission maker drops a squad of paratroopers over open country and leaves their
supply cache in tall grass. To let them find it, the maker places red signal smoke at
the cache and sets it to burn for fifteen minutes from the end of freeze. Nobody sees
the smoke during slot selection, briefing or freeze, so the other side learns nothing
early. The moment freeze ends and the squad is in the air, a red column stands over
the cache; a quarter of an hour later it thins out and is gone.

**Why this priority**: it replaces smoke that either burns out within two minutes or
burns for the whole game from the moment the world loads. It is independent of
session resume and of random deployment.

**Independent Test**: in the demo world, place the addon's signal smoke with the
timing component set to thirty seconds from the end of freeze, start a game with a
one-minute freeze and watch it; join a second client during the thirty seconds and a
third after them.

**Acceptance Scenarios**:

1. **Given** an effect set to start at the end of freeze, **When** the world loads and
   through slot selection, briefing and freeze time, **Then** it shows nothing on any
   machine.
2. **Given** the same effect, **When** freeze time ends, by its countdown or by an
   admin, **Then** it starts on every connected machine within a second.
3. **Given** an effect set to start with the game phase, **When** the game phase
   begins, **Then** it starts, during freeze time or without one.
4. **Given** a burning effect, **When** its time has passed on the mission clock,
   **Then** it stops emitting and the smoke already in the air thins out instead of
   vanishing.
5. **Given** a burning effect, **When** a player joins or reconnects, **Then** they see
   it for what is left of its time; a player joining after its time has passed never
   sees it.
6. **Given** a mission without a timed effect, **When** it is played, **Then** nothing
   about it differs from before the amendment.

---

### User Story 7 - The assembly area leaves the map (Priority: P3)

A mission maker draws the area where one side assembles. Both sides should see it
while they plan and through the first quarter of an hour of the fight, and then no
more, because by then that side has moved on and the drawing only clutters the map.
The maker sets the zone's hide delay to fifteen minutes. The zone is on the map through briefing, freeze and the
first fifteen minutes after it, then it is gone for everyone.

**Why this priority**: it gives every mission a setting that today only a mission's
own script provides. It is independent of the other stories.

**Independent Test**: in the demo world, give a map zone a hide delay of thirty
seconds, start a game with a one-minute freeze and keep the map open;
join a second client during the thirty seconds and a third after them.

**Acceptance Scenarios**:

1. **Given** a zone with a delay of thirty seconds, **When** any player looks at the
   map during briefing, during freeze or within thirty seconds after the freeze
   ended, **Then** the zone is drawn; afterwards it is drawn on no map, including one
   that stayed open.
2. **Given** a zone with the existing setting that hides it when freeze ends,
   **When** freeze ends, **Then** the zone hides at that moment, exactly as before the
   amendment, whatever its delay.
3. **Given** a mission with a mission-end timer, **When** a player joins after the
   freeze ended, **Then** the zone hides for them at the same moment as for everyone
   else; in a mission without one, at the moment counted from the configured freeze
   length.
4. **Given** a zone with neither the hide setting nor a delay, **When** the mission
   is played,
   **Then** nothing about it differs from before the amendment.

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
  configuration; the statistics inside each snapshot follow it.
- **The statistics of a snapshot** are part of its lobby record, read in the same
  save as the rest of it, so they never describe a later moment than the world and
  no file can go missing.

Random deployment:

- **Freeze ended early, extended or revived.** An end-of-freeze deployment happens
  when the freeze actually ends, by its countdown or by an admin, and once per game:
  a freeze an admin revives after it ended deploys nobody when it ends again.
- **A mission with no freeze time** deploys its end-of-freeze squads at the start of
  the game phase.
- **Freeze ended by an admin during a hard freeze.** The squads deploy at that
  moment; controls stay held and nothing takes damage until the hard freeze ends.
- **An air drop while freeze protection is on** (a start-of-freeze deployment in a
  mission that forbids damage during freeze) is protected from the fall like from
  any other damage during freeze; the feature adds no protection of its own. Every
  other air drop relies on the mission's parachute.
- **A member with no parachute in an air drop** falls and dies once damage is on.
  The addon does not check equipment.
- **A member who died before the deployment** is not moved; the slot stays KIA.
- **Bodies without a player** (a free slot, or a slot held for a disconnected
  player) go with the squad and arrive as the others do, in an air drop at the
  height, under the mission's parachute.
- **A line that runs over water or obstacles** is the mission maker's choice, as a
  point is: the line only follows the arrow and the terrain height.
- **A player who takes a free slot of the squad during freeze**, before an
  end-of-freeze deployment, is deployed with the squad.
- **A squad whose free slots are removed at game start** deploys the bodies that
  remain.
- **A spawn point linked to a squad that does not exist, or a squad set to deploy
  with no spawn point**, is logged at the start of the game phase; the squad stays
  where it was placed.
- **A spawn point on water, a roof, a slope or among trees** is the mission maker's
  choice. The addon keeps the bodies apart and puts ground arrivals on the terrain
  surface; it does not judge the place.
- **The squad spawn marker.** A squad that deploys at the end of freeze has none, on
  the briefing map or the game map, for anyone, and its name in the roster leads to
  no position. A squad that deploys at the start of freeze has one like any squad:
  its placed position on the briefing map, and where it stands when the game phase
  begins on the game map.
- **Live map markers a mission enables** (squad-leader markers, trackers) keep
  working as configured; whether a deployed squad is visible to its own side on the
  map is the mission's setting, not this feature's.
- **A resume from a snapshot taken after a deployment** restores every body where
  the snapshot has it and deploys nobody again.
- **A resume from a snapshot taken during freeze, before an end-of-freeze
  deployment**, deploys the squad once, when the freeze ends after the release. The
  point is drawn then; nothing about a pending draw is saved.

Timed particle effects:

- **Freeze changed by an admin.** An effect that starts at the end of freeze starts
  when the freeze actually ends. Once started it burns its full time; a freeze an
  admin revives afterwards does not put it out.
- **A late joiner after an admin changed the freeze**, in a mission without a
  mission-end timer, measures the effect from the configured freeze length, because
  nothing else about the end of freeze is on the wire; its remaining time is off by
  the admin's change. A mission-end timer's start is exact for everyone.
- **A mission with no freeze time**: an effect set to the end of freeze starts with
  the game phase.
- **Hard freeze**: the mission clock stands still, so a burning effect keeps burning
  and its remaining time does not run down.
- **A resume**: the resumed game's clock continues, and players coming back see what
  is left of an effect's time as any late joiner does.
- **The end of the game phase**: a burning effect stops when debriefing begins.
- **The component on something that is not a particle effect** logs one warning and
  does nothing.
- **A short effect stays short**: the component starts and stops a particle; it does
  not make one that ends on its own, such as a smoke grenade's ninety seconds, last
  longer. The addon's signal smoke plays until it is stopped.
- **A dedicated server** draws no particles; the component does nothing there.

Timed map zones:

- **A map that stays open** loses the zone the moment its delay passes.
- **Freeze changed by an admin.** The delay counts from the first end of freeze in
  the game. A freeze an admin revives afterwards shows the zone again while it runs,
  as the hide setting already does; when it ends, the delay has long passed and the
  zone hides at once.
- **A late joiner** hides the zone at the same moment as everyone else in a mission
  with a mission-end timer; without one, at the moment counted from the configured
  freeze length.
- **A resume** keeps a mission-end timer's start; without one, players coming back
  count from the configured freeze length.
- **Hard freeze**: the mission clock stands still, so the delay does not run down.

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
  game phase, fresh or resumed, is allowed only once no replacement is pending. An admin MAY request a snapshot at any time with the `/snapshot` chat command (a test aid); the request obeys the same gate.
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
- **FR-020**: The statistics recorder's continuation state MUST be part of every
  snapshot's lobby record (amended 2026-09-19: no per-snapshot statistics file, no
  pruning). A snapshot without it MUST NOT refuse the resume; the recorder MUST start
  a new recording and log it.
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
  mission without that timer MUST keep the elapsed clock; timed announcements
  that inherit the timer are not mission-end timers, are unlimited and resume from
  their own saved start reading; more than one timer or
  a zero duration MUST be logged at the roster check; alive players see nothing
  new.
- **FR-018**: With the setting on but the mission unable to save (save types
  unticked, systems config unset), the server log MUST state the cause at game start.
- **FR-019**: The mission-making guide's instruction to untick the save types MUST be
  replaced by the instruction to leave them and to set the systems config, in both
  languages, as part of this feature.

- **FR-024**: A mission maker MUST be able to give a squad random deployment by
  placing spawn points in the world and linking them to the squad, any number of
  points per squad, each point serving one squad. A squad with no spawn point, and
  a mission with none, MUST behave as before the amendment, whatever the
  session-saves setting.
- **FR-025**: Per squad, the mission maker MUST be able to set the moment of the
  deployment (the start of freeze time, which is the start of the game phase, or
  the end of freeze time), the height above the ground at which bodies arrive
  (none means on the ground) and the dispersion radius around the point.
- **FR-026**: At the configured moment the server MUST draw one of the squad's spawn
  points at random, each with the same chance, once per squad and once per game.
  The choice MUST be the server's alone and MUST NOT be known to any client before
  the squad is there.
- **FR-027**: A deployment MUST place every living body of the squad, with or
  without a player, within the dispersion radius of the drawn point, with no two
  bodies overlapping. A body arriving on the ground MUST stand on the terrain
  surface. The squad MUST remain the same squad: its roster rows, callsign, radio
  net, statistics and the triggers that name it are unaffected.
- **FR-028**: With a height set, every body of the squad, with or without a
  connected player, MUST arrive at that height above the ground over its own
  position. The addon MUST NOT provide, open or require a
  parachute and MUST NOT slow the fall.
- **FR-029**: A squad with an end-of-freeze deployment MUST spend freeze time at its
  placed position under the normal freeze rules. A start-of-freeze deployment MUST
  have the squad at the drawn point from the first moment its players control
  bodies, and MUST NOT cause a freeze zone to count any member as having left it.
- **FR-030**: At an end-of-freeze deployment a member seated in a vehicle MUST be
  taken out of the seat and deployed with the squad; the vehicle MUST stay where it
  is.
- **FR-031**: A player who reconnects into, or takes, a slot of a deployed squad
  MUST get that slot's body where it stands.
- **FR-032**: A squad with an end-of-freeze deployment MUST have no squad spawn
  marker on any map, for any player or spectator, from briefing on; the marker MUST
  be left out by the server, not hidden on the client. A squad with a
  start-of-freeze deployment MUST keep its marker as any other squad does.
- **FR-033**: A resumed game MUST NOT deploy a squad that had deployed before the
  snapshot, and MUST deploy a squad whose end-of-freeze deployment was still ahead
  exactly once, when the freeze ends in the resumed game.
- **FR-034**: Random deployment MUST add no replicated property, no replicated
  collection and no join-in-progress payload; FR-015's count of scalars stands. The
  draw and every placement are decided on the server. If moving a player's body
  needs a message to that player's machine, it is one message per deployment to
  that player only, never a broadcast and never repeated.
- **FR-035**: The server log MUST name the squad, the drawn point and the number of
  bodies placed for every deployment, and at the start of the game phase MUST name
  every spawn point whose squad does not exist and every squad set to deploy with
  no spawn point; such a squad MUST stay at its placed position.

- **FR-036**: A mission maker MUST be able to time a particle effect placed in the
  world by adding the addon's timing component to it and setting the moment it
  starts (the end of freeze time, the default, or the start of the game phase) and
  how many seconds it plays.
- **FR-037**: A timed effect MUST show nothing before its start moment, from the
  moment the world loads.
- **FR-038**: At its start moment a timed effect MUST start on every machine that
  draws it, and once its seconds have passed on the mission clock it MUST stop
  emitting and let the particles already emitted finish.
- **FR-039**: A player who joins or reconnects while a timed effect burns MUST see it
  for the rest of its time and never after, with the end of freeze taken from the
  reading of FR-044.
- **FR-040**: A started effect MUST burn its time out; a freeze revived afterwards
  MUST NOT stop it. It MUST stop when the game phase ends.
- **FR-041**: Timed effects MUST add no replicated property, no RPC and no
  join-in-progress payload; each machine decides from the game state, the freeze
  countdown and the mission clock it already receives. A dedicated server MUST do
  nothing for them.
- **FR-042**: The addon MUST ship one particle to time: a red signal smoke, made from
  the game's red smoke grenade, that plays until it is stopped.

- **FR-043**: A map zone MUST take a hide delay in seconds, zero (off) by default. A
  delay above zero MUST, on its own, keep the zone on every map until that many
  seconds after the end of freeze on the mission clock and then hide it for every
  player, also from a map that is open. The existing setting that hides a zone when
  freeze ends MUST keep doing so at once, whatever the delay.
- **FR-044**: Every machine MUST take the end of freeze from one reading that timed
  zones and timed effects share: the mission-end timer's replicated start when the
  mission has one; otherwise the mission clock at the moment this machine saw the
  freeze countdown reach zero; otherwise the configured freeze length. The reading is
  the first end of freeze in a game.
- **FR-045**: The delay and the reading MUST add no replicated property, no RPC and
  no join-in-progress payload.
- **FR-046**: A zone's other settings (the hidden game states, the faction list, the
  hide setting without a delay) MUST behave as before the amendment.

- **FR-047**: A deployment MUST offer a line drop as a setting: the bodies one after
  another from the drawn point along the point's forward axis, at a set mean spacing
  with a set random shift along and across the line, each later body higher by a set
  rise; the dispersion radius is then not used.
- **FR-048**: A body without a player that arrives in the air MUST hold no weapon or
  gadget in its hands, as a player's body does.
- **FR-049**: The addon's spawn point MUST show its forward axis as an arrow in the
  World Editor, and nothing of it may exist in the game.

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

- **Spawn point**: a position the mission maker places in the world and links to one
  squad; a candidate for that squad's deployment.
- **Deployment**: the one-time move of a whole squad to a spawn point drawn by the
  server, at the start or at the end of freeze time, on the ground or at a height,
  spread within a radius or in a line along the point's arrow; its moment, height,
  radius and line are set per squad.

- **Timed effect**: a particle effect placed in the world with the addon's timing
  component; it plays for a set number of seconds from the end of freeze time or from
  the start of the game phase.

- **Freeze-end reading**: the mission clock at the first end of freeze in a game, as
  one machine knows it; shared by timed effects and timed zones.

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

- **SC-010**: In the demo world, over ten starts of a mission whose squad is linked
  to three spawn points, the squad arrives at one of its points every time and at
  more than one point overall; in every start no member is further from the drawn
  point than the dispersion radius, no two members overlap, and the server log names
  the point.
- **SC-011**: In the demo world, a squad set to deploy in the air at the end of
  freeze has every member, with or without a player, at the set height above the
  ground, within five metres, at the moment freeze ends, and every member whose equipment includes
  a self-opening parachute lands alive; a member seated in a vehicle at that moment
  is among them and the vehicle has not moved.
- **SC-012**: In the demo world with session saves on, a resume from a snapshot
  taken after a deployment has every member within one metre of its saved position
  and deploys nobody, and a resume from a snapshot taken during freeze deploys the
  squad once when the freeze ends; one check each.
- **SC-013**: SC-006 holds for a mission with random deployment configured: no
  traffic is added for players outside a deploying squad.
- **SC-014**: In the demo world on a dedicated server, the addon's signal smoke set to
  thirty seconds from the end of freeze shows nothing on two clients during a
  one-minute freeze, appears on both within a second of the freeze ending and stops
  emitting thirty seconds later; a third client joining ten seconds into the burn sees
  it for about twenty seconds, and one joining after the burn sees nothing.
- **SC-015**: SC-006 holds for a mission with timed effects: they add no network
  traffic.
- **SC-016**: In the demo world on a dedicated server, a map zone hidden thirty
  seconds after a one-minute freeze is on the open map of two clients until thirty
  seconds after the freeze ended, then on neither; a client joining after that never
  sees it; with a mission-end timer added, a client joining ten seconds into the
  delay loses the zone at the same moment as the others.

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
- Random deployment draws from placed points only; the mission maker answers for
  each point being a place a squad can stand or land.
- The addon has no parachute and the game has none; an air drop is survivable
  because the mission gives the squad a parachute that opens by itself from a
  mid-air start (operator, 2026-10-11).
- A timed effect is a particle the mission maker places; the addon times it and ships
  one continuous red signal smoke. Other colours, and making the game's own short
  effects continuous, are not part of it (operator, 2026-10-21).
- A spawn point serves one squad; two squads that should arrive together are not a
  case this feature covers.
- Players are not told at the moment of a deployment; the mission's briefing is
  where they learn of it.
- Placing the deployment and its spawn points in the demo world is Workbench work
  for the operator. The deployment prefab and the spawn-point prefab were written
  by the agent on the operator's word of 2026-10-11, an exception for those two
  files.
- The mission-maker guide is not changed by the amendment (operator, 2026-10-11).
