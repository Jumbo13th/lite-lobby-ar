# Data Model: Map Line Drawing

## Map line (server)

Held by the server line store, owned by `LL_LobbyManager` on the authority.

| Field | Type | Rule |
|-------|------|------|
| id | int | Server counter, starts at 1, never reused in a session; continues above the highest restored id after a resume. Also the stacking order. |
| factionKey | FactionKey | The author's engine faction when the line was accepted. Never changes. |
| authorUid | string | Identity id of the author. Server only; never sent to a client. |
| authorName | string | Player name when the line was accepted. |
| colour | int | Index into the placed-marker colour list. |
| width | float | Ground width in metres, 0.1 m to 50 m. |
| points | array<int> | Decimetres, x0, y0, x1, y1, ...; 4 to 128 numbers (2 to 64 points), all inside the world's bounding box. |
| createdAt | WorldTimestamp | Server time of acceptance. |

**Limits**: 64 points per line, 20 lines per author identity, 150 lines per faction. One
accepted line per author identity every 2 seconds (last accepted time per identity).

**Lifecycle**: accepted → stored → delivered to the faction → deleted by any player of its faction who may place markers,
or dropped with the mission. A session resume restores it with its id, author, colour, width,
points and age.

## Line sync job (server)

One pending delivery of a faction's lines to one player.

| Field | Type | Rule |
|-------|------|------|
| playerId | int | Recipient. The job is dropped when the player disconnects or a newer sync starts for them. |
| lineIds | array<int> | Ids still to send, ascending. Lines deleted meanwhile are skipped. |

Processed on one 250 ms tick: at most 10 lines per job and 100 in total per tick.

## Map line (client)

Held by a client-side cache fed only by owner RPCs on the local player's `LL_LobbyPlayerComponent`.

| Field | Type | Rule |
|-------|------|------|
| id | int | Key; a second delivery of the same id is ignored. |
| colour | int | As on the server. |
| width | float | As on the server. |
| points | array<int> | As on the server. |
| createdAt | WorldTimestamp | `GetServerTimestamp()` at receipt minus the delivered age. |
| authorName | string | Shown with the marker-info key. |
| own | bool | The local player drew it (decided by the server per recipient). |
| bboxMin / bboxMax | vector | Ground bounding box, computed on receipt, for culling and hit tests. |

Kept sorted by id. Cleared by the reset RPC, never by the client on its own.

## Line in progress (client only)

| Field | Type | Rule |
|-------|------|------|
| points | array<int> | Decimetres; 1 point after Ctrl + click, at most 64. |
| width | float | Set from the zoom at each click, sent on Enter (research R2). |
| colour | int | The last confirmed colour; replaced by the double-click dialog's choice on OK. |

Discarded on Escape, when the map closes, or when the player loses the right to draw.

## Persistent record (session resume)

`LL_MapLineRecord`, written by `LL_MapLineSerializer` under version 1: id, factionKey, authorUid,
authorName, colour, width, points, age (seconds before the snapshot). An unknown version drops the
lines with a warning, as markers do.
