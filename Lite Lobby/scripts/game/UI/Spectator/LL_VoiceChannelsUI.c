// The Voice Channels panel shared by the lobby tab, the briefing side panel and the
// spectator screen. Rooms listed by context: slot selection shows the full own-faction
// set, briefing shows HQ plus squads only (leaders see every squad of their faction),
// spectators see Deafen, Global and Public only. A header click asks the server to join;
// the same validation runs here first for instant feedback. Rows show a talking
// indicator from the VoN receive/capture events; it is status only.

class LL_VoiceChannelList : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName ROOM_LAYOUT = "{7D84B5C90A2C6395}UI/Spectator/VoiceChannelRoom.layout";

	protected VerticalLayoutWidget m_wRoomsList;
	protected bool m_bRebuildQueued;
	protected int m_iLocalPlayerId;

	protected ref map<int, LL_VoiceChannelPlayerRow> m_mPlayerRows = new map<int, LL_VoiceChannelPlayerRow>();

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (!GetGame().InPlayMode())
			return;

		m_wRoomsList = VerticalLayoutWidget.Cast(w.FindAnyWidget("RoomsList"));

		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
		{
			vonMgr.GetOnPlayerChannelChanged().Insert(OnChannelsChanged);
			vonMgr.GetOnPlayerParkedChanged().Insert(OnParkedChanged);
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnPlayerNameUpdated().Insert(OnPlayerNameUpdated);
			mgr.GetOnPlayerConnectionChanged().Insert(OnPlayerConnectionChanged);
			mgr.GetOnPlayerRemoved().Insert(OnPlayerRemoved);
			mgr.GetOnPlayerAssigned().Insert(OnSlotChanged);
			mgr.GetOnPlayerUnassigned().Insert(OnSlotChanged);
			mgr.GetOnSlotRegistered().Insert(OnSlotRegistered);
		}

		SCR_VoNComponent.LL_GetOnTalkingChanged().Insert(OnTalkingChanged);

		QueueRebuild();
	}

	override void HandlerDeattached(Widget w)
	{
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
		{
			vonMgr.GetOnPlayerChannelChanged().Remove(OnChannelsChanged);
			vonMgr.GetOnPlayerParkedChanged().Remove(OnParkedChanged);
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			mgr.GetOnPlayerNameUpdated().Remove(OnPlayerNameUpdated);
			mgr.GetOnPlayerConnectionChanged().Remove(OnPlayerConnectionChanged);
			mgr.GetOnPlayerRemoved().Remove(OnPlayerRemoved);
			mgr.GetOnPlayerAssigned().Remove(OnSlotChanged);
			mgr.GetOnPlayerUnassigned().Remove(OnSlotChanged);
			mgr.GetOnSlotRegistered().Remove(OnSlotRegistered);
		}

		SCR_VoNComponent.LL_GetOnTalkingChanged().Remove(OnTalkingChanged);

		super.HandlerDeattached(w);
	}

	protected void OnChannelsChanged(int playerId, string channelKey)
	{
		QueueRebuild();
	}

	protected void OnParkedChanged(int playerId, bool parked)
	{
		QueueRebuild();
	}

	protected void OnPlayerNameUpdated(int playerId, string name)
	{
		QueueRebuild();
	}

	protected void OnPlayerConnectionChanged(int playerId, bool connected)
	{
		QueueRebuild();
	}

	protected void OnPlayerRemoved(int playerId)
	{
		QueueRebuild();
	}

	protected void OnSlotChanged(int playerId, int slotRplId)
	{
		QueueRebuild();
	}

	protected void OnSlotRegistered(LL_SlotData slot)
	{
		QueueRebuild();
	}

	protected void OnTalkingChanged(int playerId, bool talking)
	{
		LL_VoiceChannelPlayerRow row;
		if (m_mPlayerRows.Find(playerId, row))
			row.SetTalking(talking);
	}

	// Channel assignments arrive in bursts on state changes.
	protected void QueueRebuild()
	{
		if (m_bRebuildQueued)
			return;

		m_bRebuildQueued = true;
		GetGame().GetCallqueue().CallLater(Rebuild, 0, false);
	}

	protected void Rebuild()
	{
		m_bRebuildQueued = false;

		if (!m_wRoomsList)
			return;

		Widget child = m_wRoomsList.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
		m_mPlayerRows.Clear();

		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!vonMgr || !mgr)
			return;

		int localId = GetLocalPlayerId();
		string localChannel = vonMgr.GetPlayerChannel(localId);
		LL_SlotData mySlot = mgr.FindSlotByPlayerId(localId);

		Color neutral = Color.FromSRGBA(125, 125, 125, 255);

		// Briefing drops Global/Deafen/Faction/Public. Gated on mySlot: a slot-less player
		// has no squad to focus on and keeps the general set.
		if (IsBriefingContext() && mySlot)
		{
			string factionKey = mySlot.m_sFactionKey;
			Color factionColor = FactionColor(factionKey);

			string cKey = LL_VoNChannelsManager.GetCommandChannelKey(factionKey);
			CreateRoom(cKey, "#LL-VoN_HQ", factionColor,
				localChannel == cKey, vonMgr.GetPlayersInChannel(cKey));

			// Leaders coordinate across squads; members see only their own.
			bool leaderSeesAll = mgr.IsPlayerGroupLeader(localId);
			AddSquadRooms(mgr, vonMgr, factionKey, factionColor, localChannel,
				!leaderSeesAll, mySlot.m_iGroupId);
			return;
		}

		CreateRoom(LL_VoNChannelsManager.GetDeafenChannelKey(localId), "#LL-VoN_Deafen",
			neutral,
			LL_VoNChannelsManager.IsDeafenChannelKey(localChannel),
			vonMgr.GetDeafenedPlayers());

		string globalKey = LL_VoNChannelsManager.GetLobbyChannelKey();
		CreateRoom(globalKey, "#LL-VoN_Global",
			neutral,
			localChannel == globalKey,
			vonMgr.GetPlayersInChannel(globalKey));

		// A dead player is out of the fight; Global and Public stay for cross-team chat.
		if (mySlot && !IsSpectatorContext())
		{
			string factionKey = mySlot.m_sFactionKey;
			Color factionColor = FactionColor(factionKey);

			string fKey = LL_VoNChannelsManager.GetFactionChannelKey(factionKey);
			CreateRoom(fKey, FactionDisplayName(factionKey), factionColor,
				localChannel == fKey, vonMgr.GetPlayersInChannel(fKey));

			string cKey = LL_VoNChannelsManager.GetCommandChannelKey(factionKey);
			CreateRoom(cKey, "#LL-VoN_HQ", factionColor,
				localChannel == cKey, vonMgr.GetPlayersInChannel(cKey));

			AddSquadRooms(mgr, vonMgr, factionKey, factionColor, localChannel, false, 0);
		}

		// The own Public channel is always listed; another player's only while occupied.
		// Membership is a pure local read of the replicated channel map.
		array<int> playerIds = {};
		mgr.GetKnownPlayerIds(playerIds);
		foreach (int playerId : playerIds)
		{
			string pKey = LL_VoNChannelsManager.GetPublicChannelKey(playerId);
			array<int> members = vonMgr.GetPlayersInChannel(pKey);

			if (playerId != localId && members.IsEmpty())
				continue;

			CreateRoom(pKey, WidgetManager.Translate("#LL-VoN_PublicChannel", PlayerDisplayName(playerId)),
				neutral,
				localChannel == pKey, members);
		}
	}

	// Squad rooms in slot-registration order; restrictToOwnGroup limits the list to
	// ownGroupId.
	protected void AddSquadRooms(LL_LobbyManager mgr, LL_VoNChannelsManager vonMgr, string factionKey, Color factionColor, string localChannel, bool restrictToOwnGroup, int ownGroupId)
	{
		ref array<int> seenGroups = {};
		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.m_sFactionKey != factionKey || seenGroups.Contains(slot.m_iGroupId))
				continue;

			if (restrictToOwnGroup && slot.m_iGroupId != ownGroupId)
				continue;

			seenGroups.Insert(slot.m_iGroupId);

			string gKey = LL_VoNChannelsManager.GetGroupChannelKey(slot.m_iGroupId, factionKey);
			string gName = slot.m_sGroupName;
			if (gName == "")
				gName = WidgetManager.Translate("#LL-VoN_SquadN", slot.m_iGroupId.ToString());

			CreateRoom(gKey, gName, factionColor,
				localChannel == gKey, vonMgr.GetPlayersInChannel(gKey));
		}
	}

	// Covers every spectator entry.
	protected bool IsSpectatorContext()
	{
		LL_SpectatorManager spectatorMgr = LL_SpectatorManager.GetInstance();
		return spectatorMgr && spectatorMgr.IsSpectating();
	}

	// A dead player keeps the spectator set even if the briefing view is reached.
	protected bool IsBriefingContext()
	{
		if (IsSpectatorContext())
			return false;

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		return gameMode && gameMode.GetLobbyState() == SCR_EGameModeState.BRIEFING;
	}

	protected void CreateRoom(string channelKey, string displayName, Color headerColor, bool joined, array<int> members)
	{
		Widget roomRoot = GetGame().GetWorkspace().CreateWidgets(ROOM_LAYOUT, m_wRoomsList);
		if (!roomRoot)
			return;

		LL_VoiceChannelRoom room = LL_VoiceChannelRoom.Cast(roomRoot.FindHandler(LL_VoiceChannelRoom));
		if (!room)
			return;

		room.InitRoom(this, channelKey, displayName, headerColor, joined);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		int localId = GetLocalPlayerId();

		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();

		foreach (int playerId : members)
		{
			// A parked player talks through real radios; his kept assignment is a
			// reservation for when he dies, not presence.
			if (vonMgr && vonMgr.IsPlayerParked(playerId))
				continue;

			string name = PlayerDisplayName(playerId);
			if (name == "")
				continue;

			Color memberColor = Color.FromSRGBA(120, 120, 120, 255);
			string groupName = "";
			bool isLeader = false;
			string iconPath = "";
			string iconName = "";

			if (mgr)
			{
				LL_SlotData slot = mgr.FindSlotByPlayerId(playerId);
				if (slot)
				{
					memberColor = FactionColor(slot.m_sFactionKey);
					groupName = slot.m_sGroupName;
					isLeader = mgr.IsPlayerGroupLeader(playerId);
					iconPath = slot.m_sIconPath;
					iconName = slot.m_sIconName;
				}
			}

			LL_VoiceChannelPlayerRow row = room.AddPlayer(playerId, name, memberColor, groupName, isLeader, playerId == localId, iconPath, iconName);
			if (row)
				m_mPlayerRows.Set(playerId, row);
		}
	}

	void RequestJoin(string channelKey)
	{
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		int localId = GetLocalPlayerId();

		// Same validation the server runs.
		if (!vonMgr || !vonMgr.CanPlayerJoinChannel(localId, channelKey))
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (lobbyPlayer)
			lobbyPlayer.AskJoinVoNChannel(channelKey);
	}

	protected int GetLocalPlayerId()
	{
		// Menus auto-open before the engine assigns the id on dedicated clients; ids start at 1.
		if (m_iLocalPlayerId <= 0)
		{
			PlayerController pc = GetGame().GetPlayerController();
			if (pc)
				m_iLocalPlayerId = pc.GetPlayerId();
		}

		return m_iLocalPlayerId;
	}

	protected Color FactionColor(string factionKey)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (factionManager)
		{
			Faction faction = factionManager.GetFactionByKey(factionKey);
			if (faction)
				return faction.GetFactionColor();
		}

		return Color.FromSRGBA(120, 120, 120, 255);
	}

	protected string FactionDisplayName(string factionKey)
	{
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
		if (faction)
			return faction.GetFactionName();
		return factionKey;
	}

	protected string PlayerDisplayName(int playerId)
	{
		string name = "";
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			name = mgr.GetPlayerName(playerId);

		if (name == "")
		{
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm)
				name = pm.GetPlayerName(playerId);
		}

		return name;
	}
}

class LL_VoiceChannelRoom : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName PLAYER_ROW_LAYOUT = "{7D84B5C90A2C6396}UI/Spectator/VoiceChannelPlayer.layout";

	protected LL_VoiceChannelList m_List;
	protected string m_sChannelKey;

	protected TextWidget m_wRoomName;
	protected ImageWidget m_wRoomHeaderBackground;
	protected Widget m_wRoomJoinedBorder;
	protected VerticalLayoutWidget m_wPlayersLayout;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoomName = TextWidget.Cast(w.FindAnyWidget("RoomName"));
		m_wRoomHeaderBackground = ImageWidget.Cast(w.FindAnyWidget("RoomHeaderBackground"));
		m_wRoomJoinedBorder = w.FindAnyWidget("RoomJoinedBorder");
		m_wPlayersLayout = VerticalLayoutWidget.Cast(w.FindAnyWidget("PlayersVerticalLayout"));

		// A ButtonWidget sizes to its content and ignores stretch alignment inside vertical
		// lists; these rows fill panels of three widths.
		Widget header = w.FindAnyWidget("RoomHeader");
		if (header)
		{
			LL_VoiceChannelHeaderButton headerHandler = LL_VoiceChannelHeaderButton.Cast(header.FindHandler(LL_VoiceChannelHeaderButton));
			if (headerHandler)
				headerHandler.SetRoom(this);
		}
	}

	void OnHeaderClicked()
	{
		if (m_List)
			m_List.RequestJoin(m_sChannelKey);
	}

	void InitRoom(LL_VoiceChannelList list, string channelKey, string displayName, Color headerColor, bool joined)
	{
		m_List = list;
		m_sChannelKey = channelKey;

		if (m_wRoomName)
			m_wRoomName.SetText(displayName);

		if (m_wRoomHeaderBackground)
			m_wRoomHeaderBackground.SetColor(headerColor);

		if (m_wRoomJoinedBorder)
			m_wRoomJoinedBorder.SetVisible(joined);
	}

	LL_VoiceChannelPlayerRow AddPlayer(int playerId, string name, Color factionColor, string groupName, bool isLeader, bool isSelf, string iconPath, string iconName)
	{
		if (!m_wPlayersLayout)
			return null;

		Widget rowRoot = GetGame().GetWorkspace().CreateWidgets(PLAYER_ROW_LAYOUT, m_wPlayersLayout);
		if (!rowRoot)
			return null;

		LL_VoiceChannelPlayerRow row = LL_VoiceChannelPlayerRow.Cast(rowRoot.FindHandler(LL_VoiceChannelPlayerRow));
		if (row)
			row.InitRow(playerId, name, factionColor, groupName, isLeader, isSelf, iconPath, iconName);

		return row;
	}
}

// Channel header click and hover, on a plain frame; see LL_VoiceChannelRoom.
class LL_VoiceChannelHeaderButton : SCR_ScriptedWidgetComponent
{
	protected LL_VoiceChannelRoom m_Room;
	protected Widget m_wHoverHighlight;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		// The header background carries the room colour; hover must not overwrite it.
		m_wHoverHighlight = w.FindAnyWidget("RoomHoverHighlight");
	}

	void SetRoom(LL_VoiceChannelRoom room)
	{
		m_Room = room;
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_wHoverHighlight)
			m_wHoverHighlight.SetVisible(true);
		return false;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_wHoverHighlight)
			m_wHoverHighlight.SetVisible(false);
		return false;
	}

	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;

		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
		if (m_Room)
			m_Room.OnHeaderClicked();

		return true;
	}
}

class LL_VoiceChannelPlayerRow : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName ICONS_IMAGESET = "{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset";

	protected bool m_bTalking;

	protected TextWidget m_wPlayerName;
	protected TextWidget m_wPlayerGroupName;
	protected ImageWidget m_wPlayerFactionColor;
	protected ImageWidget m_wLeaderIcon;
	protected ImageWidget m_wUnitIcon;
	protected Widget m_wImageCurrent;
	protected ImageWidget m_wVoiceIcon;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wPlayerName = TextWidget.Cast(w.FindAnyWidget("PlayerName"));
		m_wPlayerGroupName = TextWidget.Cast(w.FindAnyWidget("PlayerGroupName"));
		m_wPlayerFactionColor = ImageWidget.Cast(w.FindAnyWidget("PlayerFactionColor"));
		m_wLeaderIcon = ImageWidget.Cast(w.FindAnyWidget("LeaderIcon"));
		m_wUnitIcon = ImageWidget.Cast(w.FindAnyWidget("UnitIcon"));
		m_wImageCurrent = w.FindAnyWidget("ImageCurrent");
		m_wVoiceIcon = ImageWidget.Cast(w.FindAnyWidget("VoiceIcon"));
	}

	void InitRow(int playerId, string name, Color factionColor, string groupName, bool isLeader, bool isSelf, string iconPath, string iconName)
	{
		if (m_wPlayerName)
		{
			// The layout widget is a RichTextWidget, so the accent-coloured tag renders.
			m_wPlayerName.SetText(LL_LobbyManager.FormatPlayerNameRich(name));

			if (SCR_Global.IsAdmin(playerId))
				m_wPlayerName.SetColor(Color.FromInt(0xfff2a34b));
			else
				m_wPlayerName.SetColor(Color.White);
		}

		if (m_wPlayerGroupName)
			m_wPlayerGroupName.SetText(groupName);

		if (m_wPlayerFactionColor)
			m_wPlayerFactionColor.SetColor(factionColor);

		if (m_wLeaderIcon)
			m_wLeaderIcon.SetVisible(isLeader);

		if (m_wImageCurrent)
			m_wImageCurrent.SetVisible(isSelf);

		// Resolved server-side at slot registration; hidden for slot-less players.
		if (m_wUnitIcon)
		{
			if (iconPath != "")
			{
				if (iconName != "")
					m_wUnitIcon.LoadImageFromSet(0, iconPath, iconName);
				else
					m_wUnitIcon.LoadImageTexture(0, iconPath);

				m_wUnitIcon.SetVisible(true);
			}
			else
			{
				m_wUnitIcon.SetVisible(false);
			}
		}

		m_bTalking = SCR_VoNComponent.LL_IsTalking(playerId);
		UpdateVoiceIcon();
	}

	void SetTalking(bool talking)
	{
		m_bTalking = talking;
		UpdateVoiceIcon();
	}

	// Status only: orange while transmitting, white otherwise.
	protected void UpdateVoiceIcon()
	{
		if (!m_wVoiceIcon)
			return;

		m_wVoiceIcon.LoadImageFromSet(0, ICONS_IMAGESET, "VON_directspeech");

		if (m_bTalking)
			m_wVoiceIcon.SetColor(Color.FromSRGBA(226, 168, 80, 255));
		else
			m_wVoiceIcon.SetColor(Color.White);
	}
}