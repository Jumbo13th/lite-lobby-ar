# Quickstart: Game Master Only Player Notices

Validation on a dedicated server running the demo scenario
(`Missions/LobbyDemo.conf`). No Workbench edit is needed.

Clients: **GM** holds Game Master rights (a logged-in admin; Y opens the editor),
**P1** and **P2** are ordinary players, **P3** is the player who joins and goes.

## Scenario 1 - Join and leave in the lobby (US1, US2)

1. GM and P1 are in slot selection, GM with the editor closed.
2. P3 joins. GM's feed shows "P3 joined", marked as a Game Master notice; P1's feed
   shows nothing.
3. P3 quits to the main menu. GM's feed shows "P3 left"; P1's shows nothing.

## Scenario 2 - Kick in the game and in spectator (US1, US2)

1. Start the game; P1 slotted and alive, P2 dead and in spectator.
2. P3 joins; GM kicks P3 from the admin player actions.
3. GM's feed shows the kick with its reason; P1 and P2 see nothing.

## Scenario 3 - Game Master leaves (FR-004)

1. With a second admin as GM, kick GM.
2. P1 sees "Game Master left" and no kick notice; the second admin sees both.

## Scenario 4 - Rights change mid-session (US2)

1. Take GM's Game Master rights away (Y no longer opens the editor), then let P3
   join: GM no longer sees it.
2. Give the rights back, let P3 leave: GM sees it.

## Scenario 5 - Without the lobby game mode (FR-006)

1. Run any vanilla scenario with the addon loaded; P3 joins and leaves.
2. P1 sees both notices, as in the unmodified game.
