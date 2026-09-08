// Shows the lobby's display-name rename on every vanilla surface (chat, player list,
// nametags, kill feed, VON display, group tiles, marker labels): all resolve names
// through this cache, and PlayerManager has no name setter. The stock path strips
// < > # before names reach RichText widgets; the lobby name takes another path, so it
// is stripped here too.

modded class SCR_PlayerNamesFilterCache
{
	override string GetPlayerDisplayName(int playerId)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			string name = mgr.GetPlayerName(playerId);
			if (name != "")
			{
				name.Replace("<", "");
				name.Replace(">", "");
				name.Replace("#", "");
				return name;
			}
		}

		return super.GetPlayerDisplayName(playerId);
	}
}