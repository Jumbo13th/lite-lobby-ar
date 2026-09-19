# Quickstart: validating Session Resume After a Server Crash

## Prerequisites

- The addon compiled in Workbench with the new scripts and the two config files
  from [contracts/config-files.md](contracts/config-files.md).
- The demo scenario `Missions/LobbyDemo.conf` with its systems config pointed at
  `LL_LobbySystems.conf` and its save types at the default, on a **dedicated
  server**. Nothing here is trustworthy on a listen host.
- `m_bSessionSaves` ticked on the demo world's game-mode entity.
- In the test server's `config.json`, the `persistence` block with
  `loadSessionSave: false`, `autoSaveInterval: 1`, and the session storage pointed at
  the game's JSON database preset so a snapshot can be read.
- Three clients: two on opposite factions in one squad each, one of them admin, and
  a third for the late-join and early-connect cases. The demo world's stock rifle
  squads, at least one car, one helicopter and, if the demo has one, one tracked
  vehicle on a slope; one destructible building; one named trigger of each kind
  under test; one AI enemy group within sight of a player position.
- A way to kill the server process (not a graceful stop), a way to stop it
  gracefully (the container stop), and a way to start it with an extra launch
  parameter for one start, all with the same profile.

Every scenario ends with the log excerpt handed back. Where a scenario names an
expected result the spec calls **unverified**, the observed result is recorded in
research.md whatever it is.

## Scenario 0: fresh by default, resume on request (US3, SC-007; Phase 2 gate)

Run with the engine records only, before any lobby record exists.

1. Start the game phase, wait for two `[LL_Lobby] Snapshot AUTO ... ok in N ms` log
   lines, kill the server.
2. Start it plainly. Expected: slot selection, and the log says
   `Resume: no snapshot loaded`.
3. Kill it again. Start it with `-loadSessionSave`. Expected: the log shows
   `Resume: snapshot <uuid> <time> active`, the world's entities are restored, and
   every squad's members appear once in the log; slot selection is expected at this
   stage because no lobby record exists yet.
4. Read the snapshot folder in the profile: note the UUID in the `meta-info.json` of
   the older snapshot. Kill the server, start it with `-loadSessionSave <uuid>`.
   Expected: the older world.

If step 3 comes up with `Resume: no snapshot loaded` although the log lists
snapshots, the launch parameter does not override the config file: stop here and
report; the operator decides between the configuration value flipped for one start
and any other option. No addon chooser is built.

**Final-build variant** (for the full run at the end): steps 1, 2 and 4 unchanged;
step 3 expects the held game phase, not slot selection. A snapshot without a lobby
record is then Scenario 7.3, a refusal.

## Scenario 1: the world comes back, once (US1)

1. Both clients slot, in different squads, at least one of which has callsigns so
   its roster label differs from its entity name; start the game, end the freeze.
   Destroy the building, wound one character, drive the car somewhere and park it
   with a noted fuel level, drop a weapon, open a door, place a map marker, arm a
   mine, capture the capture-zone trigger, let the mission-end timer run for a
   minute, get one kill.
2. Wait for a snapshot. Then get a second kill, destroy a second building, damage
   and refuel the car, and move both players. Kill the server. Start it with the
   resume parameter.
3. Reconnect both clients.

Expected: the mission is in the game phase with the hold engaged; the roster has
exactly as many slots as at the snapshot, each under the squad it belonged to, and no
squad has a second set of members standing at its spawn; each squad's radio frequency
is the one from its briefing; each client is in its own character, within a metre of
where it stood at the snapshot (compare the `body available` position in the log with
the held position), with the same wounds and kit; the first building is a ruin and the
second intact; the car is where it was parked with the snapshot's fuel and damage,
not the later ones; the weapon lies where it fell; the door is as it was; the marker
is on the map for the same faction; the mine is armed; the captured zone is shown
captured and its effect does not fire again; the mission-end countdown continues
from its remaining time; the spectator clock reads the snapshot's remaining mission
time, standing still until the release (asserted from Phase 4 on; in the Phase 3
build the plain hard freeze does not yet hold the clock); the statistics show the
first kill and not the second, their event timestamps are mission-clock seconds,
and after the release a new kill is recorded and the publish works. Count the
assignment broadcasts in the server log: exactly one per returning holder.

## Scenario 2: dead players, seats, aircraft (US1 edge cases; the pin spike)

1. Kill one player's character. Seat the other in the helicopter's pilot seat, take
   off, hover at 30 m.
2. Wait for a snapshot. Kill and restart with the resume parameter. Reconnect both.
3. Release the hold with `/hardfreeze 0`. The pilot flies for a minute. Engage a
   hard freeze with `/hardfreeze 120` while airborne, release it again.
4. Repeat steps 1 and 2 with the helicopter carrying a passenger and no pilot, and
   once more with a damaged engine.

Expected: the dead player comes back as a spectator, their slot KIA, their body where
it fell. The pilot comes back in the pilot seat of a helicopter that holds its
position in the air for as long as the hold lasts, through the hand-over of the
aircraft to the pilot's machine; on release the aircraft is in autohover and flies
when the pilot takes the controls; the second hold and release behave the same; the
pilotless and the damaged aircraft hold position too. Any criterion that fails stops
the phase; the operator decides the fallback.

## Scenario 3: the hold (US2)

1. Resume as in Scenario 1 but reconnect only one client first, positioned in view
   of the AI enemy group.
2. Try to move, fire, enter the car, leave a seat, place a marker, damage the other
   player's body. Note the mission-end countdown value and the position of the
   tracked vehicle on the slope.
3. Read the HUD. Wait two minutes. Have the non-admin client type `/hardfreeze 0`.
4. Reconnect the second client. Release with `/hardfreeze 0` from the admin.

Expected: nothing in step 2 works; the AI may move but neither it nor any player
takes damage; the HUD names the snapshot time, shows "1 of 2 players back" and the
limits line; the non-admin release is rejected; the countdown did not move during the
wait; after the admin release both act at the same moment, a broadcast says the game
is on, the countdown continues from its noted value, daylight moves again, and the
release log lists any vehicle that moved more than a metre.

Then, the clock and freeze cases, each a separate resume:

- Snapshot during the mission's initial timed hard freeze (daylight advancing).
  Expected: after the admin releases the resume hold, the timed hard freeze runs
  for its saved remainder, the freeze zones confine the players again, the
  ordinary freeze countdown then runs to zero and ends, only then does the
  mission-end timer start with its full configured duration, and daylight
  advances again after the timed freeze ends.
- Snapshot after the hard freeze ended but before the freeze ended. Expected: the
  mission-end timer starts with its full duration when the freeze ends, not at
  zero.
- Resume and leave the server empty for two minutes, then connect; later have
  every player disconnect for two minutes during the hold and reconnect. Expected:
  the spectator clock and the server log's elapsed value are unchanged by both
  waits.
- Two spectators, one on a client with a frame-rate cap of 30 and one uncapped,
  one of them joining mid-hold. Expected: both clocks show the same value within a
  second, before and after the release; note in the log which arrived first at
  the joining client, the hold flag or the clock value, and that the value
  settled within a second either way.
- With saves off: adjust the freeze time with the admin command during the
  initial hard freeze, then let the hard freeze end. Expected: the freeze drains
  one second per second, not two. Repeat during a resume hold; and once after
  ending the freeze early and adjusting it again.
- Release a resumed server, have everyone disconnect for two minutes, then join
  as a spectator. Expected: the countdown shown equals the time at which the
  mission-end timer actually fires, to the second; repeat with three short holds
  of a few seconds each. Then snapshot a started timer with under a second left
  and resume; expected: it fires within one evaluation of the release, not two or
  three seconds later.
- Start with daylight advancing, snapshot during the initial timed hard freeze,
  resume, snapshot again during the resume hold, kill, resume from that second
  snapshot, release. Expected: daylight advances after the release.
- With a mission set to fixed daylight, resume and release. Expected: daylight
  stays fixed.
- Step outside a freeze zone so the return countdown is running, let a snapshot
  land, resume. Expected: the countdown does not drain during the hold, and the
  player is enforced again from their first entry into the zone.

## Scenario 4: switch off, planned stop, debriefing (US3)

1. Untick `m_bSessionSaves`. With a one-minute hard freeze and a ten-minute freeze,
   start the game with one spectator watching. Expected: the spectator clock reads
   the full mission-end duration and stands still through the hard freeze, the
   freeze countdown starts when the hard freeze ends, the freeze ends eleven
   minutes after the game started, the mission-end countdown then drains; issue a
   five-minute admin hard freeze mid-game and confirm the countdown and daylight
   read the same at its start and end. Run five more minutes, restart with the
   resume parameter. Expected: no snapshot log line at all, slot selection.
2. Tick it again. Untick the mission's save types in Workbench, start the game phase.
   Expected: one log line naming that cause. Re-tick. Point the mission at the game's
   own `MissionSystems.conf` instead of the lobby's; expected: one log line naming
   that cause. Restore.
3. Play into the game phase, wait for a snapshot, play one more minute, stop the
   container gracefully. Expected: a `SHUTDOWN` snapshot in the log before exit. Note
   how long the process took to exit; twice that is the stop budget to record.
   Restart with the resume parameter. Expected: the world of the stop, not of the
   scheduled snapshot. Repeat the stop once while a save is in flight and once
   while a Game Master revive is in progress; expected: the in-flight save
   completes, exactly one `SHUTDOWN` save follows it (two snapshots, the later one
   is the stop, no third), the replacement completes or is logged as abandoned
   before the shutdown save, no stray body after the resume, and the process exits
   within the budget; the snapshot folder shows exactly one stop-time snapshot,
   whichever path wrote it (compare request log lines with committed snapshots).
   Stop once more with a revive pending and no autosave queued; expected: the
   stop-time save runs after the replacement finishes, not never. Also force two
   replacement failures after the exclusion is acquired (in a test build: make
   the spawn return null once, and delete the new body before its finish runs
   once) and confirm the count returns to zero and the next autosave runs. And:
   with the interval at one minute, start a game whose first autosave is queued
   before the game phase; expected: it lands after the fresh bodies have finished
   replacing, and the snapshot holds one body per slot; in a test build, make the
   first of two spawns fail synchronously; expected: saving opens only after the
   second replacement finishes, and the roster check runs once, after it.
4. Advance to debriefing, then restart with the resume parameter. Expected: slot
   selection, and the log says no snapshot was loaded.

## Scenario 5: the save is not felt (US4, SC-001, SC-003, SC-006)

On the live event server at full player count, with the server's default ten-minute
interval:

1. Before the feature is deployed, record one hour of per-client traffic at the same
   player count as the baseline.
2. With the feature, read the `Snapshot ... in N ms` lines for an hour, the
   disconnect reasons in the server log, per-client traffic, and have two observers
   report any movement or voice interruption at the logged save times.
3. Force-kill the server once and resume; everyone reconnects.

Expected: every duration under one second; no disconnect attributed to a snapshot;
traffic within the baseline plus the three once-set scalars; no interruption
reported; every reconnecting player back in their own character within a metre as
SC-001 measures it (vehicle occupants against the vehicle's position at release). A
failed number is a failed criterion.

## Scenario 6: reconnect cases (US1, FR-006)

1. Resume with one client absent. Have the third client (a new player) connect
   while the server log still shows the load in progress (before `hold engaged`).
   Release the hold. Repeat the resume with the absent holder themselves connecting
   during the load. In the same session, have the admin move a player to another
   slot and revive a dead player through the Game Master; expected: both work as
   today, one assignment each. Then, with saves off, disconnect a player during
   slot selection and reconnect, and disconnect a player whose squad the admin
   then locks and reconnect; expected: today's behaviour, untouched by the claim.
   Then deliver a late identity inside the one-second spectator delay: with the
   verification gate on, connect during the load so the finaliser sends the
   spectator-entry request, and inject the identity 0.2 s after that request (a
   test-build delay on the audit), so the claim runs at 0.2 s, possession at 0.7 s
   and the spectator entry fires at 1.0 s; expected: the player is in their body,
   not in spectator, and their voice is parked as an in-body speaker.
2. Ten minutes later, reconnect the absent client. In a separate run, keep them
   absent through the publish and the debriefing.
3. Lock the absent client's squad as admin before the next snapshot; kill, resume,
   reconnect them. Then with a KIA holder: kill their character, snapshot, resume,
   reconnect.
4. With the name-form reconnect key enabled on the mission, repeat step 2 with a
   client whose identity the backend does not supply.
5. Resume with one holder absent, wait for a second snapshot, kill, resume from
   that second snapshot, then reconnect the holder.

Expected: the early-connecting new player is a spectator (or takes a free slot if
the admin allows it) and never takes a reserved slot; the early-connecting holder is
in their own body when the hold engages; the absent holder's slot stays theirs the
whole time and they are put back into their body on return, and when they never
return their survival credit in the published statistics is on their saved identity;
the locked holder gets their body back with the squad still locked; the KIA holder
gets their slot back as a spectator without possessing the corpse and hears the
spectator voice room, not the squad's; the name-form holder is put back by name;
the twice-absent holder is put back after the second resume.

## Scenario 7: faults and rare cases (FR-013, FR-014, FR-020, SC-008)

1. Kill the server inside a save (watch for `Snapshot AUTO` without its `ok` line).
   Resume. Expected: the previous complete snapshot loads; record which one did.
2. Change one script in the addon between a snapshot and a resume. Expected: the
   game or the lobby refuses; record what refused and how the server ended.
3. Produce a snapshot with no lobby record (mission pointed at the game's own
   `MissionSystems.conf`, one snapshot), restore the lobby config, list the
   snapshot folder, resume from it. Expected: `Resume refused: no lobby record`,
   the snapshot folder identical before and after (no new save, none removed),
   the process exited; note what the container did next (stayed stopped, or
   restarted plainly into a fresh game).
4. Delete the statistics file named by the newest snapshot, resume. Expected: the
   world resumes, the log says the statistics are incomplete, the publish panel
   shows it; after the release, a new kill and a slot change are recorded and the
   publish works, under the original statistics session id. Let another snapshot
   land, resume from that one; expected: still marked incomplete, same session id.
5. Take a snapshot while a resume hold is active; resume from it. Expected: the
   clocks are those of the moment before the first hold, daylight follows the
   pre-hold setting after release.
6. Kill every member of one squad and let the bodies be removed before a snapshot;
   resume. Expected: those slots and their squad row are absent from the roster,
   as they were before the crash; a KIA slot whose corpse remained is KIA with the
   corpse.
6a. Force a native load failure (in a test build, corrupt the newest snapshot's
   world data, not the lobby record) and resume. Expected: `Resume refused: load
   failed`, snapshots untouched, server shut down; distinct from 7.3.
6b. Set the server's retention to 1, take three snapshots, resume from the only
   one; then with retention 3 take three, roll back to the oldest, take two more.
   Expected: after each successful save the statistics folder holds no file older
   than the oldest listed snapshot by more than a minute, and the file of the
   loaded snapshot is always present.
6c. Start a mission whose squad has no entity name, one with two squads sharing a
   name, one whose player prefab lacks the persistence component, and one with
   two mission-end timers. Expected: one log line each when the game phase's
   bodies have finished replacing, naming the squad, the slot or the timer.
   Then snapshot and resume the untracked-body mission. Expected: `Resume refused:
   slot <name> has no persistence id`.
6d. In a test build, make the save listing callback return an empty successful
   list once and a failed list once, then let a save land each time. Expected:
   no statistics file is pruned in either case; in a normal run, the file written
   30 s before its own save point survives the next prune.
6e. In a test build, delay one body's network id past the finaliser's deadline.
   Expected: `Resume refused` naming that slot, no fallback id in the log. Then
   make one body available during deserialisation, before `ACTIVE`, and register
   normally; expected: resumed, no premature refusal. Then delay a registration
   to just after `ACTIVE` plus ten seconds; expected: refused.
6f. Restart with a corrupted world part of the snapshot so the native load fails
   before `ACTIVE` would arrive. Expected: `Resume refused: load failed` without
   waiting for `ACTIVE`, snapshots untouched, server shut down.
7. As Game Master, start placing a composition and let a snapshot land mid-edit;
   resume. Expected: what was placed before the save is there, nothing is doubled,
   the editor works.
8. Seat a character in a vehicle a Game Master then deletes before the snapshot;
   resume. Expected (unverified): the character is on the ground at the vehicle's
   last position; record the outcome.
