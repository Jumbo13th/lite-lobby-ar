// Keeps a disconnecting player's character alive. The base game mode deletes the
// controlled entity unless HandlePlayerDisconnect returns true, and deleting the body
// unregisters the slot for everyone. Nothing is stored: the lobby manager owns the
// reservation and re-possesses the same body, and stored data would schedule vanilla's
// expiry that deletes the body. The component must be enabled on the game-mode prefab;
// a disabled one never initialises and the base game mode deletes regardless.

modded class SCR_ReconnectComponent
{
	override bool HandlePlayerDisconnect(int playerId, KickCauseCode cause)
	{
		if (LL_GameModeCoop.GetInstance())
		{
			Print(string.Format("[LL_Lobby] Reconnect: keeping disconnected player %1's character alive", playerId), LogLevel.NORMAL);
			return true;
		}

		return super.HandlePlayerDisconnect(playerId, cause);
	}
}