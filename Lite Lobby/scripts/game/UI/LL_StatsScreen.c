// The statistics screen every player can see: This Game (the published unit table,
// force-opened at publish and force-closed on hide) and Season (website standings,
// always viewable). A real menu because only menus get a mouse cursor. While results
// are published, non-admins cannot dismiss it.

modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	StatsScreenMenu,
}

class LL_StatsScreen : MenuBase
{
	// The menu manager instantiates the class, so Open cannot pass constructor arguments.
	protected static string s_sPendingFocusTag;
	protected static bool s_bPendingFocusRequested;

	protected Widget m_wRoot;
	protected TextWidget m_wTitle;
	protected TextWidget m_wSubtitle;
	protected Widget m_wContentList;
	protected Widget m_wCloseButton;
	protected Widget m_wGameTabButton;
	protected Widget m_wSeasonTabButton;
	protected SCR_ButtonBaseComponent m_GameTab;
	protected SCR_ButtonBaseComponent m_SeasonTab;

	protected bool m_bLockHeld;
	protected bool m_bSeasonTab;
	protected string m_sFocusUnitTag;
	protected bool m_bFocusRequested;

	static void Open(string focusUnitTag = "", bool focusRequested = false)
	{
		s_sPendingFocusTag = focusUnitTag;
		s_bPendingFocusRequested = focusRequested;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		LL_StatsScreen existing = LL_StatsScreen.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.StatsScreenMenu));
		if (existing)
		{
			existing.ApplyPendingFocus();
			return;
		}

		// The map screens' crosshair cursor survives menu stacking; the game mode brings
		// the stage screen back.
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.GameMapMenu);

		menuManager.OpenMenu(ChimeraMenuPreset.StatsScreenMenu);
	}

	// The unit tag rides the "[TAG]Callsign" display name.
	static void OpenForPlayer(int playerId)
	{
		Open(ExtractUnitTag(playerId), true);
	}

	static void CloseStatic()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase menu = menuManager.FindMenuByPreset(ChimeraMenuPreset.StatsScreenMenu);
		if (menu)
			menu.Close();
	}

	static void RefreshStatic()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		LL_StatsScreen screen = LL_StatsScreen.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.StatsScreenMenu));
		if (screen)
			screen.Render();
	}

	override void OnMenuOpen()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Close();
			return;
		}

		m_wRoot = GetRootWidget();
		if (!m_wRoot)
		{
			Print("[LL_Lobby] Stats: stats screen menu has no root widget — re-index UI/Stats resources in Workbench", LogLevel.WARNING);
			Close();
			return;
		}

		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		m_wSubtitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Subtitle"));
		m_wContentList = m_wRoot.FindAnyWidget("ContentList");

		m_wGameTabButton = m_wRoot.FindAnyWidget("GameTabButton");
		m_wSeasonTabButton = m_wRoot.FindAnyWidget("SeasonTabButton");

		m_GameTab = FindButton("GameTabButton");
		if (m_GameTab)
			m_GameTab.m_OnClicked.Insert(OnGameTab);

		m_SeasonTab = FindButton("SeasonTabButton");
		if (m_SeasonTab)
			m_SeasonTab.m_OnClicked.Insert(OnSeasonTab);

		m_wCloseButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_ButtonBaseComponent closeButton = FindButton("CloseButton");
		if (closeButton)
			closeButton.m_OnClicked.Insert(OnCloseClicked);

		// Esc obeys the same ceremony hold.
		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnBackAction);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.GetOnSeasonUpdated().Insert(OnSeasonUpdated);

		LL_InputLock.Acquire();
		m_bLockHeld = true;

		ApplyPendingFocus();
	}

	override void OnMenuClose()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnBackAction);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.GetOnSeasonUpdated().Remove(OnSeasonUpdated);

		m_wRoot = null;

		if (m_bLockHeld)
		{
			m_bLockHeld = false;
			LL_InputLock.Release();
		}

		// A CallLater on this menu would die with it (weak-ref call queue).
		LL_GameModeCoop gm = LL_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gm)
			gm.RestoreStateMenuIfBare();
	}

	protected void ApplyPendingFocus()
	{
		m_sFocusUnitTag = s_sPendingFocusTag;
		m_bFocusRequested = s_bPendingFocusRequested;

		// Nothing published yet: land on the Season tab, where a unit focus highlights
		// its row.
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		m_bSeasonTab = mgr && !mgr.IsStatsPublished();

		// A client that streamed in during the season broadcast has nothing; pull once.
		if (mgr && mgr.GetSeasonJson() == "")
		{
			LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
			if (lobbyPlayer)
				lobbyPlayer.AskSeasonResend();
		}

		Render();
	}

	protected SCR_ButtonBaseComponent FindButton(string name)
	{
		Widget button = m_wRoot.FindAnyWidget(name);
		if (!button)
			return null;
		return SCR_ButtonBaseComponent.Cast(button.FindHandler(SCR_ButtonBaseComponent));
	}

	protected void OnGameTab()
	{
		m_bSeasonTab = false;
		Render();
	}

	protected void OnSeasonTab()
	{
		m_bSeasonTab = true;
		Render();
	}

	protected void OnCloseClicked()
	{
		if (IsForceHeld())
			return;

		Close();
	}

	protected void OnBackAction()
	{
		// Registered on the raw action, so only the focused menu may react.
		if (!IsFocused())
			return;

		if (IsForceHeld())
		{
			// The ceremony cannot be dismissed, but Esc must still reach the pause menu.
			// Deferred: opening a menu inline from an input event fights the menu manager.
			GetGame().GetCallqueue().CallLater(OpenPauseMenuWrap, 0);
			return;
		}

		Close();
	}

	protected void OpenPauseMenuWrap()
	{
		ArmaReforgerScripted.OpenPauseMenu();
	}

	// Published results are a ceremony: only admins may dismiss locally.
	protected bool IsForceHeld()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		return mgr && mgr.IsStatsPublished() && !SCR_Global.IsAdmin();
	}

	protected void OnSeasonUpdated()
	{
		if (m_bSeasonTab)
			Render();
	}

	// Season leads the tab row until the publish; same X positions the layout authors.
	protected void UpdateTabOrder(bool published)
	{
		if (!m_wGameTabButton || !m_wSeasonTabButton)
			return;

		int gameX = 16;
		int seasonX = 234;
		if (!published)
		{
			gameX = 234;
			seasonX = 16;
		}

		FrameSlot.SetPos(m_wGameTabButton, gameX, 74);
		FrameSlot.SetPos(m_wSeasonTabButton, seasonX, 74);
	}

	protected void Render()
	{
		if (!m_wContentList)
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();

		UpdateTabOrder(mgr && mgr.IsStatsPublished());

		if (m_GameTab)
			m_GameTab.SetToggled(!m_bSeasonTab);
		if (m_SeasonTab)
			m_SeasonTab.SetToggled(m_bSeasonTab);

		if (m_wCloseButton)
			m_wCloseButton.SetVisible(!IsForceHeld());

		LL_StatsTableUI.Clear(m_wContentList);

		if (m_bFocusRequested && m_sFocusUnitTag == "")
			LL_StatsTableUI.AddNotice(m_wContentList, WidgetManager.Translate("#LL-Stats_NoUnit"));

		if (m_bSeasonTab)
		{
			if (m_wTitle)
				m_wTitle.SetText(WidgetManager.Translate("#LL-Stats_SeasonTitle"));
			if (m_wSubtitle)
				m_wSubtitle.SetText("");

			string seasonJson = "";
			if (mgr)
				seasonJson = mgr.GetSeasonJson();

			LL_StatsTableUI.BuildSeason(m_wContentList, seasonJson, m_sFocusUnitTag);
			return;
		}

		if (m_wTitle)
			m_wTitle.SetText(WidgetManager.Translate("#LL-Stats_GameTitle"));

		if (!mgr || !mgr.IsStatsPublished())
		{
			if (m_wSubtitle)
				m_wSubtitle.SetText("");
			LL_StatsTableUI.AddNotice(m_wContentList, WidgetManager.Translate("#LL-Stats_NotPublished"));
			return;
		}

		LL_StatsView view = ParseView(mgr.GetStatsViewJson());
		if (!view)
		{
			LL_StatsTableUI.AddNotice(m_wContentList, WidgetManager.Translate("#LL-Stats_NotPublished"));
			return;
		}

		if (m_wSubtitle)
			m_wSubtitle.SetText(view.missionName);

		LL_StatsTableUI.BuildGameView(m_wContentList, view, m_sFocusUnitTag);
	}

	static LL_StatsView ParseView(string json)
	{
		if (json == "")
			return null;

		LL_StatsView view = new LL_StatsView();
		view.ExpandFromRAW(json);

		// ExpandFromRAW has no error return; a real view always carries the session id.
		if (view.sessionId == "")
		{
			string head = json;
			if (head.Length() > 160)
				head = head.Substring(0, 160);
			Print("[LL_Lobby] Stats: view payload did not parse (first bytes: " + head + ")", LogLevel.WARNING);
			return null;
		}

		return view;
	}

	static string ExtractUnitTag(int playerId)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return "";

		string name = mgr.GetPlayerName(playerId);
		if (name.Length() < 3 || name.Substring(0, 1) != "[")
			return "";

		int close = name.IndexOf("]");
		if (close <= 1)
			return "";

		return name.Substring(1, close - 1);
	}
}