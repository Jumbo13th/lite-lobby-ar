# Quickstart: validating AI Spotting Markers Switch

## Prerequisites

- The addon compiled in Workbench with the two changed scripts.
- The demo scenario `Missions/LobbyDemo.conf` on a dedicated server; the join scenario
  is not trustworthy on a listen host.
- Two clients on opposite factions, plus a third that joins later. The demo world
  already places one stock rifle squad per faction; leave "Remove AI units not occupied
  by players" unticked so they stay.

## Scenario 1: default keeps the map clean (US1, FR-001 to FR-003)

1. Start the mission with the game-mode checkbox at its default (off). Slot both
   clients, start the game, end the freeze.
2. Walk one client into plain view of the other faction's AI squad, within 200 m, and
   stay in sight for two minutes. Let the AI fire if it wants; the point is that it
   identifies the player.
3. Open the map on both clients.

Expected: no military symbol with an "N minutes ago" label anywhere on either map.

## Scenario 2: join-in-progress stays clean (US1, FR-003)

1. Continue Scenario 1 for five more minutes.
2. Connect the third client, slot it on either faction, open the map.

Expected: no such marker on the third client either.

## Scenario 3: vehicles (US1)

1. Repeat Scenario 1 with the watched player driving a vehicle past the AI squad.

Expected: no motorised symbol appears.

## Scenario 4: other markers unaffected (US1, FR-006, SC-003)

1. With the switch off, place a player marker, check a squad leader's marker, display
   a mission-maker marker and a tracker if the world has one.

Expected: each behaves exactly as in the previous build, shown to the same players.

## Scenario 5: opting in restores the game's behaviour (US2, FR-004, FR-005)

1. Operator: in Workbench, tick the new checkbox on the `LL_GameMode_Lobby` entity in
   the demo world (or on the prefab), save, restart the server.
2. Repeat Scenario 1.

Expected: within a minute of the AI identifying the player, a military infantry
symbol of the AI squad's faction appears near the player on the map of the AI squad's
faction, with an age label, and none on the spotted player's map. Optional check of
FR-005: set the game's own report setting off on one faction's config and confirm
that faction's AI still places nothing while the other's does.

3. Operator: untick the checkbox again and save, so the demo world ships with the
   default.

## Scenario 6: traffic (SC-004)

1. With the switch off, compare per-client traffic during the game with the previous
   build using the server's network statistics, in a session where the AI has players
   in sight.

Expected: equal or lower.

## Sign-off

Scenarios 1, 2 and 5 are the minimum on a dedicated server. Scenario 6 is recorded at
the next full event before the feature is called done.
