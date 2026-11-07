# Research: Game Master Only Player Notices

## R1. Where the game sends the four notices

**Finding**: `SCR_NotificationSenderComponent` (1.9.0.37,
`scripts/Game/GameMode/SCR_NotificationSenderComponent.c`) is the only sender of
`PLAYER_JOINED`, `PLAYER_LEFT`, `PLAYER_KICKED`, `PLAYER_BANNED` and
`PLAYER_BANNED_NO_DURATION`; a search of `scripts/` for those values finds no other
caller.

- `OnPlayerRegistered(int playerId)` runs on every machine and calls
  `SCR_NotificationsComponent.SendLocal(ENotification.PLAYER_JOINED, playerId)`: each
  client shows the join to its own player, nothing crosses the network.
- `OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)` runs on the
  server. It sends `EDITOR_GM_LEFT` to everyone when the player held Game Master
  rights, resolves kick or ban from `m_PlayerKickReasonsConfig` and the data
  collector's ban timeout, and sends the kick, ban or left notice with
  `SendToEveryone`, one owner RPC per connected player.

`LL_GameMode_Lobby.et` carries the component with the kill feed disabled; Lite Lobby
has no override of it and no join or leave notice of its own.

## R2. The game's Game Master only delivery

**Finding**: the notifications component already has both halves.

- `SendToUnlimitedEditorPlayers` sends only to players whose editor manager is not
  limited; the game uses it for `PLAYER_JOINED_FACTION` in the same component.
- `SendLocalUnlimitedEditor` shows a local notice only when the local editor manager
  is not limited; the game uses it for the possessed-AI kill feed lines.
- A notice sent either way is marked `GM_ONLY` or `LOCAL_GM_ONLY`, and
  `SCR_NotificationMessageUIComponent` shows its Game Master only widgets for it.
- An editor manager that is not limited is exactly what the game calls the Game
  Master role: `SCR_EditorManagerEntity.UpdateLimited` gives or clears
  `EPlayerRole.GAME_MASTER` from the same flag. The flag is about rights, not about the
  editor being open.

The kick and ban display (`SCR_NotificationPlayerKicked`) builds its text from the
parameters only, so the Game Master sees the same text the game showed everyone.

## R3. Hook choice

**Decision**: a modded `SCR_NotificationSenderComponent` overriding
`OnPlayerRegistered` and `OnPlayerDisconnected`. In a lobby mission the join goes
through `SendLocalUnlimitedEditor` and the disconnect body is the game's own with
`SendToEveryone` replaced by `SendToUnlimitedEditorPlayers` for the kick, ban and left
notices. Without a lobby game mode both overrides call `super`.

**Alternatives rejected**:

- Filter on the receiving side, in the notifications component: every disconnect would
  still be sent to every player, which FR-007 forbids and which keeps the cost the
  feature removes.
- Override the static `SendToEveryone` and redirect by notification id: it is an
  overloaded static helper that every system calls, and no official script overrides
  one of its kind with a call to the original, so it is not known to work
  (constitution I).

**Cost of the choice**: the disconnect body is a copy of the game's. Each game-version
audit compares it with the vanilla method, as it does for the other copied overrides.
