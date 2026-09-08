// Alive-players panel of the spectator screen: collapsible squad groups, faction counter
// tabs, show-dead toggle and the alive-ratio bars. Built from LL_LobbyManager slots, so
// empty slots show their role name.

class LL_AlivePlayerList : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName GROUP_LAYOUT = "{7D84B5C90A2C6391}UI/Spectator/AlivePlayerGroup.layout";
	protected static const ResourceName FACTION_BUTTON_LAYOUT = "{7D84B5C90A2C6393}UI/Spectator/AliveFactionButton.layout";

	protected LL_LobbyManager m_LobbyManager;
	protected LL_SpectatorMenu m_Menu;

	protected VerticalLayoutWidget m_wPlayersList;
	protected HorizontalLayoutWidget m_wFactionsLayout;
	protected ScrollLayoutWidget m_wScroll;
	protected SCR_ButtonBaseComponent m_hShowDeathButton;

	protected ref map<int, LL_AlivePlayerGroup> m_mGroups = new map<int, LL_AlivePlayerGroup>();
	protected ref map<string, LL_AliveFactionButton> m_mFactionButtons = new map<string, LL_AliveFactionButton>();
	protected ref array<string> m_aSelectedFactions = {};

	protected ref ScriptInvokerBool m_OnShowDead = new ScriptInvokerBool();

	ScriptInvokerBool GetOnShowDead()
	{
		return m_OnShowDead;
	}

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wPlayersList = VerticalLayoutWidget.Cast(w.FindAnyWidget("AlivePlayersListLayout"));
		m_wFactionsLayout = HorizontalLayoutWidget.Cast(w.FindAnyWidget("HorizontalLayoutFactions"));
		m_wScroll = ScrollLayoutWidget.Cast(w.FindAnyWidget("AlivePlayersListScroll"));

		Widget showDeath = w.FindAnyWidget("ShowDeathButton");
		if (showDeath)
			m_hShowDeathButton = SCR_ButtonBaseComponent.Cast(showDeath.FindHandler(SCR_ButtonBaseComponent));
		if (m_hShowDeathButton)
			m_hShowDeathButton.m_OnClicked.Insert(OnShowDeadClicked);
	}

	override void HandlerDeattached(Widget w)
	{
		if (m_LobbyManager)
		{
			m_LobbyManager.GetOnSlotRegistered().Remove(OnSlotRegistered);
			m_LobbyManager.GetOnSlotUnregistered().Remove(OnSlotUnregistered);
			m_LobbyManager.GetOnSlotUpdated().Remove(OnSlotUpdated);
		}

		super.HandlerDeattached(w);
	}

	void Init(LL_SpectatorMenu menu)
	{
		m_Menu = menu;
		m_LobbyManager = LL_LobbyManager.GetInstance();
		if (!m_LobbyManager)
			return;

		// Slots arrive in registration order, which interleaves factions; the list shows
		// contiguous faction blocks in scenario order.
		array<ref LL_SlotData> slots = m_LobbyManager.GetSlots();

		FactionManager factionManager = GetGame().GetFactionManager();
		array<Faction> factions = {};
		if (factionManager)
			factionManager.GetFactionsList(factions);

		foreach (Faction faction : factions)
		{
			string factionKey = faction.GetFactionKey();
			foreach (LL_SlotData slot : slots)
			{
				if (slot.m_sFactionKey == factionKey)
					AddSlot(slot);
			}
		}

		// A slot whose faction key resolves to no faction is still listed.
		foreach (LL_SlotData slot : slots)
		{
			if (!factionManager || !factionManager.GetFactionByKey(slot.m_sFactionKey))
				AddSlot(slot);
		}

		UpdateFactionCounts();

		m_LobbyManager.GetOnSlotRegistered().Insert(OnSlotRegistered);
		m_LobbyManager.GetOnSlotUnregistered().Insert(OnSlotUnregistered);
		m_LobbyManager.GetOnSlotUpdated().Insert(OnSlotUpdated);
	}

	bool IsShowDead()
	{
		return m_hShowDeathButton && m_hShowDeathButton.IsToggled();
	}

	LL_SpectatorMenu GetMenu()
	{
		return m_Menu;
	}

	protected void AddSlot(LL_SlotData slot)
	{
		LL_AlivePlayerGroup group;
		if (!m_mGroups.Find(slot.m_iGroupId, group))
		{
			Widget groupRoot = GetGame().GetWorkspace().CreateWidgets(GROUP_LAYOUT, m_wPlayersList);
			if (!groupRoot)
				return;

			group = LL_AlivePlayerGroup.Cast(groupRoot.FindHandler(LL_AlivePlayerGroup));
			if (!group)
				return;

			group.InitGroup(slot.m_iGroupId, slot.m_sGroupName, slot.m_sFactionKey, this);
			groupRoot.SetVisible(IsFactionSelected(slot.m_sFactionKey));
			m_mGroups.Insert(slot.m_iGroupId, group);
		}

		group.InsertSlot(slot);
	}

	void OnGroupRemoved(int groupId)
	{
		m_mGroups.Remove(groupId);
	}

	protected void UpdateFactionCounts()
	{
		map<string, int> aliveCounts = new map<string, int>();
		foreach (LL_SlotData slot : m_LobbyManager.GetSlots())
		{
			int alive = 0;
			if (!slot.IsDestroyed())
				alive = 1;

			int current;
			if (aliveCounts.Find(slot.m_sFactionKey, current))
				aliveCounts.Set(slot.m_sFactionKey, current + alive);
			else
				aliveCounts.Insert(slot.m_sFactionKey, alive);
		}

		foreach (string factionKey, int count : aliveCounts)
		{
			LL_AliveFactionButton button;
			if (!m_mFactionButtons.Find(factionKey, button))
			{
				Widget buttonRoot = GetGame().GetWorkspace().CreateWidgets(FACTION_BUTTON_LAYOUT, m_wFactionsLayout);
				if (!buttonRoot)
					continue;

				button = LL_AliveFactionButton.Cast(buttonRoot.FindHandler(LL_AliveFactionButton));
				if (!button)
					continue;

				button.SetFaction(factionKey);
				button.m_OnClicked.Insert(OnFactionButtonClicked);
				m_mFactionButtons.Insert(factionKey, button);

				if (!m_aSelectedFactions.Contains(factionKey))
					m_aSelectedFactions.Insert(factionKey);
			}

			button.SetAliveCount(count);
		}

		// Groups created before their faction entered the selection spawned hidden.
		ApplyFactionVisibility();
	}

	protected void ApplyFactionVisibility()
	{
		foreach (int groupId, LL_AlivePlayerGroup group : m_mGroups)
		{
			group.GetRootWidget().SetVisible(IsFactionSelected(group.GetFactionKey()));
		}
	}

	//! The spectator map draws the same slots, so the faction tabs filter both screens.
	bool IsFactionSelected(string factionKey)
	{
		return m_aSelectedFactions.Contains(factionKey);
	}

	protected void OnSlotRegistered(LL_SlotData slot)
	{
		AddSlot(slot);
		UpdateFactionCounts();
	}

	protected void OnSlotUnregistered(int rplId)
	{
		UpdateFactionCounts();
	}

	protected void OnSlotUpdated(LL_SlotData slot)
	{
		UpdateFactionCounts();
	}

	protected void OnFactionButtonClicked(SCR_ButtonBaseComponent button)
	{
		if (m_wScroll)
			m_wScroll.SetSliderPos(0, 0);

		LL_AliveFactionButton factionButton = LL_AliveFactionButton.Cast(button);
		if (!factionButton)
			return;

		string factionKey = factionButton.GetFactionKey();
		if (m_aSelectedFactions.Contains(factionKey))
			m_aSelectedFactions.RemoveItem(factionKey);
		else
			m_aSelectedFactions.Insert(factionKey);

		ApplyFactionVisibility();
	}

	protected void OnShowDeadClicked(SCR_ButtonBaseComponent button)
	{
		m_OnShowDead.Invoke(IsShowDead());
	}
}

class LL_AlivePlayerGroup : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName ROW_LAYOUT = "{7D84B5C90A2C6392}UI/Spectator/AlivePlayerRow.layout";

	protected VerticalLayoutWidget m_wPlayersLayout;
	protected TextWidget m_wGroupName;
	protected ImageWidget m_wGroupFactionColor;
	protected SCR_ButtonBaseComponent m_hGroupButton;

	protected LL_AlivePlayerList m_List;
	protected int m_iGroupId;
	protected string m_sFactionKey;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wPlayersLayout = VerticalLayoutWidget.Cast(w.FindAnyWidget("PlayersVerticalLayout"));
		m_wGroupName = TextWidget.Cast(w.FindAnyWidget("GroupName"));
		m_wGroupFactionColor = ImageWidget.Cast(w.FindAnyWidget("GroupFactionColor"));

		Widget button = w.FindAnyWidget("AlivePlayerGroupButton");
		if (button)
			m_hGroupButton = SCR_ButtonBaseComponent.Cast(button.FindHandler(SCR_ButtonBaseComponent));
		if (m_hGroupButton)
			m_hGroupButton.m_OnClicked.Insert(OnGroupClicked);
	}

	void InitGroup(int groupId, string groupName, string factionKey, LL_AlivePlayerList list)
	{
		m_iGroupId = groupId;
		m_sFactionKey = factionKey;
		m_List = list;

		if (m_wGroupName)
		{
			if (groupName != "")
				m_wGroupName.SetText(groupName);
			else
				m_wGroupName.SetText("#LL-Group_Fallback");
		}

		if (m_wGroupFactionColor)
		{
			Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
			if (faction)
				m_wGroupFactionColor.SetColor(faction.GetFactionColor());
		}
	}

	string GetFactionKey()
	{
		return m_sFactionKey;
	}

	void InsertSlot(LL_SlotData slot)
	{
		Widget rowRoot = GetGame().GetWorkspace().CreateWidgets(ROW_LAYOUT, m_wPlayersLayout);
		if (!rowRoot)
			return;

		LL_AlivePlayerRow row = LL_AlivePlayerRow.Cast(rowRoot.FindHandler(LL_AlivePlayerRow));
		if (row)
			row.InitRow(slot, this, m_List);
	}

	void OnRowRemoved()
	{
		if (m_wPlayersLayout && !m_wPlayersLayout.GetChildren())
		{
			m_wRoot.RemoveFromHierarchy();
			if (m_List)
				m_List.OnGroupRemoved(m_iGroupId);
		}
	}

	protected void OnGroupClicked(SCR_ButtonBaseComponent button)
	{
		if (m_wPlayersLayout)
			m_wPlayersLayout.SetVisible(!m_wPlayersLayout.IsVisible());
	}
}

class LL_AlivePlayerRow : SCR_ButtonBaseComponent
{
	protected ref Color m_DeathColor = Color.FromInt(0xFF2c2c2c);

	protected ImageWidget m_wUnitIcon;
	protected ImageWidget m_wDeadIcon;
	protected RichTextWidget m_wPlayerName;
	protected ImageWidget m_wFactionColor;

	protected LL_AlivePlayerGroup m_Group;
	protected LL_AlivePlayerList m_List;
	protected int m_iSlotRplId = -1;
	protected int m_iPlayerId = -1;
	protected bool m_bDead;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wUnitIcon = ImageWidget.Cast(w.FindAnyWidget("UnitIcon"));
		m_wDeadIcon = ImageWidget.Cast(w.FindAnyWidget("DeadIcon"));
		m_wPlayerName = RichTextWidget.Cast(w.FindAnyWidget("PlayerName"));
		m_wFactionColor = ImageWidget.Cast(w.FindAnyWidget("PlayerFactionColor"));
	}

	override void HandlerDeattached(Widget w)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnSlotUpdated().Remove(OnSlotUpdated);
			mgr.GetOnSlotUnregistered().Remove(OnSlotUnregistered);
			mgr.GetOnPlayerAssigned().Remove(OnPlayerSlotChanged);
			mgr.GetOnPlayerUnassigned().Remove(OnPlayerSlotChanged);
			mgr.GetOnPlayerNameUpdated().Remove(OnPlayerNameUpdated);
		}

		if (m_List)
			m_List.GetOnShowDead().Remove(UpdateShowDead);

		super.HandlerDeattached(w);
	}

	void InitRow(LL_SlotData slot, LL_AlivePlayerGroup group, LL_AlivePlayerList list)
	{
		m_iSlotRplId = slot.m_iRplId;
		m_Group = group;
		m_List = list;

		if (m_wFactionColor)
		{
			Faction faction = GetGame().GetFactionManager().GetFactionByKey(slot.m_sFactionKey);
			if (faction)
				m_wFactionColor.SetColor(faction.GetFactionColor());
		}

		if (m_wUnitIcon && slot.m_sIconPath != "")
		{
			if (slot.m_sIconName != "")
				m_wUnitIcon.LoadImageFromSet(0, slot.m_sIconPath, slot.m_sIconName);
			else
				m_wUnitIcon.LoadImageTexture(0, slot.m_sIconPath);
		}

		UpdateFromSlot(slot);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnSlotUpdated().Insert(OnSlotUpdated);
			mgr.GetOnSlotUnregistered().Insert(OnSlotUnregistered);
			mgr.GetOnPlayerAssigned().Insert(OnPlayerSlotChanged);
			mgr.GetOnPlayerUnassigned().Insert(OnPlayerSlotChanged);
			mgr.GetOnPlayerNameUpdated().Insert(OnPlayerNameUpdated);
		}

		m_List.GetOnShowDead().Insert(UpdateShowDead);

		m_OnClicked.Insert(OnRowClicked);
	}

	int GetSlotRplId()
	{
		return m_iSlotRplId;
	}

	protected void UpdateFromSlot(LL_SlotData slot)
	{
		m_iPlayerId = slot.m_iPlayerId;

		if (m_wPlayerName)
		{
			string name = "";
			LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
			if (mgr && slot.m_iPlayerId >= 0)
				name = mgr.GetPlayerName(slot.m_iPlayerId);
			if (name == "")
				name = slot.m_sName;

			m_wPlayerName.SetText(LL_LobbyManager.FormatPlayerNameRich(name));
		}

		m_bDead = slot.IsDestroyed();
		if (m_bDead)
		{
			if (m_wUnitIcon)
				m_wUnitIcon.SetVisible(false);
			if (m_wDeadIcon)
			{
				m_wDeadIcon.SetVisible(true);
				m_wDeadIcon.SetColor(m_DeathColor);
			}
			if (m_wPlayerName)
				m_wPlayerName.SetColor(m_DeathColor);
		}
		else
		{
			if (m_wUnitIcon)
				m_wUnitIcon.SetVisible(true);
			if (m_wDeadIcon)
				m_wDeadIcon.SetVisible(false);
			if (m_wPlayerName)
				m_wPlayerName.SetColor(Color.White);
		}

		UpdateShowDead(m_List.IsShowDead());
	}

	protected void UpdateShowDead(bool showDead)
	{
		m_wRoot.SetVisible(showDead || !m_bDead);
	}

	protected void RefreshFromManager()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		LL_SlotData slot = mgr.FindSlotByRplId(m_iSlotRplId);
		if (slot)
			UpdateFromSlot(slot);
	}

	protected void OnSlotUpdated(LL_SlotData slot)
	{
		if (slot && slot.m_iRplId == m_iSlotRplId)
			UpdateFromSlot(slot);
	}

	protected void OnSlotUnregistered(int rplId)
	{
		if (rplId != m_iSlotRplId)
			return;

		LL_AlivePlayerGroup group = m_Group;
		m_wRoot.RemoveFromHierarchy();
		if (group)
			group.OnRowRemoved();
	}

	protected void OnPlayerSlotChanged(int playerId, int slotRplId)
	{
		if (slotRplId == m_iSlotRplId)
			RefreshFromManager();
	}

	protected void OnPlayerNameUpdated(int playerId, string name)
	{
		if (playerId == m_iPlayerId)
			RefreshFromManager();
	}

	protected void OnRowClicked(SCR_ButtonBaseComponent button)
	{
		LL_SpectatorMenu menu = m_List.GetMenu();
		if (menu)
			menu.OnRowClicked(m_iSlotRplId);
	}
}

class LL_AliveFactionButton : SCR_ButtonBaseComponent
{
	protected ImageWidget m_wBackgroundFaction;
	protected TextWidget m_wSideCount;

	protected string m_sFactionKey;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wBackgroundFaction = ImageWidget.Cast(w.FindAnyWidget("BackgroundFaction"));
		m_wSideCount = TextWidget.Cast(w.FindAnyWidget("SideCount"));
	}

	void SetFaction(string factionKey)
	{
		m_sFactionKey = factionKey;

		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(factionKey));
		if (faction && m_wBackgroundFaction)
			m_wBackgroundFaction.SetColor(faction.GetOutlineFactionColor());
	}

	string GetFactionKey()
	{
		return m_sFactionKey;
	}

	void SetAliveCount(int countAlive)
	{
		if (m_wSideCount)
			m_wSideCount.SetText(countAlive.ToString());
	}
}

// One coloured line per faction, width weighted by its alive count.
class LL_SidesRatio : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName LINE_LAYOUT = "{7D84B5C90A2C6394}UI/Spectator/SideRatioLine.layout";

	protected ref map<string, LL_SidesRatioLine> m_mLines = new map<string, LL_SidesRatioLine>();

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (!GetGame().InPlayMode())
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnSlotRegistered().Insert(OnSlotsChanged);
			mgr.GetOnSlotUnregistered().Insert(OnSlotsChangedInt);
			mgr.GetOnSlotUpdated().Insert(OnSlotsChanged);
		}

		UpdateInfo();
	}

	override void HandlerDeattached(Widget w)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnSlotRegistered().Remove(OnSlotsChanged);
			mgr.GetOnSlotUnregistered().Remove(OnSlotsChangedInt);
			mgr.GetOnSlotUpdated().Remove(OnSlotsChanged);
		}

		super.HandlerDeattached(w);
	}

	protected void OnSlotsChanged(LL_SlotData slot)	{ UpdateInfo(); }
	protected void OnSlotsChangedInt(int rplId)		{ UpdateInfo(); }

	protected void UpdateInfo()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		map<string, int> aliveCounts = new map<string, int>();
		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.IsDestroyed())
				continue;

			int current;
			if (aliveCounts.Find(slot.m_sFactionKey, current))
				aliveCounts.Set(slot.m_sFactionKey, current + 1);
			else
				aliveCounts.Insert(slot.m_sFactionKey, 1);
		}

		array<string> toRemove = {};
		foreach (string factionKey, LL_SidesRatioLine line : m_mLines)
		{
			if (!aliveCounts.Contains(factionKey))
			{
				line.GetRootWidget().RemoveFromHierarchy();
				toRemove.Insert(factionKey);
			}
		}
		foreach (string factionKey : toRemove)
		{
			m_mLines.Remove(factionKey);
		}

		foreach (string factionKey, int count : aliveCounts)
		{
			LL_SidesRatioLine line;
			if (!m_mLines.Find(factionKey, line))
			{
				Widget lineRoot = GetGame().GetWorkspace().CreateWidgets(LINE_LAYOUT, m_wRoot);
				if (!lineRoot)
					continue;

				line = LL_SidesRatioLine.Cast(lineRoot.FindHandler(LL_SidesRatioLine));
				if (!line)
					continue;

				line.SetFaction(factionKey);
				m_mLines.Insert(factionKey, line);
			}

			line.SetCount(count);
		}
	}
}

class LL_SidesRatioLine : SCR_ScriptedWidgetComponent
{
	void SetFaction(string factionKey)
	{
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
		if (faction)
		{
			PanelWidget panel = PanelWidget.Cast(m_wRoot);
			if (panel)
				panel.SetColor(faction.GetFactionColor());
		}
	}

	void SetCount(int count)
	{
		HorizontalLayoutSlot.SetFillWeight(m_wRoot, count);
	}
}