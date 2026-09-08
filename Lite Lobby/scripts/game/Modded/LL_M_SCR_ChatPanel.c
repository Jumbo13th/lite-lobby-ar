// Hides the chat feed for alive, slotted players during GAME when the "Disable text
// chat" option is on; admins and spectators keep it. Display-side only: this is the
// panel's own redraw of the message lines, and it runs per redraw.
modded class SCR_ChatPanel : ScriptedWidgetComponent
{
	override protected void UpdateChatMessages()
	{
		if (LL_ShouldShowChat())
		{
			super.UpdateChatMessages();
			return;
		}

		for (int i = 0; i < m_iMessageLineCount; i++)
		{
			if (i < 0 || i > m_aMessageLines.Count() - 1)
				continue;

			SCR_ChatMessageLineComponent lineComp = m_aMessageLines[i];
			if (lineComp)
				lineComp.SetVisible(false);
		}
	}

	protected bool LL_ShouldShowChat()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();

		if (!mode || !mode.IsChatDisabled())
			return true;

		if (mode.GetState() != SCR_EGameModeState.GAME)
			return true;

		if (SCR_Global.IsAdmin())
			return true;

		LL_SpectatorManager spectator = LL_SpectatorManager.GetInstance();
		if (spectator && spectator.IsSpectating())
			return true;

		return false;
	}
}