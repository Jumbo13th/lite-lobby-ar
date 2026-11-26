// Voice rooms for players without a living character. Each connected player gets a
// replicated VoN proxy entity with a radio (LL_VoNProxyComponent); rooms are string-keyed
// channels and the server owns the assignments. Proxies are placed on every machine
// because they replicate everywhere.
//
// Isolation is distance: menu talk is direct speech through the Game Master pipeline,
// which ignores frequencies and encryption keys, so each proxy stands at its room's
// position (GetRoomPosition), rooms a 200 m grid cell apart, 10 km above the playable space.
//
// Channel keys: "Lobby", "Faction_{key}", "Group_{groupId}_{key}", "Command_{key}",
// "Public_{playerId}", "Deafen_{playerId}".
//
// Parking: a player controlling a living character speaks through real radios, so their
// proxy stands in a cell of its own. The flag is computed on the server and replicated,
// never derived from local entity state: a death moves the channel in the same server
// frame as the kill, so the channel RPC races the corpse's replicated state.

class LL_VoNChannelsManagerClass : SCR_BaseGameModeComponentClass
{
}

class LL_VoNChannelsManager : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Per-player VoN proxy prefab (RplComponent streaming disabled + LL_VoNProxyComponent + SCR_VoNComponent + BaseRadioComponent)", "et")]
	protected ResourceName m_sVoNProxyPrefab;

	// Server-side; clients resolve proxies through LL_VoNProxyComponent's registry.
	protected ref map<int, IEntity> m_mProxies_S = new map<int, IEntity>();

	protected static LL_VoNChannelsManager s_Instance;

	static LL_VoNChannelsManager GetInstance()
	{
		return s_Instance;
	}

	protected ref map<int, string> m_mPlayerChannels = new map<int, string>();

	protected ref array<string> m_aChannels = {};

	// Server-computed and replicated; a missing entry means menu speaker.
	protected ref map<int, bool> m_mPlayerParked = new map<int, bool>();

	// Server-only; resolved one frame later, see QueueSlotChannelUpdate_S.
	protected ref array<int> m_aPendingSlotMoves_S = {};

	// Applied one frame later on every machine, see PlaceProxy.
	protected ref array<int> m_aPendingPlacements = {};

	protected ref ScriptInvoker m_OnPlayerChannelChanged = new ScriptInvoker(); // (int playerId, string channelKey)
	protected ref ScriptInvoker m_OnPlayerParkedChanged = new ScriptInvoker(); // (int playerId, bool parked)

	ScriptInvoker GetOnPlayerChannelChanged()
	{
		return m_OnPlayerChannelChanged;
	}

	ScriptInvoker GetOnPlayerParkedChanged()
	{
		return m_OnPlayerParkedChanged;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;

		// Sibling game-mode components have no guaranteed init order.
		GetGame().GetCallqueue().CallLater(SubscribeLobbyEvents, 0, false);

		Print("[LL_VoN] VoNChannelsManager initialized", LogLevel.NORMAL);
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);
		if (s_Instance == this)
			s_Instance = null;
	}

	protected void SubscribeLobbyEvents()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.GetOnPlayerAssigned().Insert(OnPlayerSlotAssigned);
		mgr.GetOnPlayerUnassigned().Insert(OnPlayerSlotUnassigned);
	}

	protected void OnPlayerSlotAssigned(int playerId, int slotRplId)
	{
		if (!Replication.IsServer())
			return;

		QueueSlotChannelUpdate_S(playerId);
	}

	protected void OnPlayerSlotUnassigned(int playerId, int slotRplId)
	{
		if (!Replication.IsServer())
			return;

		QueueSlotChannelUpdate_S(playerId);
	}

	// Changing slot is an unassign + assign pair in one call stack; resolving the final
	// state one frame later sends one channel update instead of two.
	protected void QueueSlotChannelUpdate_S(int playerId)
	{
		if (m_aPendingSlotMoves_S.Contains(playerId))
			return;

		m_aPendingSlotMoves_S.Insert(playerId);

		if (m_aPendingSlotMoves_S.Count() == 1)
			GetGame().GetCallqueue().CallLater(ResolveSlotChannels_S, 0, false);
	}

	protected void ResolveSlotChannels_S()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();

		foreach (int playerId : m_aPendingSlotMoves_S)
		{
			if (!m_mPlayerChannels.Contains(playerId))
				continue;

			LL_SlotData slot;
			if (mgr)
				slot = mgr.FindSlotByPlayerId(playerId);

			if (slot)
				SetPlayerChannel_S(playerId, GetGroupChannelKey(slot.m_iGroupId, slot.m_sFactionKey));
			else
				SetPlayerChannel_S(playerId, GetLobbyChannelKey());

			// Mid-game slot changes possess in the same call stack; speaker-ness may have flipped.
			UpdateParked_S(playerId);
		}

		m_aPendingSlotMoves_S.Clear();
	}

	static string GetLobbyChannelKey()
	{
		return "Lobby";
	}

	static string GetFactionChannelKey(string factionKey)
	{
		return "Faction_" + factionKey;
	}

	static string GetGroupChannelKey(int groupId, string factionKey)
	{
		return string.Format("Group_%1_%2", groupId, factionKey);
	}

	static string GetCommandChannelKey(string factionKey)
	{
		return "Command_" + factionKey;
	}

	static string GetPublicChannelKey(int playerId)
	{
		return string.Format("Public_%1", playerId);
	}

	static string GetDeafenChannelKey(int playerId)
	{
		return string.Format("Deafen_%1", playerId);
	}

	static bool IsDeafenChannelKey(string channelKey)
	{
		return channelKey.StartsWith("Deafen_");
	}

	static bool IsPublicChannelKey(string channelKey)
	{
		return channelKey.StartsWith("Public_");
	}

	// Player id from a "Public_{id}" / "Deafen_{id}" key, -1 otherwise.
	static int GetChannelKeyPlayerId(string channelKey)
	{
		int sep = channelKey.IndexOf("_");
		if (sep < 0)
			return -1;

		string prefix = channelKey.Substring(0, sep);
		if (prefix != "Public" && prefix != "Deafen")
			return -1;

		return channelKey.Substring(sep + 1, channelKey.Length() - sep - 1).ToInt();
	}

	// Faction key from "Faction_{key}" / "Command_{key}", empty otherwise.
	static string GetChannelKeyFactionKey(string channelKey)
	{
		if (channelKey.StartsWith("Faction_"))
			return channelKey.Substring(8, channelKey.Length() - 8);

		if (channelKey.StartsWith("Command_"))
			return channelKey.Substring(8, channelKey.Length() - 8);

		return "";
	}

	// Parse "Group_{groupId}_{factionKey}". Returns false for other keys.
	static bool ParseGroupChannelKey(string channelKey, out int groupId, out string factionKey)
	{
		if (!channelKey.StartsWith("Group_"))
			return false;

		string rest = channelKey.Substring(6, channelKey.Length() - 6);
		int sep = rest.IndexOf("_");
		if (sep <= 0)
			return false;

		groupId = rest.Substring(0, sep).ToInt();
		factionKey = rest.Substring(sep + 1, rest.Length() - sep - 1);
		return true;
	}

	void SetPlayerChannel_S(int playerId, string channelKey)
	{
		if (!Replication.IsServer())
			return;

		string currentChannel;
		if (m_mPlayerChannels.Find(playerId, currentChannel))
		{
			if (currentChannel == channelKey)
				return;
		}

		Print(string.Format("[LL_VoN] Player %1 → channel '%2'", playerId, channelKey), LogLevel.NORMAL);

		InitChannelIfNeeded(channelKey);

		ApplySetPlayerChannel(playerId, channelKey);

		Rpc(RpcDo_SetPlayerChannel, playerId, channelKey);
	}

	void JoinChannel_S(int playerId, string channelKey)
	{
		if (!Replication.IsServer())
			return;

		if (!CanPlayerJoinChannel(playerId, channelKey))
		{
			Print(string.Format("[LL_VoN] Join denied: player=%1 channel='%2'", playerId, channelKey), LogLevel.WARNING);
			return;
		}

		SetPlayerChannel_S(playerId, channelKey);
	}

	// Replicated state only, so clients can grey out rooms with the rule the server enforces.
	bool CanPlayerJoinChannel(int playerId, string channelKey)
	{
		if (channelKey == "")
			return false;

		if (channelKey == GetLobbyChannelKey())
			return true;

		if (IsDeafenChannelKey(channelKey))
			return GetChannelKeyPlayerId(channelKey) == playerId;

		if (IsPublicChannelKey(channelKey))
			return GetChannelKeyPlayerId(channelKey) >= 0;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return false;

		LL_SlotData slot = mgr.FindSlotByPlayerId(playerId);

		string factionKey = GetChannelKeyFactionKey(channelKey);
		if (factionKey != "")
		{
			if (!slot || slot.m_sFactionKey != factionKey)
				return false;

			// Any faction member may join HQ: HQ staff are not necessarily engine group leaders.
			return true;
		}

		int groupId;
		string groupFaction;
		if (ParseGroupChannelKey(channelKey, groupId, groupFaction))
		{
			// Cross-squad visits inside the own faction are allowed.
			return slot && slot.m_sFactionKey == groupFaction;
		}

		return false;
	}

	void AssignChannelsForState_S(SCR_EGameModeState state)
	{
		if (!Replication.IsServer())
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		Print(string.Format("[LL_VoN] Assigning channels for state: %1",
			typename.EnumToString(SCR_EGameModeState, state)), LogLevel.NORMAL);

		// Slotted players go to their squad channel; manual joins are overridden on a stage change.
		array<ref LL_SlotData> slots = mgr.GetSlots();
		foreach (LL_SlotData slot : slots)
		{
			// A placeholder holds a resumed slot for an absent player and has no voice.
			if (slot.m_iPlayerId < 0 || LL_LobbyManager.IsPlaceholderId(slot.m_iPlayerId))
				continue;

			SetPlayerChannel_S(slot.m_iPlayerId, GetGroupChannelKey(slot.m_iGroupId, slot.m_sFactionKey));
		}
	}

	//! For a possession that happens after the assignment's own next-frame resolution.
	void RefreshParking_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		UpdateParked_S(playerId);
	}

	// Spectators sit in the global channel.
	void SetPlayerSpectator_S(int playerId)
	{
		SetPlayerChannel_S(playerId, GetLobbyChannelKey());

		// A spectator is a menu speaker by definition; stated rather than derived.
		SetPlayerParked_S(playerId, false);
	}

	// After mass possession at GAME entry.
	void UpdateAllParked_S()
	{
		if (!Replication.IsServer())
			return;

		for (int i = 0; i < m_mPlayerChannels.Count(); i++)
		{
			UpdateParked_S(m_mPlayerChannels.GetKey(i));
		}
	}

	// Server-only: entity state is authoritative and synchronous with possession and kills.
	protected void UpdateParked_S(int playerId)
	{
		SetPlayerParked_S(playerId, !SCR_VoNComponent.LL_IsMenuSpeaker(playerId));
	}

	void SetPlayerParked_S(int playerId, bool parked)
	{
		if (!Replication.IsServer())
			return;

		if (IsPlayerParked(playerId) == parked)
			return;

		Print(string.Format("[LL_VoN] Player %1 radio parked=%2", playerId, parked), LogLevel.NORMAL);

		ApplySetPlayerParked(playerId, parked);
		Rpc(RpcDo_SetPlayerParked, playerId, parked);
	}

	// On every machine: clients mirror m_mPlayerChannels, so a server-only removal leaves
	// a ghost member in the room list.
	void RemovePlayerChannel_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (!m_mPlayerChannels.Contains(playerId) && !m_mPlayerParked.Contains(playerId))
			return;

		ApplyRemovePlayerChannel(playerId);
		Rpc(RpcDo_RemovePlayerChannel, playerId);
	}

	// A Public room is keyed by its owner; once the owner leaves nobody could leave it.
	protected void MigratePublicRoomToGlobal_S(int ownerId)
	{
		if (!Replication.IsServer())
			return;

		array<int> occupants = GetPlayersInChannel(GetPublicChannelKey(ownerId));
		foreach (int occupantId : occupants)
		{
			if (occupantId == ownerId)
				continue;

			SetPlayerChannel_S(occupantId, GetLobbyChannelKey());
		}
	}

	protected void SpawnProxy_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (m_mProxies_S.Contains(playerId))
			return;

		if (m_sVoNProxyPrefab == "")
		{
			Print("[LL_VoN] No VoN proxy prefab configured on LL_VoNChannelsManager — menu voice disabled", LogLevel.WARNING);
			return;
		}

		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!pc)
		{
			// Controller can lag behind the connect callback — try again.
			GetGame().GetCallqueue().CallLater(SpawnProxy_S, 500, false, playerId);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = GetRoomPosition(playerId);

		IEntity proxy = GetGame().SpawnEntityPrefab(Resource.Load(m_sVoNProxyPrefab), GetGame().GetWorld(), params);
		if (!proxy)
		{
			Print("[LL_VoN] Failed to spawn VoN proxy prefab", LogLevel.ERROR);
			return;
		}

		// Since 1.8 the Game Master voice pipeline only discovers endpoints whose radio
		// carries an EditorFactionTransceiver; with a plain one transmit looks healthy and
		// nobody receives.
		BaseRadioComponent proxyRadio = BaseRadioComponent.Cast(proxy.FindComponent(BaseRadioComponent));
		if (!proxyRadio || proxyRadio.TransceiversCount() < 1
			|| !EditorFactionTransceiver.Cast(proxyRadio.GetTransceiver(0)))
			Print("[LL_VoN] VoN proxy prefab's radio has no EditorFactionTransceiver — menu voice will be SILENT for everyone (transmit works, delivery never happens). Fix the transceiver class on the proxy prefab.", LogLevel.ERROR);

		// The owning client must drive capture on this entity's VoN component.
		RplIdentity playerRplID = pc.GetRplIdentity();
		if (playerRplID != RplIdentity.Local())
		{
			RplComponent rpl = RplComponent.Cast(proxy.FindComponent(RplComponent));
			if (rpl)
				rpl.Give(playerRplID);
		}

		LL_VoNProxyComponent proxyComp = LL_VoNProxyComponent.Cast(proxy.FindComponent(LL_VoNProxyComponent));
		if (proxyComp)
			proxyComp.SetPlayerId_S(playerId);
		else
			Print("[LL_VoN] VoN proxy prefab is missing LL_VoNProxyComponent", LogLevel.ERROR);

		// The radio gadget does not initialise itself on non-character entities.
		SCR_RadioComponent radioGadget = SCR_RadioComponent.Cast(proxy.FindComponent(SCR_RadioComponent));
		if (radioGadget)
			radioGadget.OnPostInit(proxy);

		// The server makes the routing decision and learns about editor endpoints only
		// through the editor's replicated open state, which the proxies do not have.
		SCR_VoNComponent vonComp = SCR_VoNComponent.Cast(proxy.FindComponent(SCR_VoNComponent));
		if (vonComp)
			vonComp.ConnectEditorToVoNSystem(playerId);

		m_mProxies_S.Set(playerId, proxy);
		Print(string.Format("[LL_VoN] Proxy created for player %1 '%2' (at %3)",
			playerId, PlayerNameFor(playerId), proxy.GetOrigin().ToString()), LogLevel.NORMAL);

		PlaceProxy(playerId);
	}

	protected string PlayerNameFor(int playerId)
	{
		string name;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			name = mgr.GetPlayerName(playerId);

		if (name == "")
		{
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm)
				name = pm.GetPlayerName(playerId);
		}

		if (name == "")
			name = "<unknown>";

		return name;
	}

	protected void DeleteProxy_S(int playerId)
	{
		IEntity proxy;
		if (!m_mProxies_S.Find(playerId, proxy))
			return;

		m_mProxies_S.Remove(playerId);
		if (proxy)
		{
			SCR_VoNComponent vonComp = SCR_VoNComponent.Cast(proxy.FindComponent(SCR_VoNComponent));
			if (vonComp)
				vonComp.DisconnectEditorFromVoNSystem();

			SCR_EntityHelper.DeleteEntityAndChildren(proxy);
		}

		Print(string.Format("[LL_VoN] Proxy destroyed for player %1 '%2'",
			playerId, PlayerNameFor(playerId)), LogLevel.NORMAL);
	}

	// Editor-connected menu voice goes through the Game Master pipeline, which discovers
	// endpoints by transceiver class, ignores frequencies and keys, and gates by distance.
	// Since 1.9 it takes that distance from the entity GetActiveEditorEntity returns, so the
	// proxy stands at this position on every machine (PlaceProxyNow): same channel = in
	// speech range, other rooms a grid cell away, parked and unassigned in cells of their own.
	vector GetRoomPosition(int playerId)
	{
		// A parked device never transmits; no cluster is within earshot.
		if (IsPlayerParked(playerId))
			return RoomCell(0);

		string channelKey;
		if (!m_mPlayerChannels.Find(playerId, channelKey))
			return RoomCell(1);

		int index = m_aChannels.Find(channelKey);
		if (index < 0)
			return RoomCell(1);

		// Members spread inside the cluster; an id collision co-locates two, which is harmless.
		vector memberOffset = Vector(2 * Math.Mod(playerId, 5), 0, 2 * Math.Mod(Math.Floor(playerId / 5), 5));
		return RoomCell(index + 2) + memberOffset;
	}

	// The proxy spawn grid: 200 m is beyond direct-speech range, and proxies have always
	// lived inside this footprint without hitting the world-bounds assert.
	protected vector RoomCell(int cell)
	{
		return Vector(200 * Math.Mod(cell, 16), 10000, 200 * Math.Floor(cell / 16));
	}

	// Runs on every machine. Deferred and coalesced: one event can deliver two updates for
	// a player in one frame, and the proxy can stream in before or after its channel data.
	void PlaceProxy(int playerId)
	{
		if (m_aPendingPlacements.Contains(playerId))
			return;

		m_aPendingPlacements.Insert(playerId);

		if (m_aPendingPlacements.Count() == 1)
			GetGame().GetCallqueue().CallLater(ResolvePlacements, 0, false);
	}

	protected void ResolvePlacements()
	{
		foreach (int playerId : m_aPendingPlacements)
		{
			PlaceProxyNow(playerId);
		}

		m_aPendingPlacements.Clear();
	}

	protected void PlaceProxyNow(int playerId)
	{
		IEntity proxy = LL_VoNProxyComponent.GetProxyEntity(playerId);
		if (!proxy)
			return;

		// Every machine places its own copy from replicated state, so rooms never depend on
		// whether the engine replicates the move.
		proxy.SetOrigin(GetRoomPosition(playerId));
	}

	protected void ApplySetPlayerChannel(int playerId, string channelKey)
	{
		m_mPlayerChannels.Set(playerId, channelKey);
		PlaceProxy(playerId);
		m_OnPlayerChannelChanged.Invoke(playerId, channelKey);
	}

	protected void ApplySetPlayerParked(int playerId, bool parked)
	{
		m_mPlayerParked.Set(playerId, parked);
		PlaceProxy(playerId);

		// The flag is the server saying "you are (not) a menu speaker now"; the local device
		// re-evaluates on it because no local entity event fires on death.
		PlayerController pc = GetGame().GetPlayerController();
		if (pc && pc.GetPlayerId() == playerId)
			LL_MenuVoN.Refresh();

		m_OnPlayerParkedChanged.Invoke(playerId, parked);
	}

	// Empty key = no channel.
	protected void ApplyRemovePlayerChannel(int playerId)
	{
		m_mPlayerChannels.Remove(playerId);
		m_mPlayerParked.Remove(playerId);
		m_OnPlayerChannelChanged.Invoke(playerId, "");
	}

	protected void InitChannelIfNeeded(string channelKey)
	{
		if (m_aChannels.Contains(channelKey))
			return;

		m_aChannels.Insert(channelKey);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerChannel(int playerId, string channelKey)
	{
		InitChannelIfNeeded(channelKey);
		ApplySetPlayerChannel(playerId, channelKey);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerParked(int playerId, bool parked)
	{
		ApplySetPlayerParked(playerId, parked);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemovePlayerChannel(int playerId)
	{
		ApplyRemovePlayerChannel(playerId);
	}

	override event protected bool RplSave(ScriptBitWriter writer)
	{
		int channelCount = m_aChannels.Count();
		writer.WriteInt(channelCount);
		foreach (string ch : m_aChannels)
		{
			writer.WriteString(ch);
		}

		// The parked flag rides along: JIP clients must tune alive players' proxies to the
		// parked slot.
		int playerCount = m_mPlayerChannels.Count();
		writer.WriteInt(playerCount);
		for (int i = 0; i < playerCount; i++)
		{
			writer.WriteInt(m_mPlayerChannels.GetKey(i));
			writer.WriteString(m_mPlayerChannels.GetElement(i));
			writer.WriteBool(IsPlayerParked(m_mPlayerChannels.GetKey(i)));
		}

		Print(string.Format("[LL_VoN] RplSave: %1 channels, %2 assignments", channelCount, playerCount), LogLevel.NORMAL);
		return true;
	}

	override event protected bool RplLoad(ScriptBitReader reader)
	{
		int channelCount;
		reader.ReadInt(channelCount);
		for (int i = 0; i < channelCount; i++)
		{
			string ch;
			reader.ReadString(ch);
			m_aChannels.Insert(ch);
		}

		int playerCount;
		reader.ReadInt(playerCount);
		for (int i = 0; i < playerCount; i++)
		{
			int playerId;
			string channelKey;
			bool parked;
			reader.ReadInt(playerId);
			reader.ReadString(channelKey);
			reader.ReadBool(parked);
			m_mPlayerChannels.Set(playerId, channelKey);
			m_mPlayerParked.Set(playerId, parked);
		}

		Print(string.Format("[LL_VoN] RplLoad: %1 channels, %2 assignments", channelCount, playerCount), LogLevel.NORMAL);

		GetGame().GetCallqueue().CallLater(FireJIPEvents, 0, false);
		return true;
	}

	protected void FireJIPEvents()
	{
		for (int i = 0; i < m_mPlayerChannels.Count(); i++)
		{
			m_OnPlayerChannelChanged.Invoke(m_mPlayerChannels.GetKey(i), m_mPlayerChannels.GetElement(i));

			// Proxies streamed in before this data arrived.
			PlaceProxy(m_mPlayerChannels.GetKey(i));
		}
	}

	string GetPlayerChannel(int playerId)
	{
		string ch;
		if (m_mPlayerChannels.Find(playerId, ch))
			return ch;
		return "";
	}

	bool IsPlayerParked(int playerId)
	{
		bool parked;
		if (m_mPlayerParked.Find(playerId, parked))
			return parked;
		return false;
	}

	array<string> GetChannels()
	{
		return m_aChannels;
	}

	array<int> GetPlayersInChannel(string channelKey)
	{
		array<int> result = {};
		for (int i = 0; i < m_mPlayerChannels.Count(); i++)
		{
			if (m_mPlayerChannels.GetElement(i) == channelKey)
				result.Insert(m_mPlayerChannels.GetKey(i));
		}
		return result;
	}

	// Each deafened player sits in their own room; the UI shows one collective row.
	array<int> GetDeafenedPlayers()
	{
		array<int> result = {};
		for (int i = 0; i < m_mPlayerChannels.Count(); i++)
		{
			if (IsDeafenChannelKey(m_mPlayerChannels.GetElement(i)))
				result.Insert(m_mPlayerChannels.GetKey(i));
		}
		return result;
	}

	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		super.OnGameStateChanged(state);

		if (Replication.IsServer())
			AssignChannelsForState_S(state);
	}

	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		if (!Replication.IsServer())
			return;

		SpawnProxy_S(playerId);

		SetPlayerChannel_S(playerId, GetLobbyChannelKey());
	}

	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (!Replication.IsServer())
			return;

		DeleteProxy_S(playerId);

		MigratePublicRoomToGlobal_S(playerId);

		RemovePlayerChannel_S(playerId);

		// End-of-handler marker for the watchdog log.
		Print(string.Format("[LL_VoN] disconnect cleanup done for player %1", playerId), LogLevel.NORMAL);
	}
}