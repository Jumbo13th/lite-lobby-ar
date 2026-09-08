// Header shared by the lobby and briefing screens: stage tabs, mission title and author,
// the admin advance button and the statistics button.

class LL_GameModeHeader : ScriptedWidgetComponent
{
	protected ButtonWidget m_wLobbyButton;
	protected ButtonWidget m_wBriefingButton;
	protected ButtonWidget m_wInGameButton;
	protected ButtonWidget m_wAdvanceButton;
	protected ImageWidget m_wAdvanceIcon;
	protected TextWidget m_wTitleText;
	protected RichTextWidget m_wAuthorText;
	protected ImageWidget m_wHeaderLine;

	protected LL_GameModeHeaderButton m_hButtonLobby;
	protected LL_GameModeHeaderButton m_hButtonBriefing;
	protected LL_GameModeHeaderButton m_hButtonInGame;

	protected LL_GameModeCoop m_GameModeCoop;
	protected bool m_bInitialized;
	protected SCR_EGameModeState m_eLastState;
	protected bool m_bLastIsAdmin;

	// Each stage menu embeds its own header, so the viewed stage is the hosting menu. Tab
	// clicks switch menus locally; the advance button changes the real state for everyone.
	protected SCR_EGameModeState m_eHostStage = SCR_EGameModeState.SLOTSELECTION;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wLobbyButton = ButtonWidget.Cast(w.FindAnyWidget("LobbyButton"));
		m_wBriefingButton = ButtonWidget.Cast(w.FindAnyWidget("BriefingButton"));
		m_wInGameButton = ButtonWidget.Cast(w.FindAnyWidget("InGameButton"));
		m_wAdvanceButton = ButtonWidget.Cast(w.FindAnyWidget("AdvanceButton"));
		m_wTitleText = TextWidget.Cast(w.FindAnyWidget("TitleText"));
		m_wAuthorText = RichTextWidget.Cast(w.FindAnyWidget("AuthorRichText"));
		m_wHeaderLine = ImageWidget.Cast(w.FindAnyWidget("GameModeHeaderLine"));

		if (m_wLobbyButton)
			m_hButtonLobby = LL_GameModeHeaderButton.Cast(m_wLobbyButton.FindHandler(LL_GameModeHeaderButton));
		if (m_wBriefingButton)
			m_hButtonBriefing = LL_GameModeHeaderButton.Cast(m_wBriefingButton.FindHandler(LL_GameModeHeaderButton));
		if (m_wInGameButton)
			m_hButtonInGame = LL_GameModeHeaderButton.Cast(m_wInGameButton.FindHandler(LL_GameModeHeaderButton));

		Widget statsButton = w.FindAnyWidget("StatsButton");
		if (statsButton)
		{
			SCR_ButtonBaseComponent statsComp = SCR_ButtonBaseComponent.Cast(
				statsButton.FindHandler(SCR_ButtonBaseComponent));
			if (statsComp)
				statsComp.m_OnClicked.Insert(OnStatsClicked);
			else
				Print("[LL_Lobby] Header: StatsButton has no SCR_ButtonBaseComponent — stale GameModeHeader.layout, re-index resources", LogLevel.WARNING);
		}
		else
		{
			Print("[LL_Lobby] Header: StatsButton widget not found — stale GameModeHeader.layout, re-index resources", LogLevel.WARNING);
		}

		if (m_wAdvanceButton)
		{
			m_wAdvanceIcon = ImageWidget.Cast(m_wAdvanceButton.FindAnyWidget("AdvanceIcon"));

			SCR_ButtonBaseComponent advanceComp = SCR_ButtonBaseComponent.Cast(
				m_wAdvanceButton.FindHandler(SCR_ButtonBaseComponent));
			if (advanceComp)
				advanceComp.m_OnClicked.Insert(OnAdvanceClicked);
		}

		// 'func' parameters are not supported in script methods, so no wiring helper.
		SCR_ButtonBaseComponent tabComp;
		if (m_wLobbyButton)
		{
			tabComp = SCR_ButtonBaseComponent.Cast(m_wLobbyButton.FindHandler(SCR_ButtonBaseComponent));
			if (tabComp)
				tabComp.m_OnClicked.Insert(OnLobbyTabClicked);
		}
		if (m_wBriefingButton)
		{
			tabComp = SCR_ButtonBaseComponent.Cast(m_wBriefingButton.FindHandler(SCR_ButtonBaseComponent));
			if (tabComp)
				tabComp.m_OnClicked.Insert(OnBriefingTabClicked);
		}
		if (m_wInGameButton)
		{
			tabComp = SCR_ButtonBaseComponent.Cast(m_wInGameButton.FindHandler(SCR_ButtonBaseComponent));
			if (tabComp)
				tabComp.m_OnClicked.Insert(OnGameTabClicked);
		}

		SetMissionInfo();

		m_GameModeCoop = LL_GameModeCoop.GetInstance();
		m_bInitialized = true;
	}

	// Called by the host menu right after it opens.
	void SetHostStage(SCR_EGameModeState stage)
	{
		if (m_eHostStage == stage)
			return;

		m_eHostStage = stage;
		UpdateTabs();
	}

	void TryUpdate()
	{
		if (!m_bInitialized || !m_GameModeCoop)
			return;

		// Admin login mid-session must light the advance button and unlock the tabs at once.
		SCR_EGameModeState currentState = m_GameModeCoop.GetLobbyState();
		bool isAdmin = SCR_Global.IsAdmin();
		if (currentState == m_eLastState && isAdmin == m_bLastIsAdmin)
			return;

		m_eLastState = currentState;
		m_bLastIsAdmin = isAdmin;

		// A real state change re-syncs the view by switching menus.
		UpdateTabs();
		UpdateAdvanceButton();
		UpdateProgressLine(currentState);
	}

	// Orange toggle = the stage this player views, check icon = the live state, white text
	// = clickable for this player.
	protected void UpdateTabs()
	{
		if (!m_GameModeCoop)
			return;

		SCR_EGameModeState actual = m_GameModeCoop.GetLobbyState();
		bool isAdmin = SCR_Global.IsAdmin();

		// The roster view is always reachable; the briefing tab is locked only during slot
		// selection and readable at any later stage, spectators included.
		bool lobbyAvailable = true;
		bool briefingAvailable = isAdmin
			|| actual != SCR_EGameModeState.SLOTSELECTION;

		if (m_hButtonLobby)
			m_hButtonLobby.UpdateVisuals(
				m_eHostStage == SCR_EGameModeState.SLOTSELECTION,
				actual == SCR_EGameModeState.SLOTSELECTION,
				lobbyAvailable);
		if (m_hButtonBriefing)
			m_hButtonBriefing.UpdateVisuals(
				m_eHostStage == SCR_EGameModeState.BRIEFING,
				actual == SCR_EGameModeState.BRIEFING,
				briefingAvailable);
		if (m_hButtonInGame)
			m_hButtonInGame.UpdateVisuals(
				m_eHostStage == SCR_EGameModeState.GAME,
				actual == SCR_EGameModeState.GAME,
				isAdmin || actual == SCR_EGameModeState.GAME);
	}

	// The line doubles as a stage progress bar driven by the real state.
	protected void UpdateProgressLine(SCR_EGameModeState state)
	{
		if (!m_wHeaderLine)
			return;

		float frac = 0.3333;
		switch (state)
		{
			case SCR_EGameModeState.SLOTSELECTION:
				frac = 0.3333;
				break;
			case SCR_EGameModeState.BRIEFING:
				frac = 0.6667;
				break;
			case SCR_EGameModeState.GAME:
				frac = 1.0;
				break;
			case SCR_EGameModeState.DEBRIEFING:
				frac = 1.0;
				break;
		}

		FrameSlot.SetAnchorMax(m_wHeaderLine, frac, 1);
	}

	// Everyone sees the arrow: green for admins, grey for players.
	protected void UpdateAdvanceButton()
	{
		if (!m_wAdvanceButton)
			return;

		m_wAdvanceButton.SetVisible(true);

		if (m_wAdvanceIcon)
		{
			if (SCR_Global.IsAdmin())
				m_wAdvanceIcon.SetColor(Color.FromInt(0xFF008020));
			else
				m_wAdvanceIcon.SetColor(Color.Gray);
		}
	}

	protected void SetMissionInfo()
	{
		// The author line is rebuilt even without a header, or the layout's placeholder survives.
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());

		string author = "";
		if (header)
			author = header.m_sAuthor;
		if (author == "")
			author = WidgetManager.Translate("#LL-Lobby_AuthorUnknown");

		if (header && m_wTitleText)
			m_wTitleText.SetText(header.m_sName);

		// The prefix is translated up front: SetText resolves #keys only for a whole string.
		if (m_wAuthorText)
			m_wAuthorText.SetText(string.Format("<color rgba='226,167,79,255'>%1</color> %2",
				WidgetManager.Translate("#LL-Lobby_Author"), author));
	}

	// The viewer for everyone; the Game Master panel is /stats only.
	protected void OnStatsClicked()
	{
		LL_StatsScreen.Open();
	}

	protected void OnAdvanceClicked()
	{
		if (!SCR_Global.IsAdmin())
			return;

		if (m_GameModeCoop && m_GameModeCoop.GetLobbyState() == SCR_EGameModeState.GAME)
			return;

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (lobbyPlayer)
			lobbyPlayer.AskAdvanceState();
	}

	// Tab clicks switch the local view only.

	protected void OnLobbyTabClicked()
	{
		ViewState(SCR_EGameModeState.SLOTSELECTION);
	}

	protected void OnBriefingTabClicked()
	{
		ViewState(SCR_EGameModeState.BRIEFING);
	}

	protected void OnGameTabClicked()
	{
		ViewState(SCR_EGameModeState.GAME);
	}

	protected void ViewState(SCR_EGameModeState state)
	{
		if (!m_GameModeCoop)
			return;

		SCR_EGameModeState actual = m_GameModeCoop.GetLobbyState();

		// Players: roster always, briefing not during slot selection, GAME only while
		// running. Must mirror UpdateTabs.
		if (!SCR_Global.IsAdmin())
		{
			bool allowed = false;

			if (state == SCR_EGameModeState.SLOTSELECTION)
				allowed = true;
			else if (state == SCR_EGameModeState.BRIEFING)
				allowed = actual != SCR_EGameModeState.SLOTSELECTION;
			else if (state == SCR_EGameModeState.GAME)
				allowed = actual == SCR_EGameModeState.GAME;

			if (!allowed)
			{
				UpdateTabs();
				return;
			}
		}

		if (state == m_eHostStage)
		{
			UpdateTabs();
			return;
		}

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (lobbyPlayer)
			lobbyPlayer.SwitchToMenu(state);
	}
}

// One stage tab in the header.

class LL_GameModeHeaderButton : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected bool m_bActive;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		m_wRoot = w;
	}

	// viewed = orange toggle, current = check icon, available = white text.
	void UpdateVisuals(bool viewed, bool current, bool available)
	{
		m_bActive = viewed;

		if (!m_wRoot)
			return;

		SCR_ButtonBaseComponent btnComp = SCR_ButtonBaseComponent.Cast(m_wRoot.FindHandler(SCR_ButtonBaseComponent));
		if (btnComp && btnComp.IsToggled() != viewed)
			btnComp.SetToggled(viewed);

		TextWidget stateText = TextWidget.Cast(m_wRoot.FindAnyWidget("StateText"));
		if (stateText)
		{
			if (viewed || available)
				stateText.SetColor(Color.White);
			else
				stateText.SetColor(Color.Gray);
		}

		ImageWidget activeImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("ActiveImage"));
		if (activeImage)
			activeImage.SetVisible(current);
	}

	bool IsActive()
	{
		return m_bActive;
	}
}