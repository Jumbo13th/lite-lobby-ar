# Network Contract: Map Line Drawing

All on `LL_LobbyPlayerComponent`, reliable channel. Visibility: faction-scoped, decided on the
server; nothing is broadcast and nothing enters `RplSave`/`RplLoad`.

## Client → server

| RPC | Arguments | Server checks, in order | On failure |
|-----|-----------|------------------------|------------|
| `RpcAsk_AddMapLine` | `int colour, float width, array<int> points` | marker edit rule; points count even and 4..128; every point inside the world box; colour in the placed-marker list; width 0.1 m..50 m; author identity under 20 lines; faction under 150 lines; 2 s since the author's last accepted line | `RpcDo_OwnerMapLineRefused(reason)` to the requester; nothing stored |
| `RpcAsk_RemoveMapLine` | `int lineId` | marker edit rule; line exists; line faction is the requester's faction; requester is the author (identity) or `SCR_Global.IsAdmin` | silently ignored |

## Server → one client

| RPC | Arguments | When |
|-----|-----------|------|
| `RpcDo_OwnerAddMapLine` | `int lineId, int colour, float width, int ageSeconds, string authorName, bool own, array<int> points` | to every player of the faction when a line is accepted; inside a sync |
| `RpcDo_OwnerRemoveMapLine` | `int lineId` | to every player of the faction when a line is deleted |
| `RpcDo_OwnerResetMapLines` | none | first message of every sync, and alone when the player is left with no faction |
| `RpcDo_OwnerMapLineRefused` | `int reason` | a refused add |

Refusal reasons: 1 not allowed to draw now, 2 player line limit, 3 faction line limit, 4 too soon
after the previous line, 5 invalid line. Reason 6 (point limit) is raised by the client while drawing and never sent.

## Ordering

Owner RPCs from one component arrive in send order: a reset always precedes the lines of its
sync, and a live add sent before a reset is cleared by it. A line delivered both live and in a
sync is applied once (keyed by id).

## Size

`RpcDo_OwnerAddMapLine` is at most about 560 bytes. Drawing sends nothing; confirming sends one
request and one delivery per faction member; deleting likewise.
