// Voice rooms for players without a living character. Each connected player gets a
// replicated VoN proxy entity with a radio (LL_VoNProxyComponent); rooms are string-keyed
// channels and the server owns the assignments. Radio state is applied on every machine
// because the proxies replicate everywhere.
//
// Isolation: room ↔ room by frequency (the engine skips encryption checks between
// editor-connected endpoints), menu ↔ gameplay by encryption key ("LLVoN#" namespace),
// proximity speech by spreading the proxies 200 m apart.
//
// Channel keys: "Lobby", "Faction_{key}", "Group_{groupId}_{key}", "Command_{key}",
// "Public_{playerId}", "Deafen_{playerId}".
//
// Parking: a player controlling a living character speaks through real radios, so their
// menu radio is parked on a per-player key. The flag is computed on the server and
// replicated, never derived from local entity state: a death moves the channel in the
// same server frame as the kill, so the channel RPC races the corpse's replicated state.

class LL_VoNChannelsManagerClass : SCR_BaseGameModeComponentClass
{
}

class LL_VoNChannelsManager : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Per-player VoN proxy prefab (RplComponent streaming disabled + LL_VoNProxyComponent + SCR_VoNComponent + BaseRadioComponent)", "et")]
	protected ResourceName m_sVoNProxyPrefab;

	// Server-side; clients resolve proxies through LL_VoNProxyComponent's registry.
	protected ref map<int, IEntity> m_mProxies_S = new map<int, IEntity>();

	// Grid slots are reused after disconnect; player ids grow forever on a running server
	// and would eventually place a proxy outside the radio bubble.
	protected ref map<int, int> m_mProxySlots_S = new map<int, int>();

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

	// Applied one frame later on every machine, see ApplyRadioKey.
	protected ref array<int> m_aPendingRadioApplies = {};

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

	// Changing slot is an unassign + assign pair in one call stack, and the transceiver
	// does not reliably apply a second SetFrequency in the same frame. Resolving the final
	// state one frame later produces at most one hop.
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
			if (slot.m_iPlayerId < 0)
				continue;

			SetPlayerChannel_S(slot.m_iPlayerId, GetGroupChannelKey(slot.m_iGroupId, slot.m_sFactionKey));
		}
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

		// A radio transmission also carries proximity speech around the sender, so proxies
		// are spread 200 m apart, beyond direct-speech range and inside the menu radio's
		// range. 10 km up keeps the net away from the playable space; y=50000 asserts.
		int slot = AllocateProxySlot_S(playerId);
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = Vector(200 * Math.Mod(slot, 16), 10000, 200 * Math.Floor(slot / 16));

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
		Print(string.Format("[LL_VoN] Proxy created for player %1 '%2' (grid slot %3, at %4)",
			playerId, PlayerNameFor(playerId), slot, proxy.GetOrigin().ToString()), LogLevel.NORMAL);

		ApplyRadioKey(playerId);
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

	// Smallest free grid slot. Linear scan — bounded by the player count.
	protected int AllocateProxySlot_S(int playerId)
	{
		int existing;
		if (m_mProxySlots_S.Find(playerId, existing))
			return existing;

		int slot = 0;
		// Bounded by the player count; the guard turns a corrupt map into a log line
		// instead of a watchdog hang.
		int guard = 0;
		while (true)
		{
			bool used = false;
			for (int i = 0; i < m_mProxySlots_S.Count(); i++)
			{
				if (m_mProxySlots_S.GetElement(i) == slot)
				{
					used = true;
					break;
				}
			}

			if (!used)
				break;

			slot++;

			if (++guard > 10000)
			{
				Print(string.Format("[LL_DIAG] AllocateProxySlot_S guard tripped at slot %1 (proxySlots=%2) — no free slot found (potential lock)", slot, m_mProxySlots_S.Count()), LogLevel.ERROR);
				break;
			}
		}

		m_mProxySlots_S.Set(playerId, slot);
		return slot;
	}

	protected void DeleteProxy_S(int playerId)
	{
		IEntity proxy;
		m_mProxySlots_S.Remove(playerId);
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

	// Since 1.8 editor-connected menu voice goes through the Game Master pipeline, which
	// discovers endpoints by transceiver class, ignores frequencies and keys, and gates by
	// distance between the positions GetEditorWorldLocation reports. Rooms are therefore
	// virtual positions computed from replicated state: same channel = in speech range,
	// other channels 1 km apart, parked and unassigned players out of range of every room.
	vector GetRoomPosition(int playerId)
	{
		// A parked device never transmits; no cluster is within earshot.
		if (IsPlayerParked(playerId))
			return Vector(-5000, 10000, -5000);

		string channelKey;
		if (!m_mPlayerChannels.Find(playerId, channelKey))
			return Vector(-6000, 10000, -6000);

		int index = m_aChannels.Find(channelKey);
		if (index < 0)
			return Vector(-6000, 10000, -6000);

		// Members spread inside the cluster; an id collision co-locates two, which is harmless.
		vector memberOffset = Vector(2 * Math.Mod(playerId, 5), 0, 2 * Math.Mod(Math.Floor(playerId / 5), 5));
		return Vector(1000 + 1000 * index, 10000, 1000) + memberOffset;
	}

	// The radio's own frequency still carries room isolation: channels take a slot from
	// their index in the replicated list (identical order everywhere). Slot 0 (= min) is
	// reserved for parked radios.
	protected int ChannelFrequencyFor(BaseTransceiver tsv, string channelKey)
	{
		int minFreq = tsv.GetMinFrequency();
		int maxFreq = tsv.GetMaxFrequency();
		int step = tsv.GetFrequencyResolution();
		if (step <= 0)
			step = 10;

		int index = m_aChannels.Find(channelKey);
		if (index < 0)
			return minFreq;

		int freq = minFreq + (index + 1) * step;
		if (freq > maxFreq)
		{
			// Wrapping would merge two rooms; widen the band on the proxy prefab instead.
			Print(string.Format("[LL_VoN] Out of frequency slots for channel '%1' (%2 channels, band %3-%4 step %5)",
				channelKey, m_aChannels.Count(), minFreq, maxFreq, step), LogLevel.WARNING);
			freq = maxFreq;
		}

		return freq;
	}

	// Runs on every machine. Deferred and coalesced: one event can deliver two updates for
	// a player in one frame and the transceiver does not reliably apply a second
	// SetFrequency in the same frame.
	void ApplyRadioKey(int playerId)
	{
		if (m_aPendingRadioApplies.Contains(playerId))
			return;

		m_aPendingRadioApplies.Insert(playerId);

		if (m_aPendingRadioApplies.Count() == 1)
			GetGame().GetCallqueue().CallLater(ResolveRadioApplies, 0, false);
	}

	protected void ResolveRadioApplies()
	{
		foreach (int playerId : m_aPendingRadioApplies)
		{
			ApplyRadioKeyNow(playerId);
		}

		m_aPendingRadioApplies.Clear();
	}

	protected void ApplyRadioKeyNow(int playerId)
	{
		IEntity proxy = LL_VoNProxyComponent.GetProxyEntity(playerId);
		if (!proxy)
			return;

		BaseRadioComponent radio = BaseRadioComponent.Cast(proxy.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
			return;

		BaseTransceiver tsv = radio.GetTransceiver(0);
		if (!tsv)
			return;

		string channelKey;
		if (!m_mPlayerChannels.Find(playerId, channelKey))
			return;

		// The "LLVoN#" namespace keeps the radio-level key from colliding with mission radios.
		// Parked is the replicated flag, never local entity state. The local editor check is
		// the one local exception: a player inside Game Master keeps receiving their proxy's
		// room, so the proxy parks while their own editor is open.
		if (IsPlayerParked(playerId) || IsLocalEditorOpenFor(playerId))
		{
			radio.SetEncryptionKey(string.Format("LLVoN#Parked_%1", playerId));
			tsv.SetFrequency(tsv.GetMinFrequency());
			return;
		}

		radio.SetEncryptionKey("LLVoN#" + channelKey);
		tsv.SetFrequency(ChannelFrequencyFor(tsv, channelKey));
	}

	// Re-evaluated on editor open/close through LL_MenuVoN.Refresh.
	protected bool IsLocalEditorOpenFor(int playerId)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc || pc.GetPlayerId() != playerId)
			return false;

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		return editorManager && editorManager.IsOpened();
	}

	protected void ApplySetPlayerChannel(int playerId, string channelKey)
	{
		m_mPlayerChannels.Set(playerId, channelKey);
		ApplyRadioKey(playerId);
		m_OnPlayerChannelChanged.Invoke(playerId, channelKey);
	}

	protected void ApplySetPlayerParked(int playerId, bool parked)
	{
		m_mPlayerParked.Set(playerId, parked);
		ApplyRadioKey(playerId);

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
			ApplyRadioKey(m_mPlayerChannels.GetKey(i));
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