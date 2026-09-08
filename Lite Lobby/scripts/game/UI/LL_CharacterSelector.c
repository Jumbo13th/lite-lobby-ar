// One playable slot row in the lobby. Reads LL_SlotData, sends actions through
// LL_LobbyPlayerComponent.

class LL_CharacterSelector : SCR_ButtonBaseComponent
{
	protected ImageWidget m_wFactionColor;
	protected ImageWidget m_wUnitIcon;
	protected TextWidget m_wCharacterClassName;
	protected ImageWidget m_wStateIcon;
	protected RichTextWidget m_wCharacterStatus;
	protected ButtonWidget m_wStateButton;

	protected int m_iSlotRplId = -1;
	protected LL_CoopLobby m_CoopLobby;
	protected int m_iCurrentPlayerId = -1;
	protected bool m_bCurrentLocked;

	void Init(LL_SlotData slot, LL_CoopLobby lobby)
	{
		m_iSlotRplId = slot.m_iRplId;
		m_CoopLobby = lobby;

		Widget root = GetRootWidget();
		if (!root)
			return;

		m_wFactionColor = ImageWidget.Cast(root.FindAnyWidget("CharacterFactionColor"));
		m_wUnitIcon = ImageWidget.Cast(root.FindAnyWidget("UnitIcon"));
		m_wCharacterClassName = TextWidget.Cast(root.FindAnyWidget("CharacterClassName"));
		m_wStateIcon = ImageWidget.Cast(root.FindAnyWidget("StateIcon"));
		m_wCharacterStatus = RichTextWidget.Cast(root.FindAnyWidget("CharacterStatus"));
		m_wStateButton = ButtonWidget.Cast(root.FindAnyWidget("StateButton"));

		// Unmanaged widgets show broken defaults.
		Widget voiceBtn = root.FindAnyWidget("VoiceHideableButton");
		if (voiceBtn)
			voiceBtn.SetVisible(false);

		if (m_wStateButton)
			m_wStateButton.SetVisible(false);

		if (m_wCharacterClassName)
			m_wCharacterClassName.SetText(slot.m_sName);

		// Resolved server-side at registration; the layout default is a placeholder.
		if (m_wUnitIcon && slot.m_sIconPath != "")
		{
			if (slot.m_sIconName != "")
				m_wUnitIcon.LoadImageFromSet(0, slot.m_sIconPath, slot.m_sIconName);
			else
				m_wUnitIcon.LoadImageTexture(0, slot.m_sIconPath);
		}

		if (m_wFactionColor)
		{
			SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (fm)
			{
				SCR_Faction faction = SCR_Faction.Cast(fm.GetFactionByKey(slot.m_sFactionKey));
				if (faction)
					m_wFactionColor.SetColor(faction.GetFactionColor());
			}
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnSlotUpdated().Insert(OnSlotUpdated);
			mgr.GetOnPlayerAssigned().Insert(OnPlayerChanged);
			mgr.GetOnPlayerUnassigned().Insert(OnPlayerChanged);
			mgr.GetOnPlayerReadyChanged().Insert(OnPlayerFlagChanged);
			mgr.GetOnPlayerConnectionChanged().Insert(OnPlayerFlagChanged);
			mgr.GetOnWebsiteOccupantsChanged().Insert(OnWebsiteOccupantsChanged);
		}

		UpdateDisplay(slot);

		m_OnClicked.Insert(OnClicked);
	}

	protected void UpdateDisplay(LL_SlotData slot)
	{
		if (!slot)
			return;

		int oldPlayerId = m_iCurrentPlayerId;
		m_iCurrentPlayerId = slot.m_iPlayerId;

		bool oldLocked = m_bCurrentLocked;
		m_bCurrentLocked = slot.m_bLocked;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		PlayerManager pm = GetGame().GetPlayerManager();

		// Manager state, not the live IsPlayerConnected: the engine's view is not reliably
		// flipped at notification time and is not stored for rebuilds or JIP.
		bool occupantDisconnected = slot.m_iPlayerId >= 0 && mgr && mgr.IsPlayerDisconnected(slot.m_iPlayerId);

		// Website reservation, appended to the status so everyone sees who the site assigned
		// here even after someone in-game takes the slot.
		string webLabel = "";
		if (mgr)
			webLabel = mgr.GetWebsiteOccupant(m_iSlotRplId);

		if (m_wCharacterStatus)
		{
			if (slot.m_bLocked)
			{
				m_wCharacterStatus.SetText(AppendWebsiteLabel(WidgetManager.Translate("#LL-Slot_Locked"), webLabel));
				m_wCharacterStatus.SetColor(Color.Gray);
			}
			else if (slot.IsDestroyed())
			{
				m_wCharacterStatus.SetText("#LL-Slot_KIA");
				m_wCharacterStatus.SetColor(Color.FromInt(0xFF2c2c2c));
			}
			else if (slot.m_iPlayerId >= 0)
			{
				string playerName = "";
				if (mgr)
					playerName = mgr.GetPlayerName(slot.m_iPlayerId);

				if (playerName == "" && pm)
					playerName = pm.GetPlayerName(slot.m_iPlayerId);

				m_wCharacterStatus.SetText(AppendWebsiteLabel(LL_LobbyManager.FormatPlayerNameRich(playerName), webLabel));

				// Priority: disconnected → ready → admin → default.
				if (occupantDisconnected)
					m_wCharacterStatus.SetColor(Color.FromInt(0xFF2c2c2c));
				else if (mgr && mgr.IsPlayerReady(slot.m_iPlayerId))
					m_wCharacterStatus.SetColor(Color.Green);
				else if (SCR_Global.IsAdmin(slot.m_iPlayerId))
					m_wCharacterStatus.SetColor(Color.FromInt(0xfff2a34b));
				else
					m_wCharacterStatus.SetColor(Color.White);
			}
			else
			{
				m_wCharacterStatus.SetText(AppendWebsiteLabel("", webLabel));
				m_wCharacterStatus.SetColor(Color.White);
			}
		}

		if (m_wStateIcon)
		{
			if (slot.m_bLocked)
			{
				m_wStateIcon.SetVisible(true);
				// The imageset has no "Locked" quad, and a missing quad draws the whole atlas.
				m_wStateIcon.LoadImageFromSet(0, "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "lockedCircle");
			}
			else if (slot.IsDestroyed())
			{
				m_wStateIcon.SetVisible(true);
				m_wStateIcon.LoadImageFromSet(0, "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "death");
			}
			else if (occupantDisconnected)
			{
				m_wStateIcon.SetVisible(true);
				m_wStateIcon.LoadImageFromSet(0, "{D17288006833490F}UI/Textures/Icons/icons_wrapperUI-32.imageset", "disconnection");
			}
			else
			{
				m_wStateIcon.SetVisible(false);
			}
		}

		// The squad header tracks both for its counter line.
		if (oldPlayerId != m_iCurrentPlayerId || oldLocked != m_bCurrentLocked)
		{
			LL_RolesGroup parentGroup = GetParentRolesGroup();
			if (parentGroup)
			{
				if (oldPlayerId != m_iCurrentPlayerId)
					parentGroup.OnSlotPlayerChanged(oldPlayerId, m_iCurrentPlayerId);
				if (oldLocked != m_bCurrentLocked)
					parentGroup.OnSlotLockChanged(oldLocked, m_bCurrentLocked);
			}
		}
	}

	protected void OnSlotUpdated(LL_SlotData slot)
	{
		if (!slot || slot.m_iRplId != m_iSlotRplId)
			return;

		UpdateDisplay(slot);
	}

	protected void RefreshFromManager()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		LL_SlotData slot = mgr.FindSlotByRplId(m_iSlotRplId);
		if (slot)
			UpdateDisplay(slot);
	}

	protected void OnPlayerChanged(int playerId, int slotRplId)
	{
		if (slotRplId != m_iSlotRplId)
			return;

		RefreshFromManager();
	}

	protected void OnPlayerFlagChanged(int playerId, bool value)
	{
		if (playerId != m_iCurrentPlayerId)
			return;

		RefreshFromManager();
	}

	protected void OnWebsiteOccupantsChanged()
	{
		RefreshFromManager();
	}

	// Muted secondary grey: accent orange read as a real occupant next to the unit-tag
	// rename, and a new colour read as foreign to the lobby.
	protected static string AppendWebsiteLabel(string text, string webLabel)
	{
		if (webLabel == "")
			return text;

		string tag = string.Format("<color rgba=\"160,160,160,255\">(%1)</color>", webLabel);
		if (text == "")
			return tag;

		return text + " " + tag;
	}

	protected void OnClicked()
	{
		if (!m_CoopLobby)
			return;

		LL_LobbyPlayerComponent lobbyPlayer = m_CoopLobby.GetLobbyPlayerComponent();
		if (!lobbyPlayer)
			return;

		LL_LobbyManager mgr = m_CoopLobby.GetLobbyManager();
		if (!mgr)
			return;

		int localPlayerId = m_CoopLobby.GetLocalPlayerId();
		LL_SlotData slot = mgr.FindSlotByRplId(m_iSlotRplId);
		if (!slot)
			return;

		// Admin move-to-slot: with another player selected, clicking an available slot
		// moves that player here. GetSelectedPlayer defaults to the local player.
		int selectedPlayerId = m_CoopLobby.GetSelectedPlayer();
		if (selectedPlayerId > 0 && selectedPlayerId != localPlayerId && SCR_Global.IsAdmin())
		{
			if (slot.IsAvailable())
			{
				lobbyPlayer.AskAssignPlayerToSlot(selectedPlayerId, m_iSlotRplId);
				m_CoopLobby.SetSelectedPlayer(localPlayerId);
			}
			// Occupied, locked or dead: the selection stays so the admin can click another slot.
		}
		else if (slot.m_iPlayerId == localPlayerId)
		{
			lobbyPlayer.AskLeaveSlot();
		}
		else if (slot.IsAvailable())
		{
			lobbyPlayer.AskTakeSlot(m_iSlotRplId);
		}

		// Enter doubles as the footer Ready action and the activate-focused-widget key; a
		// still-focused row would re-click itself instead of toggling Ready.
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			workspace.SetFocusedWidget(null);
	}

	// SCR_ButtonBaseComponent invokes m_OnClicked for the left button only; the right
	// button opens the admin menu.
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);

		if (button == 1)
			OpenContextMenu();

		return false;
	}

	// Hover updates the preview without stealing keyboard focus from the footer.
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		super.OnMouseEnter(w, x, y);

		if (m_CoopLobby)
			m_CoopLobby.SetPreviewSlot(m_iSlotRplId);

		return false;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		super.OnMouseLeave(w, enterW, x, y);

		if (m_CoopLobby)
			m_CoopLobby.ClearPreview();

		return false;
	}

	protected void OpenContextMenu()
	{
		if (!m_CoopLobby)
			return;

		LL_LobbyManager mgr = m_CoopLobby.GetLobbyManager();
		if (!mgr)
			return;

		LL_SlotData slot = mgr.FindSlotByRplId(m_iSlotRplId);
		if (!slot)
			return;

		int targetId = slot.m_iPlayerId;
		bool occupied = targetId >= 0;
		bool isAdmin = SCR_Global.IsAdmin();

		// Non-admins get a menu only on occupied slots (the occupant's unit statistics).
		if (!isAdmin && !occupied)
			return;

		string header;
		if (occupied)
			header = LL_LobbyManager.FormatPlayerNameRich(mgr.GetPlayerName(targetId));
		else
			header = slot.m_sName;

		LL_ContextMenu menu = LL_ContextMenu.Create(m_CoopLobby, header);
		if (!menu)
			return;

		if (occupied)
		{
			menu.AddUnitStatsAction(targetId);

			// Own slot: nothing to administer.
			if (!isAdmin || targetId == m_CoopLobby.GetLocalPlayerId())
				return;

			// Locking an occupied slot would orphan its character; the admin kicks first.
			menu.AddSelectAction(targetId);
			menu.AddKickActions(targetId);
		}
		else
		{
			menu.AddSlotLockAction(m_iSlotRplId, slot.m_bLocked);
		}
	}

	protected LL_RolesGroup GetParentRolesGroup()
	{
		Widget root = GetRootWidget();
		if (!root)
			return null;

		Widget parent = root.GetParent();
		while (parent)
		{
			LL_RolesGroup group = LL_RolesGroup.Cast(parent.FindHandler(LL_RolesGroup));
			if (group)
				return group;

			parent = parent.GetParent();
		}

		return null;
	}

	int GetSlotRplId()
	{
		return m_iSlotRplId;
	}
}