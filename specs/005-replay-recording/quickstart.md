# Quickstart: validating Replay Recording

## Prerequisites

- The addon compiled in Workbench with the new scripts and the edited
  `LL_Persistence.conf` ([contracts/config-files.md](contracts/config-files.md)).
- `LL_ReplayRecorderComponent` added to `Prefabs/MP/Modes/Editor/LL_GameMode_Lobby.et`
  by the operator in Workbench.
- In the demo world (`Missions/LobbyDemo.conf`), placed by the operator:
  - one weapon crate with an M72 or RPG-7 with PG-7VM and PG-7VL rockets, a grenade
    launcher, and hand grenades;
  - a hide delay of 30 seconds on the NeutralZone map area;
  - unconsciousness allowed by the demo game mode's health settings.
- A **dedicated server** with the demo scenario and `$profile:LL_Replays.json` containing
  `{ "Enabled": true }`. Session saves are already on in the demo world.
- Three clients: two on opposite sides, one of them admin, and one for the reconnect case.
- For the service: `go` installed, the `uplink/` module checked out.

Every scenario ends with the server log excerpt and the recording files handed back.

## Scenario 0: off means off (SC-011, FR-001)

1. Remove `$profile:LL_Replays.json`. Start the server, play the game phase and
   stop the server.
2. Expected: no `LL_Replays` folder, no `[LL_Lobby] Replay` log line, and `/replay` as
   admin answers "Replay recording is not enabled on this server."

## Scenario 1: spikes (run before the code that depends on each)

Record each observed result in [research.md](research.md), whatever it is.

| Spike | Steps | Pass |
|-------|-------|------|
| S1 AI shots | A Game Master spawns an AI group of a side without slots (for example FIA) within sight of a player. Count the AI's shot events against its magazines. Also log the player id the events carry. | events arrive for AI; otherwise the `OnAmmoCountChanged` fallback is taken |
| S2 detonations | Throw two grenades, fire a PG-7VM and a PG-7VL, fire two 40 mm grenades. | each recorded detonation is within 2 m of the visible impact; PG-7VM and 40 mm come from the explosion event |
| S3 knock-out | Knock a character out with a rifle; read the damage manager's last instigator at once and one frame later. | the attacker is known by the time it is read |
| S4 world facts | Compare the vehicles registered at game start with the demo world's placed vehicles; drive a car due north and due east; delete a fresh file and let the recorder append. | all vehicles present; headings read 0 and 90; append creates the file or the exists check handles it |
| S5 names | Read a vehicle's display name and a weapon name on the server. | keys translate (English written) or stay keys (keys written, prefab names beside them) |
| S6 shot callback | In S1, check whether the projectile is still readable in the delayed callback. | direction comes from the projectile; otherwise from the shooter's heading |

## Scenario 2: a recorded game (US1, SC-005, SC-006, SC-012)

1. With the recorder on, an admin advances to the game phase. Check that
   `$profile:LL_Replays/<session>-1.llr` appears with the header and a snapshot, and that
   `status.txt` reads `rec`.
2. Within freeze time, place a marker for one side and remove it; end freeze with
   `/endfreeze`.
3. Kill an inert enemy body with a rifle at about 150 m; kill a team mate; knock a player
   out with a rifle and revive them with a bandage or a medic kit; throw a grenade; fire a
   rocket at a crewed BTR-70 with a driver and a gunner until it is destroyed; capture the
   Road zone; wait for the NeutralZone area to disappear 30 s after freeze end; disconnect
   and reconnect the third client.
4. Stop the server cleanly. Check that the file ends with `STOP shutdown`, then start the
   next mission and check that `status.txt` names a new session.
5. Open the file in a text editor and find every event of step 2 and 3 with the right
   time (within 1 s), actors and position (within 2 m). Check that every kill event in
   `$profile:LL_GameStats/<session>-final.json` has a `K` record with the same victim
   within 1 s of `10 × t`.
6. Cut a copy of the file at a random byte and check that a reader stops at the last
   complete line.

## Scenario 3: a crash and a resume (US2, SC-004, FR-022)

1. During the game phase, after one automatic snapshot (`/snapshot` forces one), kill the
   server process (not a graceful stop).
2. Check that the last record in the segment is at most 10 s older than the kill and that
   the file has neither `STOP` nor `END`; `status.txt` still reads `rec`.
3. Start the server with the game's resume parameter. Check that `status.txt` reads
   `resume` while the hold runs, then `rec` with segment 2, and that
   `<session>-2.llr` begins with a header carrying `H resumed <t>` and a snapshot within
   10 s of the game continuing.
4. Stop the server gracefully. Check that segment 2 ends with `STOP shutdown`.
5. Start it plainly. Check that the next game opens a new session and that segment 2 is
   untouched.

## Scenario 4: the service (US3, SC-007, SC-008, SC-009, FR-028 to FR-038)

1. `cd uplink && go test ./...` passes. The suite runs a protocol receiver in the test
   process and covers: the signature test vector, a refused request without or with a
   wrong signature or an old timestamp, idempotent pieces and commits, resume after a
   receiver outage, `429` handling, the rate cap over 10-second windows, the game-over rule
   for every status-file state, retention, and a config file readable by others being
   refused.
2. On the test host, run `ll-uplink -config <file>` against the receiving website (when
   it exists). Play a game, then start the next mission. Check that the website holds the
   segment, the identity file and the latest statistics within 2 minutes.
3. While a game runs, check that the website holds nothing of it.
4. Kill the game server mid-game and leave it stopped longer than `StoppedWaitMinutes`.
   Check that the cut-off recording is then sent as `abandoned`. Repeat, resuming within
   the wait: nothing is sent until the resumed game ends.
5. `ss -ltnp` lists no socket of the service. The service log contains no secret. The
   metrics file shows the last success time.

## Scenario 5: nothing private in public files (US4, SC-010)

1. Search every `.llr` file of Scenario 2 for identity GUIDs
   (`[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}`), IP addresses and the
   configured secret. Expected: none; the GUIDs are only in `<session>-identities.txt`.
2. In the service state, check that no file identity was committed with two different
   sizes or contents.

## Scenario 6: `/replay` (US5, FR-039, FR-040)

1. As admin, type `/replay` before the game phase (ready), and during it (recording, with
   growing minutes and size), in English and in Russian.
2. As a non-admin, type `/replay`: the client says it is for admins, and the server log
   shows no reply sent.
3. Make the profile folder read-only during a game: the next `/replay` reports the
   failure, the log has one line about it, and the game goes on.

## Scenario 7: recording changes nothing in play (FR-025)

1. On the test server in its usual configuration, play the same scripted fight with
   recording off, then on. Expected: weapons, ammunition, explosions and vehicles behave
   the same in both runs, and the log has no script error from the recorder.

## Scenario 8: scale (SC-001, SC-002, SC-003)

On the event server with 127 players and at least 100 AI, for one game with recording on
and one comparable game with it off:

1. The recorder's five-minute summary lines show the longest tick under 2 ms and the
   average work under 3 ms per second (summed flush and tick time over the period).
2. The server's `-logStats` frame-time lines show an average frame rate within 2% of the
   game without recording.
3. Per-client network traffic matches the game without recording.
4. The two-hour recording is at most 60 MB on disk and at most 10 MB compressed.

A failed measurement stays failed until it passes or the operator changes the criterion.
