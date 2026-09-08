// The stage menus extend MenuBase, not MenuRootBase, so they miss the vanilla calls that
// keep exactly one chat panel visible; a player who had a character would see the HUD
// chat and the menu's panel at once.
class LL_MenuChat
{
	// Hides the gameplay HUD overlay.
	static void ShowOwnPanel(SCR_ChatPanel own)
	{
		if (!own)
			return;

		SCR_ChatPanelManager mgr = SCR_ChatPanelManager.GetInstance();
		if (!mgr)
			return;

		mgr.HideAllChatPanels();
		mgr.ShowChatPanel(own);
	}

	// Deferred a frame so the closing menu has left the stack, as vanilla does.
	static void NotifyMenuClosed()
	{
		SCR_ChatPanelManager mgr = SCR_ChatPanelManager.GetInstance();
		if (!mgr)
			return;

		GetGame().GetCallqueue().CallLater(mgr.OnMenuClosed, 1);
	}
}