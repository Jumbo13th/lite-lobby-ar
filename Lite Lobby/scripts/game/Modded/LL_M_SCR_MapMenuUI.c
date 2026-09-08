// Redirects the map key during GAME to the lobby's own map screen with the mission
// description beside it. Other states and other game modes keep the stock map.
modded class SCR_MapMenuUI
{
	override void OnMenuOpen()
	{
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && gameMode.GetState() == SCR_EGameModeState.GAME)
		{
			// Closing and opening menus inside this callback fights the menu manager.
			GetGame().GetCallqueue().CallLater(RedirectToGameMap, 0);
			return;
		}

		super.OnMenuOpen();
	}

	protected void RedirectToGameMap()
	{
		Close();
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.GameMapMenu);
	}
};