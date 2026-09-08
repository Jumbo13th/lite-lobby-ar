// Small modded vanilla classes with no file of their own.

// m_eGameState is private and vanilla only exposes StartGameMode and EndGameMode.
modded class SCR_BaseGameMode
{
	void SetGameModeState(SCR_EGameModeState state)
	{
		if (!IsMaster())
			return;

		m_eGameState = state;
		Replication.BumpMe();

		OnGameStateChanged();
	}
}
