# Quickstart: validating Session Resume After a Server Crash

## Prerequisites

- The addon compiled in Workbench with the new scripts and the two config files
  from [contracts/config-files.md](contracts/config-files.md).
- The demo scenario `Missions/LobbyDemo.conf` with its world-systems config pointed
  at `LL_LobbySystems.conf`, on a **dedicated server**. Nothing here is trustworthy
  on a listen host.
- `m_bSessionSaves` ticked on the demo world's game-mode entity and
  `m_iSaveIntervalMinutes` set to 1 for the tests.
- Two clients on opposite factions, one of them admin; a third client for the
  late-join scenario. The demo world's stock rifle squads, at least one car and one
  helicopter, one destructible building, one named trigger of each kind under test.
- A way to kill the server process (not a graceful stop) and start it again with
  the same profile.

## Scenario 0: the engine applies the save on restart (research R7 spike)

1. Start the game phase, wait for one `[LL_Lobby] Save point AUTO ... ok in N ms`
   log line, kill the server, start it again.
2. Read the log for the resume lines: `[LL_Lobby] Resume: save active`, then the
   per-slot `Resume: slot ... body available` lines and `Resume: hold engaged`.

Expected: the lines appear without any script-side selection. If the server instead
logs `Resume: no active save` and comes up at slot selection, the script-side
fallback in R7 is the implementation path; rerun the scenario after it.

## Scenario 1: the world comes back (US1)

1. Both clients slot, start the game, end the freeze. Destroy the building, wound
   one character, drive the car somewhere and park it, drop a weapon, open a door,
   place a map marker, arm a mine, let the mission-end timer run for a minute.
2. Wait for a save point. Kill the server. Start it again.
3. Reconnect both clients.

Expected: the mission is in the game phase with the hold engaged; each client is
in its own character, within a metre of where it stood, with the same wounds and
kit; the building is a ruin; the car is where it was parked with the same fuel and
damage; the weapon lies where it fell; the door is as it was; the marker is on the
map for the same faction; the mine is armed; the mission-end countdown continues
from its remaining time; the roster shows every slot with its holder.

## Scenario 2: dead players, seats, aircraft (US1 edge cases)

1. Kill one player's character. Seat the other in the helicopter's pilot seat,
   take off, hover at 30 m.
2. Wait for a save point. Kill and restart the server. Reconnect both.

Expected: the dead player comes back as a spectator, their slot KIA, their body
where it fell. The pilot comes back in the pilot seat of a helicopter that holds
its position in the air for as long as the hold lasts (spike: if it falls, stop
here and report; the operator decides the fallback).

## Scenario 3: the hold (US2)

1. Resume as in Scenario 1 but reconnect only one client first.
2. Try to move, fire, enter the car, damage the other player's body.
3. Read the HUD.
4. Reconnect the second client. Release with `/resume` from the admin.

Expected: nothing in step 2 works; the HUD says the game is resuming and shows
"1 of 2 players back"; after `/resume` both act at the same moment and a broadcast
says the game is on; the freeze timer, if any remained, resumes counting.

## Scenario 4: owner controls (US3)

1. `/save` as admin: expect a confirmation with the time and a log line. As a
   non-admin: the refusal.
2. `/discard` without the argument: expect the explanation. `/discard confirm`:
   expect the confirmation; restart the server: expect slot selection.
3. Play into the game phase again, `/shutdown` as admin: expect the broadcast, a
   blocking save point in the log and the process exiting; restart: expect a resume
   from that moment.
4. Advance to debriefing and restart: expect slot selection.
5. Untick `m_bSessionSaves`, run a game for ten minutes and restart: expect no save
   log line at all and slot selection.

## Scenario 5: the save is not felt (US4)

On the live event server at full player count, with the interval at five minutes,
read the `Save point ... in N ms` lines for an hour and the disconnect reasons in the
server log.

Expected: every duration under one second; no disconnect attributed to a save.

## Scenario 6: late joiner and never-returning holder

1. Resume with one client absent. Release the hold. Connect the third client.
2. Ten minutes later, reconnect the absent client.

Expected: the third client is a spectator (or takes a free slot if the admin allows
it); the absent holder's slot stays theirs the whole time and they are put back
into their body on return.
