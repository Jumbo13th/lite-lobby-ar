// Tames the vanilla VoN HUD popup. Menu speakers: incoming elements are hidden (the
// voice panel already shows who talks, and Game Master would be littered with lobby
// chatter); outgoing keeps the transmit indicator but drops the proxy's meaningless
// service frequency. Two game-mode options: hide incoming proximity speech entirely,
// and blank who is talking on radio while keeping the frequency.

modded class SCR_VonDisplay
{
	override protected bool UpdateTransmission(TransmissionData data, BaseTransceiver radioTransceiver, int frequency, bool IsReceiving)
	{
		if (IsReceiving && data)
		{
			if (data.m_iPlayerID > 0 && SCR_VoNComponent.LL_IsMenuSpeaker(data.m_iPlayerID))
				return false;

			if (!radioTransceiver && LL_HideProximityVon())
				return false;
		}

		bool result = super.UpdateTransmission(data, radioTransceiver, frequency, IsReceiving);

		// Outgoing carries no player id; the sender is the local player.
		if (!IsReceiving && data && data.m_Widgets && data.m_Widgets.m_wFrequency)
		{
			PlayerController pc = GetGame().GetPlayerController();
			if (pc && SCR_VoNComponent.LL_IsMenuSpeaker(pc.GetPlayerId()))
				data.m_Widgets.m_wFrequency.SetVisible(false);
		}

		if (result && IsReceiving && radioTransceiver && data && !data.m_bIsAdditional
			&& data.m_Widgets && LL_HideRadioSpeaker())
			LL_BlankIncomingSpeakerIdentity(data);

		return result;
	}

	// Leaves the radio icon and frequency visible.
	protected void LL_BlankIncomingSpeakerIdentity(TransmissionData data)
	{
		if (data.m_Widgets.m_wName)
		{
			data.m_Widgets.m_wName.SetText(string.Empty);
			data.m_Widgets.m_wName.SetVisible(false);
		}

		if (data.m_Widgets.m_wRole)
		{
			data.m_Widgets.m_wRole.SetText(string.Empty);
			data.m_Widgets.m_wRole.SetVisible(false);
		}

		if (data.m_Widgets.m_wSquadLeaderIcon)
			data.m_Widgets.m_wSquadLeaderIcon.SetVisible(false);

		if (data.m_Widgets.m_wPlatformImage)
			data.m_Widgets.m_wPlatformImage.SetVisible(false);

		if (data.m_Widgets.m_wPlatformImageGlow)
			data.m_Widgets.m_wPlatformImageGlow.SetVisible(false);

		if (data.m_Widgets.m_wGameMaster)
			data.m_Widgets.m_wGameMaster.SetVisible(false);
	}

	protected bool LL_HideProximityVon()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		return mode && mode.HideProximityVonUI();
	}

	protected bool LL_HideRadioSpeaker()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		return mode && mode.HideRadioSpeakerUI();
	}
}