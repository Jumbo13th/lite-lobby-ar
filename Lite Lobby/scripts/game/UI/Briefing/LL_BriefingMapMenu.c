// Briefing screen: fullscreen map, stage header, mission description panel, voice panel
// and chat. Opened and closed by LL_LobbyPlayerComponent.SwitchToMenu.

modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	BriefingMapMenu,
	GameMapMenu
}

class LL_BriefingMapMenu : MenuBase
{
	// A standalone curated SCR_MapConfig rather than vanilla MapFullscreen.conf, whose
	// contents change between updates and lack the radial. LL_MapCursorModule scrolls
	// panels with the wheel instead of zooming.
	protected ResourceName m_sMapConfig = "{DC07E80F6BB397EF}Configs/Map/LL_MapBriefing.conf";

	protected SCR_MapEntity m_MapEntity;
	protected LL_GameModeCoop m_GameModeCoop;
	protected LL_LobbyManager m_LobbyManager;
	protected InputManager m_InputManager;
	protected int m_iPlayerId;

	protected LL_GameModeHeader m_GameModeHeader;
	protected SCR_ChatPanel m_ChatPanel;
	protected SCR_InputButtonComponent m_NavigationCloseComp;
	protected SCR_InputButtonComponent m_NavigationChatComp;
	protected SCR_InputButtonComponent m_NavigationDescriptionComp;
	protected SCR_InputButtonComponent m_NavigationVoiceChannelsComp;
	protected Widget m_wPlayableNotSelectedOverlay;
	protected Widget m_wMissionDescriptionPanel;
	protected Widget m_wVoiceChannelsPanel;

	protected bool m_bSwitchDescriptionQueued;
	protected bool m_bSwitchVoiceQueued;

	override void OnMenuInit()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}

	override void OnMenuOpen()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Close();
			return;
		}

		m_GameModeCoop = LL_GameModeCoop.GetInstance();
		m_LobbyManager = LL_LobbyManager.GetInstance();
		m_InputManager = GetGame().GetInputManager();

		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
			m_iPlayerId = pc.GetPlayerId();

		Widget root = GetRootWidget();

		Widget wHeader = root.FindAnyWidget("GameModeHeader");
		if (wHeader)
			m_GameModeHeader = LL_GameModeHeader.Cast(wHeader.FindHandler(LL_GameModeHeader));
		if (m_GameModeHeader)
			m_GameModeHeader.SetHostStage(SCR_EGameModeState.BRIEFING);

		Widget wChatPanel = root.FindAnyWidget("ChatPanel");
		if (wChatPanel)
			m_ChatPanel = SCR_ChatPanel.Cast(wChatPanel.FindHandler(SCR_ChatPanel));

		LL_MenuChat.ShowOwnPanel(m_ChatPanel);

		Widget wNavClose = root.FindAnyWidget("NavigationClose");
		if (wNavClose)
			m_NavigationCloseComp = SCR_InputButtonComponent.Cast(wNavClose.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationCloseComp)
			m_NavigationCloseComp.m_OnActivated.Insert(Action_Exit);

		Widget wNavChat = root.FindAnyWidget("NavigationChat");
		if (wNavChat)
			m_NavigationChatComp = SCR_InputButtonComponent.Cast(wNavChat.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationChatComp)
			m_NavigationChatComp.m_OnActivated.Insert(Action_ChatOpen);

		m_InputManager.AddActionListener("ChatToggle", EActionTrigger.DOWN, Action_ChatOpen);

		// A hide button on the panel itself could not restore an already-hidden panel.
		m_wMissionDescriptionPanel = root.FindAnyWidget("MissionDescriptionPanel");

		Widget wNavDescription = root.FindAnyWidget("NavigationDescription");
		if (wNavDescription)
			m_NavigationDescriptionComp = SCR_InputButtonComponent.Cast(wNavDescription.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationDescriptionComp)
			m_NavigationDescriptionComp.m_OnActivated.Insert(Action_SwitchDescription);

		m_InputManager.AddActionListener("LL_SwitchDescription", EActionTrigger.DOWN, Action_SwitchDescription);

		m_wVoiceChannelsPanel = root.FindAnyWidget("VoiceChannelsPanel");

		Widget wNavVoiceChannels = root.FindAnyWidget("NavigationVoiceChannels");
		if (wNavVoiceChannels)
			m_NavigationVoiceChannelsComp = SCR_InputButtonComponent.Cast(wNavVoiceChannels.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationVoiceChannelsComp)
			m_NavigationVoiceChannelsComp.m_OnActivated.Insert(Action_SwitchVoice);

		m_InputManager.AddActionListener("LL_SwitchVoice", EActionTrigger.DOWN, Action_SwitchVoice);

		// Gameplay contexts are inactive while a menu is open.
		m_InputManager.AddActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_LobbyKeyToggle);

		m_fOpenedTime = GetGame().GetWorld().GetWorldTime();

		m_wPlayableNotSelectedOverlay = root.FindAnyWidget("PlayableNotSelectedOverlay");
		if (m_LobbyManager)
		{
			m_LobbyManager.GetOnPlayerAssigned().Insert(OnPlayerSlotChanged);
			m_LobbyManager.GetOnPlayerUnassigned().Insert(OnPlayerSlotChanged);
		}
		UpdateNotSelectedOverlay();

		// The map fires these once OpenMap has attached and laid out the frame.
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapOpen().Insert(OnMapOpenedSquadMarkers);
			m_MapEntity.GetOnMapClose().Insert(OnMapClosedSquadMarkers);
		}

		// The map widget tree needs a couple of frames before SCR_MapEntity can attach.
		if (m_MapEntity)
			GetGame().GetCallqueue().CallLater(OpenMapStep1, 0);
		else
			Print("[LL_Lobby] Briefing: no SCR_MapEntity in world — map disabled", LogLevel.WARNING);
	}

	override void OnMenuClose()
	{
		LL_MenuChat.NotifyMenuClosed();

		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapOpen().Remove(OnMapOpenedSquadMarkers);
			m_MapEntity.GetOnMapClose().Remove(OnMapClosedSquadMarkers);
		}
		LL_SpawnMarkers squadMarkers = LL_SpawnMarkers.GetInstance();
		if (squadMarkers)
			squadMarkers.CloseOnMap();

		if (m_MapEntity)
			m_MapEntity.CloseMap();

		if (m_NavigationCloseComp)
			m_NavigationCloseComp.m_OnActivated.Remove(Action_Exit);
		if (m_NavigationChatComp)
			m_NavigationChatComp.m_OnActivated.Remove(Action_ChatOpen);
		if (m_NavigationDescriptionComp)
			m_NavigationDescriptionComp.m_OnActivated.Remove(Action_SwitchDescription);
		if (m_NavigationVoiceChannelsComp)
			m_NavigationVoiceChannelsComp.m_OnActivated.Remove(Action_SwitchVoice);

		GetGame().GetInputManager().RemoveActionListener("ChatToggle", EActionTrigger.DOWN, Action_ChatOpen);
		GetGame().GetInputManager().RemoveActionListener("LL_SwitchDescription", EActionTrigger.DOWN, Action_SwitchDescription);
		GetGame().GetInputManager().RemoveActionListener("LL_SwitchVoice", EActionTrigger.DOWN, Action_SwitchVoice);
		GetGame().GetInputManager().RemoveActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_LobbyKeyToggle);

		if (m_LobbyManager)
		{
			m_LobbyManager.GetOnPlayerAssigned().Remove(OnPlayerSlotChanged);
			m_LobbyManager.GetOnPlayerUnassigned().Remove(OnPlayerSlotChanged);
		}
	}

	override void OnMenuUpdate(float tDelta)
	{
		// Input comes from the preset's ActionContext; force-activating another
		// priority-50 context every frame would suppress the map actions.

		if (m_ChatPanel)
			m_ChatPanel.OnUpdateChat(tDelta);

		if (m_GameModeHeader)
			m_GameModeHeader.TryUpdate();

		LL_SpawnMarkers squadMarkers = LL_SpawnMarkers.GetInstance();
		if (squadMarkers)
			squadMarkers.UpdateOnMap(m_MapEntity);
	}

	protected void OnMapOpenedSquadMarkers(MapConfiguration config)
	{
		LL_SpawnMarkers markers = LL_SpawnMarkers.GetInstance();
		if (markers)
			markers.OpenOnMap(m_MapEntity);
	}

	protected void OnMapClosedSquadMarkers(MapConfiguration config)
	{
		LL_SpawnMarkers markers = LL_SpawnMarkers.GetInstance();
		if (markers)
			markers.CloseOnMap();
	}

	protected void OpenMapStep1()
	{
		// SetupMapConfig reads the layout's MapFrame, laid out two frames after menu open.
		GetGame().GetCallqueue().CallLater(OpenMapStep2, 0);
	}

	protected void OpenMapStep2()
	{
		if (!m_MapEntity)
			return;

		MapConfiguration mapConfig = m_MapEntity.SetupMapConfig(EMapEntityMode.FULLSCREEN, m_sMapConfig, GetRootWidget());
		if (!mapConfig)
		{
			Print("[LL_Lobby] Briefing: map config failed to load", LogLevel.WARNING);
			return;
		}

		m_MapEntity.OpenMap(mapConfig);
		GetGame().GetCallqueue().CallLater(ZoomOutStep1, 0);
	}

	protected void ZoomOutStep1()
	{
		// Zoom only applies after the map canvas exists.
		GetGame().GetCallqueue().CallLater(ZoomOutStep2, 0);
	}

	protected void ZoomOutStep2()
	{
		if (m_MapEntity)
			m_MapEntity.ZoomOut();
	}

	// Lazy: on dedicated-server clients this menu can open before the player id is
	// assigned. Ids start at 1.
	protected int GetLocalPlayerId()
	{
		if (m_iPlayerId <= 0)
		{
			PlayerController pc = GetGame().GetPlayerController();
			if (pc)
				m_iPlayerId = pc.GetPlayerId();
		}

		return m_iPlayerId;
	}

	protected void OnPlayerSlotChanged(int playerId, int slotRplId)
	{
		if (playerId != GetLocalPlayerId())
			return;

		UpdateNotSelectedOverlay();
	}

	protected void UpdateNotSelectedOverlay()
	{
		if (!m_wPlayableNotSelectedOverlay)
			return;

		bool hasSlot = m_LobbyManager && m_LobbyManager.FindSlotByPlayerId(GetLocalPlayerId());
		m_wPlayableNotSelectedOverlay.SetVisible(!hasSlot);
	}

	protected void Action_Exit()
	{
		// A mandatory stage screen: "back" brings the pause menu up on top.
		GetGame().GetCallqueue().CallLater(OpenPauseMenuWrap, 0);
	}

	protected void OpenPauseMenuWrap()
	{
		ArmaReforgerScripted.OpenPauseMenu();
	}

	protected void Action_ChatOpen()
	{
		// Opening chat inline from an input event fights the menu's own input handling.
		GetGame().GetCallqueue().CallLater(ChatOpenWrap, 0);
	}

	protected void ChatOpenWrap()
	{
		if (m_ChatPanel)
			SCR_ChatPanelManager.GetInstance().OpenChatPanel(m_ChatPanel);
	}

	// Debounced: the key reaches this handler through both the direct listener and the
	// footer button's bound action in one frame.
	protected void Action_SwitchDescription()
	{
		if (m_bSwitchDescriptionQueued)
			return;

		m_bSwitchDescriptionQueued = true;
		GetGame().GetCallqueue().CallLater(SwitchDescriptionImpl, 0, false);
	}

	protected void SwitchDescriptionImpl()
	{
		m_bSwitchDescriptionQueued = false;

		if (m_wMissionDescriptionPanel)
			m_wMissionDescriptionPanel.SetVisible(!m_wMissionDescriptionPanel.IsVisible());
	}

	// Debounced like the description toggle.
	protected void Action_SwitchVoice()
	{
		if (m_bSwitchVoiceQueued)
			return;

		m_bSwitchVoiceQueued = true;
		GetGame().GetCallqueue().CallLater(SwitchVoiceImpl, 0, false);
	}

	protected void SwitchVoiceImpl()
	{
		m_bSwitchVoiceQueued = false;

		if (m_wVoiceChannelsPanel)
			m_wVoiceChannelsPanel.SetVisible(!m_wVoiceChannelsPanel.IsVisible());
	}

	// U key while the briefing is open — admin-only local dismiss (same as lobby).
	protected void Action_LobbyKeyToggle()
	{
		// Still the tail of the U hold that opened the menu: both contexts list
		// LL_OpenLobby, so the hold never deactivates across the handover.
		if (GetGame().GetWorld().GetWorldTime() - m_fOpenedTime < 600)
			return;

		if (!SCR_Global.IsAdmin())
			return;

		Close();
	}

	protected float m_fOpenedTime;
}