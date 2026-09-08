# Quickstart: validating Player List Order

## Prerequisites

- The addon compiled in Workbench with the three changed scripts.
- The demo scenario `Missions/LobbyDemo.conf` on a dedicated server (join and leave
  behaviour is not trustworthy on a listen host).
- At least three clients with profile names chosen to cover the cases:
  `bravo`, `Alpha`, `[TT]Jumbo`, `[TT]Jidai`, `[ABC]Zed`, `charlie` (six clients are
  ideal; three are enough if names are changed between runs).

## Scenario 1: order on open (US1, FR-001 to FR-003, FR-006)

1. Connect all clients, open the lobby on each.
2. Read the player panel top to bottom.

Expected: `[ABC]Zed`, `[TT]Jidai`, `[TT]Jumbo`, `Alpha`, `bravo`, `charlie`, identical
on every client. No untagged name above a tagged one.

## Scenario 2: late join (US2, FR-004)

1. Keep the lobby open on one client.
2. Connect another client named `Mike`.

Expected: `Mike` appears between `charlie` and nothing, or between the names before
and after it, not at the bottom; the row appears once.

## Scenario 3: name arrives after the row (US2, FR-004)

1. Open the lobby on a client at the moment another client is still connecting.
2. Watch the new row.

Expected: if the row shows first with an empty name, it moves to its alphabetical place
as soon as the name is known.

## Scenario 4: rebuild (US2, FR-004, FR-005)

1. Open the lobby, hover a row so it is highlighted.
2. Trigger a full rebuild (leave and re-enter the lobby menu; or let a roster re-sync
   happen after a client times out).

Expected: same order as before the rebuild.

## Scenario 5: disconnect keeps place (FR-010)

1. Let one client time out (kill its process) while others keep the lobby open.

Expected: the row greys out and stays where it was.

## Scenario 6: search over the ordered list (FR-009)

1. Type part of a name in the search box.

Expected: only matching rows remain visible and they keep their relative order; the
"no matches" hint behaves as before.

## Scenario 7: scale and traffic (SC-002, SC-004)

1. Fill the server to 127 (bots are not players; this needs a real event or a load
   test with many clients).
2. Open the lobby and watch for a hitch; compare per-client traffic with the previous
   build using the server's network statistics.

Expected: no visible hitch on open or on later joins; traffic unchanged.

## Sign-off

Scenarios 1, 2, 5 and 6 are the minimum on a dedicated server. Scenario 7 is run at the
next full event and recorded in the feature's tasks before the feature is called done.
