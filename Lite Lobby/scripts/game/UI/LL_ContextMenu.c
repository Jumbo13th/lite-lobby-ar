// Right-click action menu in the lobby's dark and orange language. Parented under the
// open menu's root with a high z-order: workspace widgets render below menus. Single
// instance. Openers gate on IsAdmin for fast feedback; every action's RPC is re-validated
// on the server.

class LL_ContextMenu : SCR_ScriptedWidgetComponent
{
	protected const ResourceName MENU_LAYOUT = "{1D14A7B8C9D0E1F2}UI/ContextMenu/ContextMenu.layout";
	protected const ResourceName ACTION_LAYOUT = "{1D15A7B8C9D0E1F2}UI/ContextMenu/ContextAction.layout";

	protected const ResourceName ICONSET = "{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset";

	// Mirrors the layout's fixed sizes: the off-screen clamp runs before layout has measured.
	protected const int MENU_WIDTH = 240;
	protected const int ROW_HEIGHT = 26;
	protected const int HEADER_HEIGHT = 26;
	protected const int PADDING = 8;

	protected static Widget s_wMenu;

	protected VerticalLayoutWidget m_wActions;
	protected Widget m_wHeader;
	protected TextWidget m_wHeaderText;
	protected Widget m_wNoActions;

	protected LL_CoopLobby m_CoopLobby;

	protected int m_iHeight = PADDING;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wActions = VerticalLayoutWidget.Cast(w.FindAnyWidget("ActionsVerticalLayout"));
		m_wHeader = w.FindAnyWidget("Header");
		m_wHeaderText = TextWidget.Cast(w.FindAnyWidget("HeaderText"));
		m_wNoActions = w.FindAnyWidget("NoActions");

		// Close on the next click anywhere; deferred a frame so an action's own handler
		// runs first.
		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.AddActionListener("MouseLeft", EActionTrigger.UP, Close);
			input.AddActionListener("MouseRight", EActionTrigger.DOWN, Close);
		}
	}

	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);

		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.RemoveActionListener("MouseLeft", EActionTrigger.UP, Close);
			input.RemoveActionListener("MouseRight", EActionTrigger.DOWN, Close);
		}

		if (s_wMenu == w)
			s_wMenu = null;
	}

	// At the mouse, parented above the lobby's root. "" hides the header band.
	static LL_ContextMenu Create(LL_CoopLobby lobby, string header)
	{
		if (!lobby)
			return null;

		LL_ContextMenu menu = CreateAt(lobby.GetRootWidget(), header);
		if (menu)
			menu.m_CoopLobby = lobby;

		return menu;
	}

	// Under any menu's root; the stats panel uses it as its option dropdown. Without a
	// lobby the lobby-specific adders stay unavailable.
	static LL_ContextMenu CreateAt(Widget parentRoot, string header)
	{
		if (!parentRoot)
			return null;

		if (s_wMenu)
			s_wMenu.RemoveFromHierarchy();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		s_wMenu = workspace.CreateWidgets(MENU_LAYOUT, parentRoot);
		if (!s_wMenu)
			return null;

		s_wMenu.SetZOrder(10000);

		LL_ContextMenu menu = LL_ContextMenu.Cast(s_wMenu.FindHandler(LL_ContextMenu));
		if (!menu)
		{
			s_wMenu.RemoveFromHierarchy();
			s_wMenu = null;
			return null;
		}

		int x, y;
		WidgetManager.GetMousePos(x, y);
		x = workspace.DPIUnscale(x);
		y = workspace.DPIUnscale(y);
		FrameSlot.SetPos(s_wMenu, x, y);

		if (header != "" && menu.m_wHeaderText)
		{
			menu.m_wHeaderText.SetText(header);
			menu.m_iHeight += HEADER_HEIGHT;
		}
		else if (menu.m_wHeader)
		{
			menu.m_wHeader.SetVisible(false);
		}

		GetGame().GetCallqueue().Call(menu.ClampOnScreen);
		return menu;
	}

	// `data` is a generic payload (target player id, slot or group id).
	LL_ContextAction AddAction(ResourceName iconSet, string iconQuad, string label, int data)
	{
		if (m_wNoActions)
		{
			m_wNoActions.RemoveFromHierarchy();
			m_wNoActions = null;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_wActions)
			return null;

		Widget row = workspace.CreateWidgets(ACTION_LAYOUT, m_wActions);
		if (!row)
			return null;

		LL_ContextAction action = LL_ContextAction.Cast(row.FindHandler(LL_ContextAction));
		if (!action)
		{
			row.RemoveFromHierarchy();
			return null;
		}

		action.Init(iconSet, iconQuad, label, data);
		m_iHeight += ROW_HEIGHT;
		return action;
	}

	// Not admin-gated: units are the public statistics entity.
	void AddUnitStatsAction(int targetPlayerId)
	{
		LL_ContextAction stats = AddAction(ICONSET, "player", "#LL-ContextAction_UnitStats", targetPlayerId);
		if (stats)
			stats.GetOnActivated().Insert(OnUnitStats);
	}

	protected void OnUnitStats(LL_ContextAction action)
	{
		LL_StatsScreen.OpenForPlayer(action.GetActionData());
	}

	// Marks the target as the move-to-slot selection; skipped if already selected.
	void AddSelectAction(int targetPlayerId)
	{
		if (!m_CoopLobby || m_CoopLobby.GetSelectedPlayer() == targetPlayerId)
			return;

		LL_ContextAction select = AddAction(ICONSET, "player", "#LL-ContextAction_SelectPlayer", targetPlayerId);
		if (select)
			select.GetOnActivated().Insert(OnSelectPlayer);
	}

	protected void OnSelectPlayer(LL_ContextAction action)
	{
		if (m_CoopLobby)
			m_CoopLobby.SetSelectedPlayer(action.GetActionData());
	}

	// Label and intent are fixed at add time, so each branch wires its own callback (the
	// action carries one int payload).
	void AddSlotLockAction(int slotRplId, bool currentlyLocked)
	{
		if (currentlyLocked)
		{
			LL_ContextAction unlock = AddAction(ICONSET, "server-unlocked", "#LL-ContextAction_UnlockSlot", slotRplId);
			if (unlock)
				unlock.GetOnActivated().Insert(OnUnlockSlot);
		}
		else
		{
			LL_ContextAction lock = AddAction(ICONSET, "server-locked", "#LL-ContextAction_LockSlot", slotRplId);
			if (lock)
				lock.GetOnActivated().Insert(OnLockSlot);
		}
	}

	protected void OnLockSlot(LL_ContextAction action)
	{
		LL_LobbyPlayerComponent pc = LL_LobbyPlayerComponent.GetLocalInstance();
		if (pc)
			pc.AskSetSlotLocked(action.GetActionData(), true);
	}

	protected void OnUnlockSlot(LL_ContextAction action)
	{
		LL_LobbyPlayerComponent pc = LL_LobbyPlayerComponent.GetLocalInstance();
		if (pc)
			pc.AskSetSlotLocked(action.GetActionData(), false);
	}

	// `data` carries the group id here.
	void AddGroupLockAction(int groupId, bool currentlyLocked)
	{
		if (currentlyLocked)
		{
			LL_ContextAction unlock = AddAction(ICONSET, "server-unlocked", "#LL-ContextAction_UnlockGroup", groupId);
			if (unlock)
				unlock.GetOnActivated().Insert(OnUnlockGroup);
		}
		else
		{
			LL_ContextAction lock = AddAction(ICONSET, "server-locked", "#LL-ContextAction_LockGroup", groupId);
			if (lock)
				lock.GetOnActivated().Insert(OnLockGroup);
		}
	}

	protected void OnLockGroup(LL_ContextAction action)
	{
		LL_LobbyPlayerComponent pc = LL_LobbyPlayerComponent.GetLocalInstance();
		if (pc)
			pc.AskSetGroupLocked(action.GetActionData(), true);
	}

	protected void OnUnlockGroup(LL_ContextAction action)
	{
		LL_LobbyPlayerComponent pc = LL_LobbyPlayerComponent.GetLocalInstance();
		if (pc)
			pc.AskSetGroupLocked(action.GetActionData(), false);
	}

	// "Kick from slot" appears only when the target occupies one.
	void AddKickActions(int targetPlayerId)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();

		if (mgr && mgr.FindSlotByPlayerId(targetPlayerId))
		{
			LL_ContextAction fromSlot = AddAction(ICONSET, "exit", "#LL-ContextAction_KickFromSlot", targetPlayerId);
			if (fromSlot)
				fromSlot.GetOnActivated().Insert(OnKickFromSlot);
		}

		LL_ContextAction fromServer = AddAction(ICONSET, "kickCommandAlt", "#LL-ContextAction_KickFromServer", targetPlayerId);
		if (fromServer)
			fromServer.GetOnActivated().Insert(OnKickFromServer);
	}

	protected void OnKickFromSlot(LL_ContextAction action)
	{
		LL_LobbyPlayerComponent pc = LL_LobbyPlayerComponent.GetLocalInstance();
		if (pc)
			pc.AskKickPlayer(action.GetActionData());
	}

	protected void OnKickFromServer(LL_ContextAction action)
	{
		LL_LobbyPlayerComponent pc = LL_LobbyPlayerComponent.GetLocalInstance();
		if (pc)
			pc.AskKickFromServer(action.GetActionData());
	}

	// A height estimate, not a measured size: this runs before layout has resolved bounds.
	void ClampOnScreen()
	{
		if (!m_wRoot)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		int x = FrameSlot.GetPosX(m_wRoot);
		int y = FrameSlot.GetPosY(m_wRoot);

		float sw, sh;
		workspace.GetScreenSize(sw, sh);
		sw = workspace.DPIUnscale(sw);
		sh = workspace.DPIUnscale(sh);

		if (x + MENU_WIDTH > sw)
			x -= MENU_WIDTH;
		if (y + m_iHeight > sh)
			y -= m_iHeight;
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;

		FrameSlot.SetPos(m_wRoot, x, y);
	}

	// Deferred a frame so an action's own click handler runs first. Removes this
	// instance's root, not s_wMenu, which may already point at a replacement.
	void Close()
	{
		GetGame().GetCallqueue().Call(CloseNow);
	}

	protected void CloseNow()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}
}