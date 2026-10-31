# Feature Specification: Replay Recording

**Feature Branch**: `005-replay-recording`

**Created**: 2026-10-31

**Status**: Draft

**Input**: User description: "Replay recording: the server records every game into compact, crash-safe files under the server profile — positions of all characters (players and AI) and vehicles, vehicle crew, deaths with weapon and distance, unconsciousness and revival, shots fired, explosions, player map markers with their side visibility (chat is NOT recorded), zone captures and trigger events, freeze and phase changes, and the game statistics already gathered — with zero network replication and without changing any weapon or ammunition prefab. A crash-resumed game continues the same recording as a new segment. A companion service that runs on the game host, kept in this repository, sends recordings and statistics to a configured website over an authenticated, rate-capped, outbound-only protocol documented in this repository; it never opens a listening port and never contacts any third party other than the configured website. Recordings must be publishable to everyone without giving anyone a way to overload the servers: the format and protocol must let the receiving website serve recordings as immutable static files from a CDN, so watching a replay never needs per-view work on the website or any traffic to the game host. Recording is off until its profile config exists. Operator decisions already made: replays become public after website approval; the host service lives in this repository; player map markers are recorded, chat is not."

## Current State

Nothing in the addon records a game. A mission ends with the statistics files and the
server logs, and nothing else survives of what happened on the map.

Two engine facts decide how a recorder can work. First, the game honours one impact
trigger per projectile, so a trigger added to a projectile replaces the one the round
already has and the round loses its effect. A recorder must therefore watch the game's
own events and never touch weapon or ammunition data. Second, the file interface has no flush and no rename. Written data
reaches the disk when the file is closed, so crash safety comes from closing files
often, and a finished file is recognised by its server having stopped, not by renaming
it.

The server can observe, without changing any prefab:

- the death of every character and vehicle, with the killer and the killer's relation to
  the victim (enemy, friendly, suicide), and at that moment the damage history, which
  names the cause;
- every character's life state: alive, unconscious, dead;
- shots fired by player characters, including a gunner firing a vehicle weapon, and
  grenades thrown. Whether the server receives the same for AI characters is not shown in
  the game scripts and must be established before the plan relies on it;
- the detonation of rockets, mortar rounds and launched grenades through the game's own
  explosion effect, and of thrown grenades by following the grenade until it is gone;
- characters entering and leaving vehicle seats, and vehicles being destroyed;
- all characters, AI included, from a list the game keeps. Vehicles have no such list and
  are tracked as they appear and disappear.

The game cannot compress data, and its outbound web requests carry only small text
bodies, so the game server writes and something outside the game sends.

The addon already holds most of what a recording needs:

- The unit statistics recorder runs only on the server. It names each server session by
  start time and world, stamps its events (kills, teamkills, zone presence, captures, key
  targets, group defeats, survivors) with the mission clock, and writes its files under
  the server profile: live every minute and approved at each Game Master publish. The session name exists only when statistics are on, and it survives a
  crash-resume only through the statistics' own continuation. Statistics reach a website
  today only when someone copies the files off the server and uploads them by hand.
- The mission clock runs during the game phase and stands still during every hard freeze.
- The lobby's roster names every slot's side, squad callsign, role and occupant, with the
  occupant's unit tag in the displayed name.
- Player-placed map markers are kept on the server with the sides that may see them, and
  mission-maker markers carry their own side gate.
- Every trigger fires through one point, and zone captures and contests are already
  reported to the statistics.
- At the start of the game phase every slotted player receives a fresh body, so the
  engine's network ids of characters change at that moment. Anything that follows
  characters across a game needs ids of its own.
- The session resume feature snapshots the game and continues it after a crash. Anything
  newer than the snapshot is lost from the world and from the statistics.

The feature fits the constitution: the recorder lives on the server, adds nothing to the
network and touches no prefab. The part that leaves the server is a separate service that
only sends, so neither the game nor the host gains a way in.

## Clarifications

### Session 2026-10-31

- Q: Who can watch a replay? → A: Everyone, once the receiving website has approved it.
  Publication must not give anyone a way to overload the servers.
- Q: Where does the service that sends recordings live? → A: In this repository, next to
  the addon.
- Q: Are map markers and chat recorded? → A: Player map markers are recorded with the
  sides that may see them; chat is never recorded.
- Q: Should public replays show each player's in-game name? → A: Yes, exactly as the game
  displayed it (for example `[ABC]Callsign`); only identity GUIDs stay private.
- Q: Should the host service send recording files while the game is still running, or
  only after it ends? → A: Only after. The recorder writes one file per game (a second
  file only after a crash-resume), and the service sends it once the game is over: ended,
  or cut off by a stop and not resumed. Nothing about a running game leaves the host, and
  an owner who runs the addon without the service gets one file per game to keep or
  share.
- Q: Should server owners who don't use the website get a way to watch their recordings?
  → A: Not now. The viewer is built on the receiving website only, and the recorder must
  leave the rest of the game exactly as it is.
- Q: Should the recording hold every individual hit, or only knock-outs and deaths? → A:
  Knock-outs and deaths only, each with who caused it, the weapon and the distance.
  Individual hits are not recorded.
- Q: Should the recording also hold the mission's drawn map areas (operation borders,
  freeze areas, restricted areas) with the sides allowed to see them? → A: Yes: every
  mission-maker map area with its outline, label, colour, the sides allowed to see it, and
  when it appears or disappears.
- Q: Should the header carry the addon's version as well as the format version? → A: The
  format version only. The game cannot report an addon's version at runtime, and a
  hand-bumped constant could mislabel files; any change to what a record means raises the
  format version instead.
- Q: Must squad and slot numbers stay the same across a crash-resume? → A: No (operator,
  decision B): they are numbered within a segment; the first snapshot of every segment
  restates the whole roster, and a squad keeps its callsign across segments.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Every game is recorded on the server (Priority: P1)

The server owner turns recording on once in the server profile. From then on every game
on that server is recorded from the start of the game phase until the server stops or
moves to the next mission: who held which slot in which squad, where every soldier and vehicle went,
who was knocked out, revived or killed, by whom and with what, who fired, where grenades,
rockets and mortar rounds landed, who sat in which vehicle seat, what markers each side
placed, which zones changed hands, and when the freeze and the game phases began and
ended. The players notice nothing.

**Why this priority**: without the recording nothing else in this feature exists, and it
is the part that must never cost the game anything.

**Independent Test**: in the demo world with recording configured, play a short game with
both sides and AI, perform the scripted events listed in the success criteria, stop the
server, then open the recording in the server profile and find each event with its
time, actors and position.

**Acceptance Scenarios**:

1. **Given** recording is configured and the mission is at slot selection, **When** an
   admin advances to the game phase, **Then** a new recording begins whose header names
   the mission, the terrain, the sides with their colours, the start time and the roster.
2. **Given** a recording is running, **When** a player kills an AI soldier with a rifle at
   150 metres, **Then** the recording holds the death with killer, victim, relation, the
   weapon and a distance within 2 metres of the true distance.
3. **Given** a recording is running, **When** a player throws a grenade and fires a
   rocket, **Then** both detonation points are recorded within 2 metres of where they
   happened.
4. **Given** a recording is running, **When** one player enters a vehicle as driver,
   another as gunner, and the vehicle is then destroyed, **Then** the seat changes with
   their roles and the destruction are recorded in order.
5. **Given** a recording is running, **When** a player of one side places a marker and
   later removes it, **Then** both are recorded with the sides that could see the marker.
6. **Given** a recording is running, **When** the server stops cleanly, **Then** the
   segment ends with a stop record and nothing more is written to it.
7. **Given** recording is not configured, **When** a full game is played, **Then** no
   recording file is written and the recorder writes nothing to the log.

---

### User Story 2 - A crash does not lose the game (Priority: P1)

The server process dies in the middle of a game. Everything recorded up to a few seconds
before the crash is on disk and readable. When the server owner resumes the game from its
snapshot, the same recording carries on as its next segment, and what the crash rolled
back is marked as such instead of being mixed into the resumed timeline.

**Why this priority**: crashes happen during the biggest games, which are the games
people most want to watch again.

**Independent Test**: on a dedicated server with recording and session saves on, kill the
server process during the game phase, check the recording on disk, resume with the game's
resume parameter, play on, and check that the resumed segment continues the same
recording.

**Acceptance Scenarios**:

1. **Given** a recording is running, **When** the server process is killed, **Then** the
   recording on disk holds everything that happened up to at most 10 seconds before the
   kill, and the file cut off mid-write is readable up to its last complete record.
2. **Given** a crashed game with a snapshot, **When** the server resumes it, **Then** the
   recording continues under the same identity in a new segment file that starts at
   the resumed mission time.
3. **Given** the crashed segment holds records newer than the snapshot, **When** the
   resumed segment begins, **Then** those records are identifiable as rolled back.
4. **Given** a crashed game, **When** the server is restarted without resuming, **Then**
   the next game starts a new recording, and the crashed one stays as it is, recognisable
   as cut off by its missing stop record.

---

### User Story 3 - Recordings and statistics leave the server by themselves (Priority: P2)

The server owner installs the companion service on the game host and gives it the
website's address and the server's credentials. When a game is over, the service sends
its recording along with the statistics files; while a game is running, or could still
be resumed after a crash, nothing about it leaves the host. Nobody downloads, copies or
uploads anything by hand. If the website or the network is down, the service waits and sends everything
later. It never slows the game.

**Why this priority**: it removes the manual work after every game, but recording has
value on its own and comes first.

**Independent Test**: point the service at a receiver that implements the documented
protocol, play a game, and compare what the receiver holds with the files in the server
profile; repeat with the receiver stopped during the game and started after it.

**Acceptance Scenarios**:

1. **Given** the service is configured and the receiver is reachable, **When** a game
   ends, **Then** within 2 minutes the receiver holds the game's recording files, its
   identity file and its latest statistics file.
2. **Given** a game is running, **When** the files the receiver holds are listed,
   **Then** nothing of the running game is among them.
3. **Given** a server stopped in the middle of a game, **When** it resumes that game,
   **Then** nothing of it is sent until the resumed game ends; **When** it instead starts
   a different game, or stays stopped longer than the configured wait, **Then** the
   cut-off recording is sent within 2 minutes.
4. **Given** the receiver is unreachable for an hour, **When** it comes back, **Then**
   everything finished meanwhile arrives within 15 minutes, with no file missing and no
   file counted twice.
5. **Given** the Game Master publishes the statistics again after the game, **When** the
   approved statistics file changes, **Then** the new version is sent.
6. **Given** the service is running, **When** the host's listening network ports are
   listed, **Then** none belongs to the service.
7. **Given** two game servers on one host both record a game, **When** their files reach
   the receiver, **Then** each arrives under its own server identity and none is mixed
   into the other's recording.
8. **Given** one recording the receiver confirmed more than the retention time ago and one
   it never confirmed, **When** the service cleans up, **Then** the confirmed one is
   deleted from the host and the unconfirmed one is kept.

---

### User Story 4 - Watching a replay costs the servers nothing (Priority: P2)

Anyone may watch a replay once the website has approved it, so a recording must be
servable to any number of viewers without touching the game host and without the website
doing work for each viewer. Everything the service sends is final when sent, never
changes afterwards, and is enough on its own to show the replay. Players appear by the
names the game displayed; nothing that viewers download carries an account identity.

**Why this priority**: the operator made public replays conditional on this. A feature
that lets viewers overload the servers is not acceptable.

**Independent Test**: take a complete recording as sent, check that it can be read with
the documented format alone, check that no file identity was ever sent with two different
contents, and search every file meant for viewers for identity GUIDs, network addresses
and secrets.

**Acceptance Scenarios**:

1. **Given** a sent recording, **When** its files are read with the documented format and
   nothing else, **Then** the whole game can be reconstructed: roster, movement, events
   and phases.
2. **Given** a file was sent, **When** its content changes later (a new statistics
   version), **Then** it is sent as a new file with a new identity and the earlier one is
   never overwritten.
3. **Given** the files meant for viewers, **When** they are searched, **Then** they
   contain no identity GUID, no network address and no secret.
4. **Given** an upload without valid credentials, **When** a receiver that follows the
   protocol gets it, **Then** the receiver can refuse it from its headers alone, before
   reading the body.

---

### User Story 5 - The admin can see that the game is being recorded (Priority: P3)

During a game an admin types `/replay` and sees whether the server is recording, the
recording's identity, how long it has been recording, how large the file is and when the
last write happened, or why recording stopped.

**Why this priority**: it is the in-game test control for the other stories and lets an
admin confirm the recording before a big game.

**Independent Test**: in the demo world, type `/replay` as an admin with recording
configured, with it unconfigured, and as a non-admin.

**Acceptance Scenarios**:

1. **Given** recording is configured and running, **When** an admin types `/replay`,
   **Then** the reply shows the recording state, identity, duration, file size and last
   write time.
2. **Given** recording is not configured, **When** an admin types `/replay`, **Then** the
   reply says recording is not enabled.
3. **Given** any state, **When** a non-admin types `/replay`, **Then** the server refuses
   and reveals nothing about the recording.
4. **Given** the admin's game runs in Russian, **When** they type `/replay`, **Then** every
   line of the reply is in Russian.

---

### Edge Cases

- A player disconnects and their character stays in the world: the character keeps being
  recorded, and the roster records the disconnect and any reconnect.
- A Game Master possesses an AI character: the recording shows who controlled it from
  when to when, and the character keeps its recording id.
- An admin moves a player to another slot or side mid-game: the roster change is recorded
  at its time, and earlier records stay with the slot they happened in.
- Entities created mid-game by the mission or a Game Master are recorded from their first
  appearance. Deleted entities are recorded as removed; a deletion is never a death.
- Corpses and parked vehicles produce no movement records once they are still.
- During a hard freeze nothing moves and the mission clock stands still. Joins, slot
  changes and admin actions in that time carry the frozen mission time and their real
  time.
- A game that runs far longer than planned keeps recording into the same file; a
  recording has no length limit of its own.
- Player names in any script, including Cyrillic and names containing separators or
  quotes, are recorded exactly as the game displays them.
- Statistics are turned off: the recording still has its identity and is complete without
  statistics, and nothing waits for statistics files.
- The service is not installed: recordings stay in the server profile, the recorder never
  deletes them, and the server owner manages the folder.
- The disk fills up or a file cannot be written: the recorder stops recording, says so
  once in the log and in `/replay`, and the game continues unaffected.
- Two game servers run on one host: each is sent under its own identity and credentials,
  and their recordings never mix.
- The host clock is off by less than five minutes: uploads are still accepted, while an
  upload captured and replayed outside the five-minute window is refused.
- The receiver asks the service to slow down or answers with an error: the service waits
  as asked, keeps every file and tries again later. It deletes nothing that was not
  confirmed.
- A recording cut off by a crash and never resumed stays a complete recording up to the
  crash, recognisable by its missing stop record.
- Recording is turned on for a server in its usual configuration: weapons, ammunition,
  vehicles and every other part of the game behave as with recording off, because the
  recorder only observes.

## Requirements *(mandatory)*

### Functional Requirements

**Recording on the server**

- **FR-001**: Recording MUST be off until the server owner writes the recorder's
  configuration in the server profile. Unconfigured, the recorder MUST write no file and
  no log line and do no work, and `/replay` MUST answer that recording is not enabled. A
  configuration file that exists but cannot be read MUST produce one warning naming the
  file and leave recording off.
- **FR-002**: When configured, the server MUST start one recording when the game phase
  begins. A server stop ends the current segment: a crash-resume continues the recording
  in a new segment (FR-022), and a game that is not resumed ends at that point. Slot selection and briefing are not recorded, but
  the markers and the roster as they stand at the start of the game phase are.
- **FR-003**: Each recording MUST carry the identity of its server session, the same
  identity the statistics files carry, whether or not statistics are enabled, and MUST
  keep it across a crash-resume.
- **FR-004**: Every segment file of a recording MUST begin with the recording's header,
  naming the mission, its author, the terrain and its bounds, the in-game date and time
  of day, the real start time in UTC, every side with its name and colour, and the
  recording format version.
- **FR-005**: A recording MUST hold the roster: every slot's side, squad callsign as the
  game shows it, role name and occupant by displayed name; and every change of occupant,
  connection state and controller (joins, leaves, reconnects, slot moves, Game Master
  possession) with its time. The displayed name is recorded exactly as the game showed it
  to players and is part of what viewers see.
- **FR-006**: A recording MUST hold the movement of every character, player or AI, and of
  every vehicle including static weapons and aircraft: position with altitude, and
  heading. Characters MUST be checked at least once per second and vehicles at least twice
  per second, and an entity MUST be written whenever it has moved at least 1 metre or
  turned at least 5 degrees since its last record. Still entities MUST NOT be written
  again.
- **FR-007**: A recording MUST hold every change of a character's life state (alive,
  unconscious, dead) with its time. A knock-out MUST carry who caused it, the weapon or
  cause as the game names it, and the distance, as a death does. Individual hits that
  neither knock out nor kill MUST NOT be recorded.
- **FR-008**: A recording MUST hold every death of a character and every destruction of a
  vehicle with the victim, the killer when there is one, their relation (enemy, friendly,
  suicide, no killer), the weapon or cause as the game names it, and the distance.
  Deletions MUST NOT be recorded as deaths.
- **FR-009**: A recording MUST hold who occupies which vehicle seat, with the seat's role
  (driver, gunner, commander, passenger), from when to when.
- **FR-010**: A recording MUST hold shots fired by characters, including weapons a
  character operates in a vehicle, as a summary per shooter with time, position, direction
  and number of shots, at most one record per shooter per second however fast the weapon
  fires. Bullets are not followed in flight.
- **FR-011**: A recording MUST hold the detonation point and time of thrown grenades,
  launched grenades, rockets and mortar rounds, with the shooter when the game reports
  one. Other explosive rounds are recorded where the game reports their detonation to the
  server without any change to weapon or ammunition data; the plan establishes which.
- **FR-012**: A recording MUST hold every player-placed and mission-maker map marker that
  exists at the start or is placed, changed or removed later, with its position, symbol,
  colour, text, author and the sides allowed to see it. Map drawings that exist only on a
  player's screen are not recorded.
- **FR-013**: A recording MUST hold every trigger zone of the mission with its outline and
  label, every change of a zone's holder, every capture, and every trigger firing, with
  their times. It MUST also hold every map area the mission maker drew (operation
  borders, freeze areas, restricted areas) with its outline, label, colour and the sides
  allowed to see it, and the time a timed area appears or disappears.
- **FR-014**: A recording MUST hold the game flow: the start of the game phase, the start
  and end of freeze time and of every hard freeze, and every admin change to their length.
- **FR-015**: Recorded times MUST use the mission clock the statistics use, so that a
  statistics event and a recorded event of the same moment carry the same time. A
  recording MUST also carry the real time in UTC often enough to recover the wall-clock
  time of any record within one second.
- **FR-016**: Chat messages and voice MUST NOT be recorded.

**Format, files and durability**

- **FR-017**: A recording MUST be written under the server profile as one file per
  segment, so a game without a crash-resume is one file. At least every five minutes of
  mission time the file MUST hold a full snapshot of everything that exists at that
  moment, so that a reader can start from any snapshot with nothing but that file.
- **FR-018**: Recorded data MUST reach the disk no later than 10 seconds after it
  happened. A file cut off mid-write MUST stay readable up to its last complete record.
- **FR-019**: A segment file is final when its recording has ended or the server that
  wrote it has stopped. A final file MUST never change afterwards.
- **FR-020**: The format MUST be versioned, documented in this repository, readable by a
  person with a text editor, and skippable record by record, so that a reader ignores
  record types it does not know. The format version MUST be raised whenever the meaning of
  a record changes, so that a reader can recognise and work around files written by a
  faulty release by their format version alone.
- **FR-021**: Identity GUIDs MUST NOT appear in the segment files. The link
  between a recording's players and their identities, which a website needs to match
  statistics, MUST be written to a separate identity file per recording.
- **FR-022**: A crash-resumed game MUST continue its recording in a new segment file
  starting at the resumed mission time, and records of the earlier segment newer than that time
  MUST be identifiable as rolled back. A plain restart MUST start a new recording.
- **FR-023**: The recorder MUST NOT delete or rewrite a recording it has finished.

**Cost to the game**

- **FR-024**: The recorder MUST run only on the server and add no replicated value and
  nothing to the join-in-progress data, and no remote procedure call other than the
  admin's on-demand `/replay` request and its reply (FR-039).
- **FR-025**: The recorder MUST NOT change any weapon, ammunition, character or vehicle
  prefab. It MUST only observe the game's own events and read the state of entities, so
  the game plays the same with recording on or off.
- **FR-026**: The recorder's work MUST be spread over frames so that no single frame
  carries a whole sampling pass, and it MUST write to disk in batches, no more often than
  once per second.
- **FR-027**: A failure to write MUST stop the recording and be reported once in the log
  and in `/replay`, and MUST never stop, slow or alter the mission.

**Sending recordings off the server**

- **FR-028**: The companion service MUST run on the same machine as the game server, read
  the recordings and statistics folders of the server profile, and stay inert until the
  server owner writes its configuration: the website's address, the server's identity and
  secret, the folders to watch, the upload rate cap, the wait after which a game cut off
  by a stop counts as over, and the local retention time. No
  address, identity or secret is part of this repository.
- **FR-029**: The service MUST send nothing about a game while that game is running or
  could still be resumed. A game is over when its server has stopped
  and has since started a different game, or has stayed stopped longer than the configured
  wait. Once a game is over, the service MUST send each of its
  segment files, its identity file and its latest statistics file, and afterwards every
  new version of the statistics files, such as an approval the Game Master publishes after
  the game.
- **FR-030**: The service MUST NOT listen on any network port and MUST NOT contact
  anything but the configured website.
- **FR-031**: Every upload MUST prove which server sent it with a per-server secret that is
  carried outside the address, MUST be signed together with its time so that a captured
  upload is refused when replayed more than five minutes from that time, and MUST let the
  receiver refuse it from its headers alone, before reading its body.
- **FR-032**: Every sent file MUST have an identity derived from its content. Sending the
  same content twice MUST be harmless, and a changed file MUST be sent as a new file.
- **FR-033**: The service MUST survive network outages, website downtime, requests to slow
  down and its own restarts without losing or duplicating a file, retrying with growing
  waits, and MUST remember what the receiver has confirmed.
- **FR-034**: The service MUST never send faster than its configured cap and MUST send no
  single request larger than 1.5 megabytes.
- **FR-035**: The service MUST delete local recordings and statistics only after the
  receiver confirmed them and the retention time has passed, and MUST NOT delete anything
  else.
- **FR-036**: The service MUST report its state (last successful upload, files waiting,
  failures) in a form the host's monitoring can read, and MUST keep secrets out of its
  logs.
- **FR-037**: The upload protocol (requests, signatures, file identities, answers, limits
  and retry rules) MUST be documented in this repository next to the code that speaks it,
  so that any website can implement the receiving side.
- **FR-038**: One host MUST be able to run the service for several game servers, each sent
  under its own identity.

**Admin control**

- **FR-039**: `/replay` MUST be available to admins only, checked on the server, and MUST
  show the recording state, identity, recorded duration, file size and last write time,
  or the reason recording stopped. A non-admin MUST get a refusal and nothing else.
- **FR-040**: Every message `/replay` shows MUST exist in English and Russian.

### Key Entities

- **Recording**: one game on one server, from the start of the game phase to its last
  server stop, identified by its server session. It is made of one or more segment files
  and an identity file.
- **Segment**: the stretch of a recording between one server start and the next stop,
  written as one file that begins with the recording's header. A crash-resume begins a
  new segment of the same recording.
- **Snapshot**: the full state of everything that exists at one moment, written into a
  segment file at least every five minutes so that a reader can start there.
- **Recorded entity**: a character or vehicle with an id of the recording's own, its kind,
  side, display name, squad and role.
- **Roster entry**: a slot with its side, squad callsign, role and changing occupant. The
  occupant appears by displayed name only.
- **Event**: a timed happening: death, life state, shot summary, detonation, seat change,
  marker change, zone change, trigger firing, phase change.
- **Identity file**: the private link between a recording's players and their identity
  GUIDs, sent to the website and never meant for viewers.
- **Statistics files**: the existing files of the same server session, unchanged by this
  feature and sent with the recording.
- **Upload**: one file sent by the service, with its content identity, server identity and
  signature.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With 127 players and at least 100 AI in the game phase on a dedicated
  server, the recorder's own work, summed per five-minute period from its own millisecond
  timings and logged, averages under 3 milliseconds per second of game time; no single
  recorder step takes more than 2 milliseconds; and the server's average frame rate over
  the same scenario is within 2 percent of its average with recording off.
- **SC-002**: Network traffic per client with recording on equals traffic with recording
  off in the same scenario on a dedicated server.
- **SC-003**: A two-hour game with 127 players and up to 100 AI produces a recording of at
  most 60 megabytes on disk and at most 10 megabytes as sent.
- **SC-004**: After a forced kill of the server process the recording holds everything up
  to at most 10 seconds before the kill, and after a resume it continues as a new segment
  within 10 seconds of the game continuing; one check in the demo world, one on a
  dedicated server.
- **SC-005**: In one scripted demo-world game (an AI killed with a rifle, a teamkill, a
  player knocked out and revived, a thrown grenade, a rocket, a vehicle crewed and
  destroyed, a marker placed and removed by one side, a zone captured, a timed map area
  disappearing, a player disconnected and reconnected, freeze time ended by an admin),
  every event is in the
  recording with its time within 1 second, the right actors, and its position within 2
  metres.
- **SC-006**: Every statistics kill event of a game has a recorded death with the same
  victim within 1 second of the same mission time.
- **SC-007**: With the receiver reachable, a game's recording files, identity file and
  latest statistics arrive within 2 minutes of the game being over, and nothing of a
  running or still resumable game arrives earlier. After a one-hour receiver outage everything
  arrives within 15 minutes of recovery, with no gaps and no duplicates.
- **SC-008**: Over any 10 seconds the service never exceeds its rate cap, and at its
  default cap the recording of a two-hour game is sent within one minute.
- **SC-009**: The host has no new listening port with the service running. Uploads with a
  wrong secret, without a signature, with a time outside the window, or replayed, are all
  refused by a receiver that follows the protocol, and no secret appears in any address
  or log line.
- **SC-010**: A search of every file meant for viewers in a full recording finds no
  identity GUID, network address or secret, and no sent file identity ever maps to two
  different contents.
- **SC-011**: With recording unconfigured, a full demo-world game leaves no recording file
  and no recorder log line, and `/replay` answers that recording is not enabled.
- **SC-012**: A person opening a recording file in a text editor can tell its time span,
  the entities in it and its events without other tools, and a file cut at a random byte
  is read up to its last complete record.

## Assumptions

- One recording per game, as the statistics already count one server session as one game.
- The goal is a replay on the map: positions with altitude and heading, no body poses, no
  per-bullet flight paths and no per-hit damage. Hits show as knock-outs and deaths.
- Checking characters once per second and vehicles twice per second, with 1 metre and 5
  degree thresholds, is enough for a smooth replay when the viewer interpolates between
  records.
- Nothing is sent while a game runs or could still be resumed. A finished recording of a
  few megabytes goes out in seconds within the rate cap. If the host itself fails during a game, that game's
  recording is lost; this rare loss is accepted so that a running game is never exposed.
- An owner who runs only the addon keeps one file per game in the server profile; the
  service and the website are optional. No viewer ships in this repository for now.
- The receiving website, its approval step, its storage behind a content delivery network,
  the replay viewer, map imagery and any announcement of published replays are built
  outside this repository against the documented protocol and format.
- The operator adds the recorder to the lobby game-mode prefab in Workbench once. Every
  mission built on that game mode inherits it, and no mission needs a setting of its own.
- The recorder works without the service, and the service works with any game server that
  writes recordings in this format.
- Local retention after confirmed receipt defaults to 14 days.
- The demo world provides both sides, AI squads, a vehicle and a trigger zone. The plan
  names any entity it adds there, such as a rocket launcher kit.
