// Slot-selection screen. Reads LL_LobbyManager, sends actions through
// LL_LobbyPlayerComponent, builds factions → squads → slots from replicated state.

modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	CoopLobby
}

class LL_CoopLobby : MenuBase
{
	protected ResourceName m_sRolesGroupPrefab = "{B45A0FA6883A7A0E}UI/Lobby/RolesGroup.layout";
	protected ResourceName m_sCharacterSelectorPrefab = "{3F761F63F1DF29D1}UI/Lobby/CharacterSelector.layout";
	protected ResourceName m_sFactionSelectorPrefab = "{DA22ED7112FA8028}UI/Lobby/FactionSelector.layout";
	protected ResourceName m_sPlayerSelectorPrefab = "{B55DD7054C5892AE}UI/Lobby/PlayerSelector.layout";

	protected LL_GameModeCoop m_GameModeCoop;
	protected LL_LobbyManager m_LobbyManager;
	protected PlayerManager m_PlayerManager;
	protected PlayerController m_PlayerController;
	protected SCR_FactionManager m_FactionManager;
	protected LL_LobbyPlayerComponent m_LobbyPlayerComponent;
	protected WorkspaceWidget m_wWorkspaceWidget;
	protected int m_iPlayerId;

	protected Widget m_wRoot;
	protected VerticalLayoutWidget m_wFactionList;
	protected VerticalLayoutWidget m_wRolesList;
	protected VerticalLayoutWidget m_wPlayersList;
	protected TextWidget m_wPlayersCounter;
	protected ButtonWidget m_wNavigationStart;
	protected ButtonWidget m_wNavigationClose;
	protected FrameWidget m_wGameModeHeader;
	protected ScrollLayoutWidget m_wRolesScroll;
	protected Widget m_wWebsiteLegend;

	protected LL_GameModeHeader m_GameModeHeader;
	protected LL_LobbyLoadoutPreview m_LoadoutPreview;
	protected SCR_InputButtonComponent m_NavigationStartComp;
	protected SCR_InputButtonComponent m_NavigationCloseComp;
	protected SCR_InputButtonComponent m_NavigationChatComp;
	protected SCR_ChatPanel m_ChatPanel;

	protected Widget m_wPlayersScroll;
	protected Widget m_wVoiceChannelsPanel;
	protected SCR_ButtonBaseComponent m_TabPlayerListComp;
	protected SCR_ButtonBaseComponent m_TabVoiceComp;

	// Client-side filter over the replicated roster.
	protected SCR_EditBoxSearchComponent m_PlayerSearch;
	protected Widget m_wPlayersSearch;
	protected Widget m_wPlayersSearchEmpty;
	protected string m_sPlayerFilter;

	protected ref map<string, LL_FactionSelector> m_mFactions = new map<string, LL_FactionSelector>();
	protected ref map<int, LL_RolesGroup> m_mGroups = new map<int, LL_RolesGroup>();
	protected ref map<int, LL_PlayerSelector> m_mPlayers = new map<int, LL_PlayerSelector>();
	protected string m_sCurrentFactionKey;
	protected bool m_bRebuildQueued;

	// Admin move-to-slot target; defaults to the local player so a normal slot click
	// acts on yourself.
	protected int m_iSelectedPlayer = -1;

	override void OnMenuOpen()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Close();
			return;
		}

		m_GameModeCoop = LL_GameModeCoop.GetInstance();
		m_LobbyManager = LL_LobbyManager.GetInstance();
		m_PlayerManager = GetGame().GetPlayerManager();
		m_PlayerController = GetGame().GetPlayerController();
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		m_wWorkspaceWidget = GetGame().GetWorkspace();
		m_iPlayerId = m_PlayerController.GetPlayerId();
		m_LobbyPlayerComponent = LL_LobbyPlayerComponent.Cast(
			m_PlayerController.FindComponent(LL_LobbyPlayerComponent));

		m_iSelectedPlayer = GetLocalPlayerId();

		m_wRoot = GetRootWidget();
		m_wFactionList = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("FactionList"));
		m_wRolesList = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("RolesList"));
		m_wPlayersList = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("PlayersList"));
		m_wPlayersCounter = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayersCounter"));
		m_wNavigationStart = ButtonWidget.Cast(m_wRoot.FindAnyWidget("NavigationStart"));
		m_wNavigationClose = ButtonWidget.Cast(m_wRoot.FindAnyWidget("NavigationClose"));
		m_wGameModeHeader = FrameWidget.Cast(m_wRoot.FindAnyWidget("GameModeHeader"));
		m_wRolesScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("RolesScroll"));
		m_wWebsiteLegend = m_wRoot.FindAnyWidget("WebsiteLegend");

		m_wPlayersScroll = m_wRoot.FindAnyWidget("PlayersScroll");
		m_wVoiceChannelsPanel = m_wRoot.FindAnyWidget("VoiceChannelsPanel");

		Widget wTabList = m_wRoot.FindAnyWidget("TabPlayerList");
		if (wTabList)
			m_TabPlayerListComp = SCR_ButtonBaseComponent.Cast(wTabList.FindHandler(SCR_ButtonBaseComponent));

		Widget wTabVoice = m_wRoot.FindAnyWidget("TabVoiceChannels");
		if (wTabVoice)
			m_TabVoiceComp = SCR_ButtonBaseComponent.Cast(wTabVoice.FindHandler(SCR_ButtonBaseComponent));

		if (m_TabPlayerListComp)
			m_TabPlayerListComp.m_OnClicked.Insert(Action_ShowPlayerList);
		if (m_TabVoiceComp)
			m_TabVoiceComp.m_OnClicked.Insert(Action_ShowVoiceChannels);

		m_wPlayersSearchEmpty = m_wRoot.FindAnyWidget("PlayersSearchEmpty");

		m_wPlayersSearch = m_wRoot.FindAnyWidget("PlayersSearch");
		if (m_wPlayersSearch)
			m_PlayerSearch = SCR_EditBoxSearchComponent.Cast(m_wPlayersSearch.FindHandler(SCR_EditBoxSearchComponent));

		if (m_PlayerSearch)
		{
			// The placeholder is a plain string the engine does not run through the string table.
			m_PlayerSearch.SetPlaceholderText(WidgetManager.Translate("#LL-Lobby_SearchPlayers"));

			// Polled while the box has focus; focus loss fires one final update.
			m_PlayerSearch.m_OnTextChange.Insert(OnPlayerSearchChanged);
		}

		SetVoiceTabActive(false);

		Widget wChatPanel = m_wRoot.FindAnyWidget("ChatPanel");
		if (wChatPanel)
			m_ChatPanel = SCR_ChatPanel.Cast(wChatPanel.FindHandler(SCR_ChatPanel));

		// Exactly one visible chat: this one, not the gameplay HUD overlay too.
		LL_MenuChat.ShowOwnPanel(m_ChatPanel);

		Widget wNavChat = m_wRoot.FindAnyWidget("NavigationChat");
		if (wNavChat)
			m_NavigationChatComp = SCR_InputButtonComponent.Cast(wNavChat.FindHandler(SCR_InputButtonComponent));

		if (m_NavigationChatComp)
			m_NavigationChatComp.m_OnActivated.Insert(Action_ChatOpen);

		GetGame().GetInputManager().AddActionListener("ChatToggle", EActionTrigger.DOWN, Action_ChatKey);

		if (m_wGameModeHeader)
			m_GameModeHeader = LL_GameModeHeader.Cast(m_wGameModeHeader.FindHandler(LL_GameModeHeader));

		// Optional pane; absent from the layout means every preview call no-ops.
		Widget wLoadoutPreview = m_wRoot.FindAnyWidget("LoadoutPreview");
		if (wLoadoutPreview)
			m_LoadoutPreview = LL_LobbyLoadoutPreview.Cast(wLoadoutPreview.FindHandler(LL_LobbyLoadoutPreview));

		if (m_GameModeHeader)
			m_GameModeHeader.SetHostStage(SCR_EGameModeState.SLOTSELECTION);

		if (m_wNavigationStart)
			m_NavigationStartComp = SCR_InputButtonComponent.Cast(m_wNavigationStart.FindHandler(SCR_InputButtonComponent));

		if (m_wNavigationClose)
			m_NavigationCloseComp = SCR_InputButtonComponent.Cast(m_wNavigationClose.FindHandler(SCR_InputButtonComponent));

		if (m_NavigationStartComp)
			m_NavigationStartComp.m_OnActivated.Insert(Action_Ready);

		if (m_NavigationCloseComp)
			m_NavigationCloseComp.m_OnActivated.Insert(Action_Exit);

		// Gameplay input contexts are inactive while a menu is open, so the menu handles
		// the admin U-toggle itself.
		GetGame().GetInputManager().AddActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_LobbyKeyToggle);

		// The U hold that opened this menu from another one is still held; arm the toggle
		// only after a grace period.
		m_fOpenedTime = GetGame().GetWorld().GetWorldTime();

		if (m_LobbyManager)
		{
			m_LobbyManager.GetOnSlotRegistered().Insert(OnSlotRegistered);
			m_LobbyManager.GetOnSlotUnregistered().Insert(OnSlotUnregistered);
			m_LobbyManager.GetOnVehicleRegistered().Insert(OnVehicleRegistered);
			m_LobbyManager.GetOnVehicleUnregistered().Insert(OnVehicleUnregistered);
			m_LobbyManager.GetOnSlotUpdated().Insert(OnSlotUpdated);
			m_LobbyManager.GetOnPlayerAssigned().Insert(OnPlayerAssigned);
			m_LobbyManager.GetOnPlayerUnassigned().Insert(OnPlayerUnassigned);
			m_LobbyManager.GetOnPlayerNameUpdated().Insert(OnPlayerNameUpdated);
			m_LobbyManager.GetOnPlayerReadyChanged().Insert(OnPlayerReadyChanged);
			m_LobbyManager.GetOnPlayerRemoved().Insert(OnPlayerRemoved);
			m_LobbyManager.GetOnPlayerConnectionChanged().Insert(OnPlayerConnectionChanged);
			m_LobbyManager.GetOnWebsiteOccupantsChanged().Insert(UpdateWebsiteLegend);
		}

		UpdateWebsiteLegend();

		// Pulled on every open to self-heal roster entries missed in connect-time RPC races.
		if (m_LobbyPlayerComponent)
			m_LobbyPlayerComponent.AskSyncRoster();

		BuildUI();
	}

	override void OnMenuClose()
	{
		LL_MenuChat.NotifyMenuClosed();

		// A queued rebuild firing after close would touch dead widgets.
		GetGame().GetCallqueue().Remove(DoQueuedRebuild);
		m_bRebuildQueued = false;

		if (m_LobbyManager)
		{
			m_LobbyManager.GetOnSlotRegistered().Remove(OnSlotRegistered);
			m_LobbyManager.GetOnSlotUnregistered().Remove(OnSlotUnregistered);
			m_LobbyManager.GetOnVehicleRegistered().Remove(OnVehicleRegistered);
			m_LobbyManager.GetOnVehicleUnregistered().Remove(OnVehicleUnregistered);
			m_LobbyManager.GetOnSlotUpdated().Remove(OnSlotUpdated);
			m_LobbyManager.GetOnPlayerAssigned().Remove(OnPlayerAssigned);
			m_LobbyManager.GetOnPlayerUnassigned().Remove(OnPlayerUnassigned);
			m_LobbyManager.GetOnPlayerNameUpdated().Remove(OnPlayerNameUpdated);
			m_LobbyManager.GetOnPlayerReadyChanged().Remove(OnPlayerReadyChanged);
			m_LobbyManager.GetOnPlayerRemoved().Remove(OnPlayerRemoved);
			m_LobbyManager.GetOnPlayerConnectionChanged().Remove(OnPlayerConnectionChanged);
		}

		if (m_NavigationStartComp)
			m_NavigationStartComp.m_OnActivated.Remove(Action_Ready);
		if (m_NavigationCloseComp)
			m_NavigationCloseComp.m_OnActivated.Remove(Action_Exit);
		if (m_NavigationChatComp)
			m_NavigationChatComp.m_OnActivated.Remove(Action_ChatOpen);
		if (m_TabPlayerListComp)
			m_TabPlayerListComp.m_OnClicked.Remove(Action_ShowPlayerList);
		if (m_TabVoiceComp)
			m_TabVoiceComp.m_OnClicked.Remove(Action_ShowVoiceChannels);
		if (m_PlayerSearch)
			m_PlayerSearch.m_OnTextChange.Remove(OnPlayerSearchChanged);

		GetGame().GetInputManager().RemoveActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_LobbyKeyToggle);
		GetGame().GetInputManager().RemoveActionListener("ChatToggle", EActionTrigger.DOWN, Action_ChatKey);
	}

	override void OnMenuUpdate(float tDelta)
	{
		// Input comes from the preset's ActionContext. SCR_ChatPanel needs a per-frame tick
		// outside the HUD manager.
		if (m_ChatPanel)
			m_ChatPanel.OnUpdateChat(tDelta);

		if (m_GameModeHeader)
			m_GameModeHeader.TryUpdate();

		if (m_LoadoutPreview)
			m_LoadoutPreview.Update(tDelta);

		CheckRosterDesync();
	}

	// Pulls a reconciling re-sync only on an actual divergence from the replicated count
	// or checksum, so a healthy lobby sends nothing.
	protected void CheckRosterDesync()
	{
		if (!m_LobbyManager || !m_LobbyPlayerComponent || m_LobbyManager.IsRosterSyncInFlight())
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fLastRosterCheck < 2000)
			return;

		m_fLastRosterCheck = now;

		if (m_LobbyManager.GetReplicatedRosterCount() != m_LobbyManager.GetKnownPlayerCount()
			|| m_LobbyManager.GetReplicatedRosterChecksum() != m_LobbyManager.ComputeLocalRosterChecksum())
			m_LobbyPlayerComponent.AskSyncRoster();
	}

	protected void BuildUI()
	{
		if (!m_LobbyManager)
			return;

		// A row hovered when a rebuild fires is removed without its mouse-leave.
		ClearPreview();

		ClearChildWidgets(m_wFactionList);
		ClearChildWidgets(m_wRolesList);
		ClearChildWidgets(m_wPlayersList);
		m_mFactions.Clear();
		m_mGroups.Clear();
		m_mPlayers.Clear();

		array<ref LL_SlotData> slots = m_LobbyManager.GetSlots();

		// Enforce maps iterate in hash order; the array keeps the manager's sorted order.
		ref map<string, ref array<ref LL_SlotData>> factionSlots = new map<string, ref array<ref LL_SlotData>>();
		ref array<string> factionOrder = {};

		foreach (LL_SlotData slot : slots)
		{
			string fKey = slot.m_sFactionKey;
			if (!factionSlots.Contains(fKey))
			{
				factionSlots.Set(fKey, new array<ref LL_SlotData>());
				factionOrder.Insert(fKey);
			}

			factionSlots[fKey].Insert(slot);
		}

		foreach (string orderedFactionKey : factionOrder)
		{
			AddFaction(orderedFactionKey, factionSlots[orderedFactionKey]);
		}

		// Keep the faction the player is viewing across rebuilds; fall back to
		// the own slot's faction, then to the first registered one.
		LL_SlotData mySlot = m_LobbyManager.FindSlotByPlayerId(GetLocalPlayerId());
		if (m_sCurrentFactionKey != "" && factionSlots.Contains(m_sCurrentFactionKey))
			SwitchCurrentFaction(m_sCurrentFactionKey);
		else if (mySlot)
			SwitchCurrentFaction(mySlot.m_sFactionKey);
		else if (factionOrder.Count() > 0)
			SwitchCurrentFaction(factionOrder[0]);

		BuildPlayerList();

		UpdatePlayerCounter();
	}

	protected void AddFaction(string factionKey, array<ref LL_SlotData> slots)
	{
		if (!m_wFactionList || !m_wWorkspaceWidget)
			return;

		Widget factionWidget = m_wWorkspaceWidget.CreateWidgets(m_sFactionSelectorPrefab, m_wFactionList);
		if (!factionWidget)
			return;

		LL_FactionSelector factionSelector = LL_FactionSelector.Cast(factionWidget.FindHandler(LL_FactionSelector));
		if (!factionSelector)
			return;

		SCR_Faction faction = null;
		if (m_FactionManager)
			faction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(factionKey));

		factionSelector.Init(faction, factionKey, this);
		m_mFactions.Set(factionKey, factionSelector);

		// Enforce maps iterate in hash order; the array keeps the sorted order.
		ref map<int, ref array<ref LL_SlotData>> groupedSlots = new map<int, ref array<ref LL_SlotData>>();
		ref array<int> groupOrder = {};

		foreach (LL_SlotData slot : slots)
		{
			int gId = slot.m_iGroupId;
			if (!groupedSlots.Contains(gId))
			{
				groupedSlots.Set(gId, new array<ref LL_SlotData>());
				groupOrder.Insert(gId);
			}

			groupedSlots[gId].Insert(slot);
		}

		foreach (int groupId : groupOrder)
		{
			AddRolesGroup(factionKey, groupId, groupedSlots[groupId]);
		}

		UpdateFactionCounts(factionKey);
	}

	protected void AddRolesGroup(string factionKey, int groupId, array<ref LL_SlotData> slots)
	{
		if (!m_wRolesList || !m_wWorkspaceWidget)
			return;

		Widget groupWidget = m_wWorkspaceWidget.CreateWidgets(m_sRolesGroupPrefab, m_wRolesList);
		if (!groupWidget)
			return;

		LL_RolesGroup rolesGroup = LL_RolesGroup.Cast(groupWidget.FindHandler(LL_RolesGroup));
		if (!rolesGroup)
			return;

		string groupName = "";
		if (slots.Count() > 0)
			groupName = slots[0].m_sGroupName;
		rolesGroup.Init(factionKey, groupId, groupName, this);
		m_mGroups.Set(groupId, rolesGroup);

		foreach (LL_SlotData slot : slots)
		{
			rolesGroup.AddSlot(slot);
		}

		array<ref LL_VehicleData> vehicles = m_LobbyManager.GetVehiclesForGroup(groupId);
		foreach (LL_VehicleData vehicle : vehicles)
		{
			rolesGroup.AddVehicle(vehicle);
		}

		groupWidget.SetVisible(factionKey == m_sCurrentFactionKey);
	}

	protected void BuildPlayerList()
	{
		if (!m_wPlayersList || !m_LobbyManager)
			return;

		ClearChildWidgets(m_wPlayersList);
		m_mPlayers.Clear();

		// The local PlayerManager fills asynchronously on dedicated-server clients; the
		// roster is server-authoritative and later arrivals come through the events.
		array<int> playerIds = {};
		m_LobbyManager.GetKnownPlayerIds(playerIds);

		foreach (int playerId : playerIds)
		{
			AddPlayer(playerId);
		}

		// A rebuild recreates every row, so a search typed before it must be re-applied.
		ApplyPlayerFilter();
	}

	protected void OnPlayerSearchChanged(string text)
	{
		string filter = text;
		filter.TrimInPlace();
		filter.ToLower();

		if (filter == m_sPlayerFilter)
			return;

		m_sPlayerFilter = filter;
		ApplyPlayerFilter();
	}

	protected void ApplyPlayerFilter()
	{
		int matches = 0;
		for (int i = 0; i < m_mPlayers.Count(); i++)
		{
			if (m_mPlayers.GetElement(i).ApplyFilter(m_sPlayerFilter))
				matches++;
		}

		// An empty panel reads as "the roster broke" rather than "nobody matches".
		if (m_wPlayersSearchEmpty)
			m_wPlayersSearchEmpty.SetVisible(m_sPlayerFilter != "" && matches == 0);
	}

	protected void AddPlayer(int playerId)
	{
		if (!m_wPlayersList || !m_wWorkspaceWidget)
			return;

		if (m_mPlayers.Contains(playerId))
			return;

		Widget playerWidget = m_wWorkspaceWidget.CreateWidgets(m_sPlayerSelectorPrefab, m_wPlayersList);
		if (!playerWidget)
			return;

		LL_PlayerSelector playerSelector = LL_PlayerSelector.Cast(playerWidget.FindHandler(LL_PlayerSelector));
		if (!playerSelector)
			return;

		string playerName = m_LobbyManager.GetPlayerName(playerId);
		if (playerName == "")
			playerName = m_PlayerManager.GetPlayerName(playerId);

		playerSelector.Init(playerId, playerName, this);
		playerSelector.ApplyFilter(m_sPlayerFilter);
		m_mPlayers.Set(playerId, playerSelector);

		UpdatePlayerCounter();
	}

	void SwitchCurrentFaction(string factionKey)
	{
		m_sCurrentFactionKey = factionKey;

		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			LL_RolesGroup group = m_mGroups.GetElement(i);
			Widget groupWidget = group.GetRootWidget();
			if (groupWidget)
				groupWidget.SetVisible(group.GetFactionKey() == factionKey);
		}

		for (int i = 0; i < m_mFactions.Count(); i++)
		{
			m_mFactions.GetElement(i).SetSelected(m_mFactions.GetKey(i) == factionKey);
		}
	}

	string GetCurrentFactionKey()
	{
		return m_sCurrentFactionKey;
	}

	protected void OnSlotRegistered(LL_SlotData slot)
	{
		// Full rebuild: slots register in bursts and the manager keeps them sorted, so
		// appending in arrival order would draw squads differently on every machine. The
		// queue collapses a burst into one rebuild per frame.
		QueueRebuild();
	}

	protected void OnSlotUnregistered(int rplId)
	{
		QueueRebuild();
	}

	protected void OnVehicleRegistered(LL_VehicleData vehicle)
	{
		QueueRebuild();
	}

	protected void OnVehicleUnregistered(int rplId)
	{
		QueueRebuild();
	}

	protected void QueueRebuild()
	{
		if (m_bRebuildQueued)
			return;

		m_bRebuildQueued = true;
		GetGame().GetCallqueue().CallLater(DoQueuedRebuild, 0, false);
	}

	protected void DoQueuedRebuild()
	{
		m_bRebuildQueued = false;
		BuildUI();
	}

	protected void OnSlotUpdated(LL_SlotData slot)
	{
		UpdateFactionCounts(slot.m_sFactionKey);
	}

	protected void OnPlayerAssigned(int playerId, int slotRplId)
	{
		UpdatePlayerCounter();
		UpdatePlayerInList(playerId);
	}

	protected void OnPlayerUnassigned(int playerId, int slotRplId)
	{
		UpdatePlayerCounter();
		UpdatePlayerInList(playerId);
	}

	protected void OnPlayerNameUpdated(int playerId, string name)
	{
		if (!m_mPlayers.Contains(playerId))
			AddPlayer(playerId);

		LL_PlayerSelector playerSel;
		if (m_mPlayers.Find(playerId, playerSel))
		{
			playerSel.UpdateName(name);
			ApplyPlayerFilter();
		}
	}

	protected void UpdateFactionCounts(string factionKey)
	{
		LL_FactionSelector factionSel;
		if (!m_mFactions.Find(factionKey, factionSel))
			return;

		if (!m_LobbyManager)
			return;

		array<ref LL_SlotData> slots = m_LobbyManager.GetSlotsForFaction(factionKey);
		int total = slots.Count();
		int occupied = 0;
		int locked = 0;

		foreach (LL_SlotData slot : slots)
		{
			if (slot.m_iPlayerId >= 0)
				occupied++;
			if (slot.m_bLocked)
				locked++;
		}

		factionSel.SetCounts(occupied, total, locked);
	}

	protected void UpdatePlayerCounter()
	{
		if (!m_wPlayersCounter || !m_LobbyManager)
			return;

		// Connected players only, from the roster rather than PlayerManager (which fills
		// asynchronously on dedicated-server clients).
		int playerCount = m_LobbyManager.GetConnectedPlayerCount();
		int slotCount = m_LobbyManager.GetSlots().Count();
		m_wPlayersCounter.SetTextFormat("%1/%2", playerCount, slotCount);
	}

	protected void UpdatePlayerInList(int playerId)
	{
		LL_PlayerSelector playerSel;
		if (!m_mPlayers.Find(playerId, playerSel))
			return;

		playerSel.Refresh();

		// The role/squad line feeds the search.
		ApplyPlayerFilter();
	}

	protected void OnPlayerReadyChanged(int playerId, bool ready)
	{
		UpdatePlayerInList(playerId);
	}

	protected void OnPlayerConnectionChanged(int playerId, bool connected)
	{
		// A player connecting after the menu was built must get a row even if their name
		// event raced the menu open.
		if (connected && !m_mPlayers.Contains(playerId))
			AddPlayer(playerId);

		UpdatePlayerInList(playerId);
		UpdatePlayerCounter();
	}

	// Shown only while an import is active.
	protected void UpdateWebsiteLegend()
	{
		if (!m_wWebsiteLegend)
			return;

		bool show = false;
		if (m_LobbyManager)
			show = m_LobbyManager.CountWebsiteOccupants() > 0;

		m_wWebsiteLegend.SetVisible(show);
	}

	protected void OnPlayerRemoved(int playerId)
	{
		LL_PlayerSelector playerSel;
		if (!m_mPlayers.Find(playerId, playerSel))
			return;

		Widget root = playerSel.GetRootWidget();
		if (root)
			root.RemoveFromHierarchy();

		m_mPlayers.Remove(playerId);

		UpdatePlayerCounter();
		ApplyPlayerFilter();
	}

	protected void Action_Ready()
	{
		// Ready flags this player only; it does not advance the game state.
		if (!m_LobbyPlayerComponent || !m_LobbyManager)
			return;

		int playerId = GetLocalPlayerId();
		bool ready = m_LobbyManager.IsPlayerReady(playerId);

		// Ready needs a slot; un-ready is always allowed. The server validates too.
		if (!ready && !m_LobbyManager.FindSlotByPlayerId(playerId))
			return;

		m_LobbyPlayerComponent.AskSetReady(!ready);
	}

	protected void Action_Exit()
	{
		// During GAME the lobby closes; in every other state it is mandatory and "back"
		// brings the pause menu up on top.
		if (m_GameModeCoop && m_GameModeCoop.GetLobbyState() == SCR_EGameModeState.GAME)
		{
			Close();
			return;
		}

		// Opening a menu inline from an input event fights the menu manager.
		GetGame().GetCallqueue().CallLater(OpenPauseMenuWrap, 0);
	}

	protected void OpenPauseMenuWrap()
	{
		ArmaReforgerScripted.OpenPauseMenu();
	}

	protected void Action_ShowPlayerList()
	{
		SetVoiceTabActive(false);
	}

	protected void Action_ShowVoiceChannels()
	{
		SetVoiceTabActive(true);
	}

	protected void SetVoiceTabActive(bool voice)
	{
		if (m_wPlayersScroll)
			m_wPlayersScroll.SetVisible(!voice);
		if (m_wVoiceChannelsPanel)
			m_wVoiceChannelsPanel.SetVisible(voice);

		// The voice panel occupies the search box's rectangle.
		if (m_wPlayersSearch)
			m_wPlayersSearch.SetVisible(!voice);

		if (voice)
		{
			if (m_wPlayersSearchEmpty)
				m_wPlayersSearchEmpty.SetVisible(false);
		}
		else
		{
			ApplyPlayerFilter();
		}

		if (m_TabPlayerListComp)
			m_TabPlayerListComp.SetToggled(!voice, false);
		if (m_TabVoiceComp)
			m_TabVoiceComp.SetToggled(voice, false);
	}

	// Menu keybinds stand down while the search box owns the keyboard. The footer
	// buttons already disable themselves on the same signal.
	protected bool IsTypingInSearch()
	{
		return m_PlayerSearch && m_PlayerSearch.IsInWriteMode();
	}

	protected void Action_ChatKey()
	{
		if (IsTypingInSearch())
			return;

		Action_ChatOpen();
	}

	protected void Action_ChatOpen()
	{
		// Opening the chat inline from an input event fights the menu's own input handling.
		GetGame().GetCallqueue().CallLater(ChatOpenWrap, 0);
	}

	protected void ChatOpenWrap()
	{
		if (m_ChatPanel)
			SCR_ChatPanelManager.GetInstance().OpenChatPanel(m_ChatPanel);
	}

	// Admin-only: players cannot dismiss the mandatory lobby. Close only affects the local UI.
	protected void Action_LobbyKeyToggle()
	{
		if (IsTypingInSearch())
			return;

		// Still the tail of the U hold that opened the menu.
		if (GetGame().GetWorld().GetWorldTime() - m_fOpenedTime < 600)
			return;

		if (!SCR_Global.IsAdmin())
			return;

		Close();
	}

	protected float m_fOpenedTime;

	protected float m_fLastRosterCheck;

	protected void ClearChildWidgets(Widget parent)
	{
		if (!parent)
			return;

		// Bounded: a child that fails to detach would spin this loop into a no-callstack hang.
		int guard = 0;
		while (parent.GetChildren())
		{
			parent.GetChildren().RemoveFromHierarchy();

			if (++guard > 1000)
			{
				Print("[LL_DIAG] ClearChildWidgets aborted after 1000 iterations — a child would not detach (potential UI lock)", LogLevel.ERROR);
				break;
			}
		}
	}

	LL_LobbyManager GetLobbyManager()
	{
		return m_LobbyManager;
	}

	LL_LobbyPlayerComponent GetLobbyPlayerComponent()
	{
		return m_LobbyPlayerComponent;
	}

	// No-op when the layout has no preview pane.
	void SetPreviewSlot(int slotRplId)
	{
		if (m_LoadoutPreview)
			m_LoadoutPreview.SetPreviewSlot(slotRplId);
	}

	void SetPreviewVehicle(int vehicleRplId)
	{
		if (m_LoadoutPreview)
			m_LoadoutPreview.SetPreviewVehicle(vehicleRplId);
	}

	void ClearPreview()
	{
		if (m_LoadoutPreview)
			m_LoadoutPreview.ClearPreview();
	}

	// Lazy: on dedicated-server clients this menu auto-opens before the player id is
	// assigned. Ids start at 1.
	int GetLocalPlayerId()
	{
		if (m_iPlayerId <= 0)
		{
			PlayerController pc = GetGame().GetPlayerController();
			if (pc)
				m_iPlayerId = pc.GetPlayerId();
		}

		return m_iPlayerId;
	}

	// Falls back to the local player until an admin selects someone.
	int GetSelectedPlayer()
	{
		if (m_iSelectedPlayer <= 0)
			m_iSelectedPlayer = GetLocalPlayerId();

		return m_iSelectedPlayer;
	}

	void SetSelectedPlayer(int playerId)
	{
		int previous = m_iSelectedPlayer;
		if (previous == playerId)
			return;

		m_iSelectedPlayer = playerId;
		UpdatePlayerInList(previous);
		UpdatePlayerInList(playerId);
	}
}