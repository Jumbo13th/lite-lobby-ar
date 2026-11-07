// In a lobby mission only Game Masters are told who joined, left, was kicked or was
// banned; sending to them alone also costs one message per Game Master per disconnect
// instead of one per player. The disconnect body is the game's with only the recipients
// changed, because super would broadcast first. Without a lobby game mode the game
// keeps its own behaviour.

modded class SCR_NotificationSenderComponent
{
	override void OnPlayerRegistered(int playerId)
	{
		if (!LL_GameModeCoop.GetInstance())
		{
			super.OnPlayerRegistered(playerId);
			return;
		}

		SCR_NotificationsComponent.SendLocalUnlimitedEditor(ENotification.PLAYER_JOINED, playerId);
	}

	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (!LL_GameModeCoop.GetInstance())
		{
			super.OnPlayerDisconnected(playerId, cause, timeout);
			return;
		}

		if (!GetGameMode().IsMaster())
			return;

		SCR_EditorManagerEntity editorManager;
		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (core)
			editorManager = core.GetEditorManager(playerId);

		bool hasUnlimitedEditor = editorManager && !editorManager.IsLimited();

		if (hasUnlimitedEditor)
			SCR_NotificationsComponent.SendToEveryone(ENotification.EDITOR_GM_LEFT, playerId);

		bool isKickedOrBanned = false;

		if (m_PlayerKickReasonsConfig)
		{
			string groupId, reasonId;
			KickCauseGroup2 groupInt;
			int reasonInt;

			GetGame().GetFullKickReason(cause, groupInt, reasonInt, groupId, reasonId);
			SCR_ConfigurableDialogUiPreset preset = m_PlayerKickReasonsConfig.FindPreset(groupId + "_" + reasonId);

			isKickedOrBanned = preset != null && !preset.m_sMessage.IsEmpty();
		}
		else
		{
			Print("'SCR_NotificationSenderComponent' has no 'm_PlayerKickReasonsConfig'! Make sure it is added else it will never know if a player was kicked!", LogLevel.ERROR);
		}

		if (isKickedOrBanned || timeout != 0)
		{
			SCR_DataCollectorComponent dataCollector = GetGame().GetDataCollector();
			if (dataCollector)
			{
				SCR_PlayerData playerData = dataCollector.GetPlayerData(playerId);

				if (playerData)
				{
					float banTimeOut = playerData.GetTimeOut();

					if (banTimeOut > 0 && banTimeOut > timeout)
						timeout = banTimeOut;
				}
			}

			if (timeout == 0)
				SCR_NotificationsComponent.SendToUnlimitedEditorPlayers(ENotification.PLAYER_KICKED, playerId, KickCauseCodeAPI.GetGroup(cause), KickCauseCodeAPI.GetReason(cause), timeout);
			else if (timeout < 0)
				SCR_NotificationsComponent.SendToUnlimitedEditorPlayers(ENotification.PLAYER_BANNED_NO_DURATION, playerId, KickCauseCodeAPI.GetGroup(cause), KickCauseCodeAPI.GetReason(cause), timeout);
			else
				SCR_NotificationsComponent.SendToUnlimitedEditorPlayers(ENotification.PLAYER_BANNED, playerId, KickCauseCodeAPI.GetGroup(cause), KickCauseCodeAPI.GetReason(cause), timeout);
		}
		else if (m_bShowPlayerLeftNotification && !hasUnlimitedEditor)
		{
			SCR_NotificationsComponent.SendToUnlimitedEditorPlayers(ENotification.PLAYER_LEFT, playerId);
		}
	}
}
