// Squad container in the lobby: header with faction colour and counters, slot rows and
// vehicle rows.

class LL_RolesGroup : SCR_ScriptedWidgetComponent
{
	protected ResourceName m_sCharacterSelectorPrefab = "{3F761F63F1DF29D1}UI/Lobby/CharacterSelector.layout";
	protected ResourceName m_sVehicleSelectorPrefab = "{6F2A1C8E4B05D7A3}UI/Lobby/VehicleSelector.layout";

	protected TextWidget m_wRolesGroupName;
	protected RichTextWidget m_wRolesGroupNameCustom;
	protected ImageWidget m_wGroupFactionColor;
	protected VerticalLayoutWidget m_wCharactersList;
	protected VerticalLayoutWidget m_wList;
	protected ButtonWidget m_wVoiceJoinButton;
	protected ButtonWidget m_wRolesGroupButton;

	protected string m_sFactionKey;
	protected int m_iGroupId;
	protected LL_CoopLobby m_CoopLobby;
	protected ref array<LL_CharacterSelector> m_aCharacterSelectors = {};
	protected ref array<LL_VehicleSelector> m_aVehicleSelectors = {};
	protected bool m_bFolded;
	protected int m_iSlotsCount;
	protected int m_iPlayersCount;
	protected int m_iLockedCount;

	void Init(string factionKey, int groupId, string groupName, LL_CoopLobby lobby)
	{
		m_sFactionKey = factionKey;
		m_iGroupId = groupId;
		m_CoopLobby = lobby;

		Widget root = GetRootWidget();
		if (!root)
			return;

		m_wRolesGroupName = TextWidget.Cast(root.FindAnyWidget("RolesGroupName"));
		m_wRolesGroupNameCustom = RichTextWidget.Cast(root.FindAnyWidget("RolesGroupNameCustom"));
		m_wGroupFactionColor = ImageWidget.Cast(root.FindAnyWidget("GroupFactionColor"));
		m_wCharactersList = VerticalLayoutWidget.Cast(root.FindAnyWidget("CharactersList"));
		m_wList = VerticalLayoutWidget.Cast(root.FindAnyWidget("List"));
		m_wVoiceJoinButton = ButtonWidget.Cast(root.FindAnyWidget("VoiceJoinButton"));
		m_wRolesGroupButton = ButtonWidget.Cast(root.FindAnyWidget("RolesGroupButton"));

		if (m_wGroupFactionColor)
		{
			SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (factionManager)
			{
				SCR_Faction faction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));
				if (faction)
					m_wGroupFactionColor.SetColor(faction.GetFactionColor());
			}
		}

		if (m_wRolesGroupName)
		{
			if (groupName != "")
				m_wRolesGroupName.SetText(groupName);
			else
				m_wRolesGroupName.SetText("#LL-Group_Fallback");
		}

		// One handler for both clicks; the back-ref lets its right-click reach this group.
		if (m_wRolesGroupButton)
		{
			LL_GroupHeaderButton headerBtn = LL_GroupHeaderButton.Cast(
				m_wRolesGroupButton.FindHandler(LL_GroupHeaderButton));
			if (headerBtn)
			{
				headerBtn.m_OnClicked.Insert(OnGroupClicked);
				headerBtn.SetRolesGroup(this);
			}
		}
	}

	void AddSlot(LL_SlotData slot)
	{
		if (!m_wCharactersList)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget charWidget = workspace.CreateWidgets(m_sCharacterSelectorPrefab, m_wCharactersList);
		if (!charWidget)
			return;

		LL_CharacterSelector charSelector = LL_CharacterSelector.Cast(
			charWidget.FindHandler(LL_CharacterSelector));
		if (!charSelector)
			return;

		charSelector.Init(slot, m_CoopLobby);
		m_aCharacterSelectors.Insert(charSelector);

		m_iSlotsCount++;
		// Occupied and locked are counted only through the slot-changed callbacks that
		// CharacterSelector.Init fires; counting here as well double-counted.

		UpdateCustomName();
	}

	void AddVehicle(LL_VehicleData vehicle)
	{
		if (!m_wCharactersList || !vehicle)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget vehicleWidget = workspace.CreateWidgets(m_sVehicleSelectorPrefab, m_wCharactersList);
		if (!vehicleWidget)
			return;

		LL_VehicleSelector vehicleSelector = LL_VehicleSelector.Cast(
			vehicleWidget.FindHandler(LL_VehicleSelector));
		if (!vehicleSelector)
			return;

		vehicleSelector.Init(vehicle, m_CoopLobby);
		m_aVehicleSelectors.Insert(vehicleSelector);
	}

	void UpdateCustomName()
	{
		if (!m_wRolesGroupNameCustom)
			return;

		int available = m_iSlotsCount - m_iLockedCount;
		if (m_iLockedCount >= m_iSlotsCount)
			m_wRolesGroupNameCustom.SetText("#LL-Group_LockedTag");
		else
			m_wRolesGroupNameCustom.SetTextFormat("(%1/%2)", m_iPlayersCount, available);
	}

	void OnSlotPlayerChanged(int oldPlayerId, int newPlayerId)
	{
		if (oldPlayerId >= 0)
			m_iPlayersCount--;
		if (newPlayerId >= 0)
			m_iPlayersCount++;

		UpdateCustomName();
	}

	void OnSlotLockChanged(bool wasLocked, bool nowLocked)
	{
		if (!wasLocked && nowLocked)
			m_iLockedCount++;
		else if (wasLocked && !nowLocked)
			m_iLockedCount--;

		UpdateCustomName();
	}

	protected void OnGroupClicked()
	{
		m_bFolded = !m_bFolded;
		if (m_wList)
			m_wList.SetVisible(!m_bFolded);
	}

	// Admin-gated for fast feedback; the RPC re-validates on the server.
	void OpenGroupContextMenu()
	{
		if (!m_CoopLobby || !SCR_Global.IsAdmin())
			return;

		LL_LobbyManager mgr = m_CoopLobby.GetLobbyManager();
		if (!mgr)
			return;

		string header = "#LL-Group_Fallback";
		if (m_wRolesGroupName)
			header = m_wRolesGroupName.GetText();

		LL_ContextMenu menu = LL_ContextMenu.Create(m_CoopLobby, header);
		if (menu)
			menu.AddGroupLockAction(m_iGroupId, mgr.IsGroupLocked(m_iGroupId));
	}

	string GetFactionKey()
	{
		return m_sFactionKey;
	}

	int GetGroupId()
	{
		return m_iGroupId;
	}
}