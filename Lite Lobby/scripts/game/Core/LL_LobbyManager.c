// Single source of truth for lobby data: slots, vehicles, player names, ready flags and
// reconnect reservations. Lives on the game-mode entity. Collections replicate as
// RpcDo_* deltas plus RplSave/RplLoad for join-in-progress; only scalars are RplProp.

class LL_LobbyManagerClass : SCR_BaseGameModeComponentClass
{
}

class LL_LobbyManager : SCR_BaseGameModeComponent
{
	protected static LL_LobbyManager s_Instance;

	static LL_LobbyManager GetInstance()
	{
		return s_Instance;
	}

	// Kept ordered by SlotOrderedBefore; linear lookups are free at 127 slots.
	protected ref array<ref LL_SlotData> m_aSlots = {};

	protected ref array<ref LL_VehicleData> m_aVehicles = {};

	// Server-only: registration order within a squad, stamped into m_iOrderIndex.
	protected int m_iNextVehicleOrder_S = 0;

	// Players without a slot still need a name in the roster.
	protected ref map<int, string> m_mPlayerNames = new map<int, string>();

	// Replicated so a client can detect a roster that drifted from the server's (a delta
	// missed while streaming in, a rename that landed mid-stream) and pull a re-sync.
	[RplProp()]
	protected int m_iRosterCount;

	[RplProp()]
	protected int m_iRosterChecksum;

	// Roster entries the server does not re-confirm during a pull-sync are ghosts and get pruned.
	protected ref array<int> m_aRosterSyncPending = new array<int>();
	protected bool m_bRosterSyncInFlight;

	protected ref map<int, bool> m_mPlayerReady = new map<int, bool>();

	// Replicated explicitly: PlayerManager.IsPlayerConnected is not reliable at the instant
	// of the notification and is not stored, so a rebuild or a JIP client would read it stale.
	protected ref array<int> m_aDisconnectedPlayerIds = new array<int>();

	// Slot RplId → website reservation label, replaced wholesale per import.
	protected ref map<int, string> m_mWebsiteOccupants = new map<int, string>();

	// Server-only: reconnect key → reserved slot RplId.
	protected ref map<string, int> m_mDisconnectedPlayers = new map<string, int>();

	// Cleanup timers are not cancelled on reconnect; each carries the generation it was
	// scheduled under and no-ops once a newer reservation superseded it.
	protected ref map<string, int> m_mReconnectGeneration = new map<string, int>();

	// Server-only: captured at connect/audit, because the backend identity may already be
	// torn down at disconnect.
	protected ref map<int, string> m_mPlayerReconnectKeys = new map<int, string>();

	// Server-only: slot RplId → live body, for possession.
	protected ref map<int, IEntity> m_mSlotEntities = new map<int, IEntity>();

	protected ref ScriptInvoker m_OnSlotRegistered = new ScriptInvoker();		// (LL_SlotData slot)
	protected ref ScriptInvoker m_OnSlotUnregistered = new ScriptInvoker();		// (int rplId)
	protected ref ScriptInvoker m_OnSlotUpdated = new ScriptInvoker();			// (LL_SlotData slot)
	protected ref ScriptInvoker m_OnPlayerAssigned = new ScriptInvoker();		// (int playerId, int slotRplId)
	protected ref ScriptInvoker m_OnPlayerUnassigned = new ScriptInvoker();		// (int playerId, int slotRplId)
	protected ref ScriptInvoker m_OnVehicleRegistered = new ScriptInvoker();	// (LL_VehicleData vehicle)
	protected ref ScriptInvoker m_OnVehicleUnregistered = new ScriptInvoker();	// (int rplId)
	protected ref ScriptInvoker m_OnPlayerNameUpdated = new ScriptInvoker();	// (int playerId, string name)
	protected ref ScriptInvoker m_OnPlayerReadyChanged = new ScriptInvoker();	// (int playerId, bool ready)
	protected ref ScriptInvoker m_OnPlayerRemoved = new ScriptInvoker();		// (int playerId)
	protected ref ScriptInvoker m_OnPlayerConnectionChanged = new ScriptInvoker();	// (int playerId, bool connected)
	protected ref ScriptInvoker m_OnWebsiteOccupantsChanged = new ScriptInvoker();	// ()

	ScriptInvoker GetOnSlotRegistered()		{ return m_OnSlotRegistered; }
	ScriptInvoker GetOnSlotUnregistered()	{ return m_OnSlotUnregistered; }
	ScriptInvoker GetOnSlotUpdated()		{ return m_OnSlotUpdated; }
	ScriptInvoker GetOnPlayerAssigned()		{ return m_OnPlayerAssigned; }
	ScriptInvoker GetOnPlayerUnassigned()	{ return m_OnPlayerUnassigned; }
	ScriptInvoker GetOnVehicleRegistered()	{ return m_OnVehicleRegistered; }
	ScriptInvoker GetOnVehicleUnregistered(){ return m_OnVehicleUnregistered; }
	ScriptInvoker GetOnPlayerNameUpdated()	{ return m_OnPlayerNameUpdated; }
	ScriptInvoker GetOnPlayerReadyChanged()	{ return m_OnPlayerReadyChanged; }
	ScriptInvoker GetOnPlayerRemoved()		{ return m_OnPlayerRemoved; }
	ScriptInvoker GetOnPlayerConnectionChanged()	{ return m_OnPlayerConnectionChanged; }
	ScriptInvoker GetOnWebsiteOccupantsChanged()	{ return m_OnWebsiteOccupantsChanged; }

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;

		Print("[LL_Lobby] LobbyManager initialized", LogLevel.NORMAL);
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);
		if (s_Instance == this)
			s_Instance = null;
	}

	array<ref LL_SlotData> GetSlots()
	{
		return m_aSlots;
	}

	LL_SlotData FindSlotByRplId(int rplId)
	{
		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_iRplId == rplId)
				return slot;
		}
		return null;
	}

	LL_SlotData FindSlotByPlayerId(int playerId)
	{
		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_iPlayerId == playerId)
				return slot;
		}
		return null;
	}

	array<ref LL_VehicleData> GetVehicles()
	{
		return m_aVehicles;
	}

	LL_VehicleData FindVehicleByRplId(int rplId)
	{
		foreach (LL_VehicleData vehicle : m_aVehicles)
		{
			if (vehicle.m_iRplId == rplId)
				return vehicle;
		}
		return null;
	}

	array<ref LL_VehicleData> GetVehiclesForGroup(int groupId)
	{
		array<ref LL_VehicleData> result = {};
		foreach (LL_VehicleData vehicle : m_aVehicles)
		{
			if (vehicle.m_iGroupId == groupId)
				result.Insert(vehicle);
		}
		return result;
	}

	string GetPlayerName(int playerId)
	{
		string name;
		if (m_mPlayerNames.Find(playerId, name))
			return name;
		return "";
	}

	// One rule for colouring and ordering, so a name is never coloured as a unit but sorted as
	// a player, or the reverse.
	static bool HasUnitTag(string name)
	{
		return name.IndexOf("[") == 0 && name.IndexOf("]") >= 2;
	}

	// Render-time only: the stored name stays plain so non-rich renderers and name
	// comparisons never see markup.
	static string FormatPlayerNameRich(string name)
	{
		if (!HasUnitTag(name))
			return name;

		int close = name.IndexOf("]");

		// 226,167,79 = UIColors.CONTRAST_COLOR, the vanilla UI accent orange.
		return string.Format("<color rgba=\"226,167,79,255\">%1</color>%2",
			name.Substring(0, close + 1),
			name.Substring(close + 1, name.Length() - close - 1));
	}

	// Rich-text roster of one faction, built from the replicated slots on any client.
	string BuildBriefingRoster(FactionKey factionKey)
	{
		array<LL_SlotData> sorted = {};
		foreach (LL_SlotData s : m_aSlots)
		{
			if (s.m_iGroupId != -1 && s.m_sFactionKey == factionKey)
				sorted.Insert(s);
		}

		array<int> groupOrder = {};
		foreach (LL_SlotData s : sorted)
		{
			if (!groupOrder.Contains(s.m_iGroupId))
				groupOrder.Insert(s.m_iGroupId);
		}

		string COL_SQUAD = "226,167,79,255";
		string COL_SEP = "150,150,150,255";
		string COL_EMPTY = "120,120,120,255";

	// Squad links point at the recorded spawn position, and only while that intel is current.
		LL_SpawnMarkers markers = LL_SpawnMarkers.GetInstance();
		bool linksActive = markers && markers.ShouldShowNow();

		string text = "";
		foreach (int groupId : groupOrder)
		{
			string groupName = "";
			string body = "";

			foreach (LL_SlotData s : sorted)
			{
				if (s.m_iGroupId != groupId)
					continue;
				if (groupName == "")
					groupName = s.m_sGroupName;

				if (s.m_iPlayerId >= 0)
				{
					string playerName = GetPlayerName(s.m_iPlayerId);
					if (playerName == "")
						playerName = WidgetManager.Translate("#LL-Roster_PlayerFallback", s.m_iPlayerId);

					body += string.Format("  %1<color=%2> - </color>%3<br/>",
						s.m_sName, COL_SEP, playerName);
				}
				else
				{
					body += string.Format("<color=%1>  %2 - %3</color><br/>",
						COL_EMPTY, s.m_sName, WidgetManager.Translate("#LL-Roster_EmptySlot"));
				}
			}

			if (groupName == "")
				groupName = WidgetManager.Translate("#LL-Roster_SquadFallback");

			string header;
			vector spawnPos;
			if (linksActive && markers.GetSquadSpawnPos(groupName, factionKey, spawnPos))
				header = string.Format("<link=@%1,%2>%3</link>", spawnPos[0], spawnPos[2], groupName);
			else
				header = string.Format("<color=%1>%2</color>", COL_SQUAD, groupName);

			text += string.Format("%1<gap/>%2<br/>", header, body);
		}

		if (text == "")
			text = WidgetManager.Translate("#LL-Roster_NoSquads");

		return text;
	}

	bool IsPlayerReady(int playerId)
	{
		bool ready;
		if (m_mPlayerReady.Find(playerId, ready))
			return ready;
		return false;
	}

	// Replicated state, not the live engine flag.
	bool IsPlayerDisconnected(int playerId)
	{
		return m_aDisconnectedPlayerIds.Contains(playerId);
	}

	// The local PlayerManager fills in asynchronously on dedicated-server clients; reading
	// it races replication and randomly misses players.
	void GetKnownPlayerIds(notnull out array<int> playerIds)
	{
		playerIds.Clear();
		for (int i = 0; i < m_mPlayerNames.Count(); i++)
		{
			playerIds.Insert(m_mPlayerNames.GetKey(i));
		}
	}

	int GetKnownPlayerCount()
	{
		return m_mPlayerNames.Count();
	}

	// Excludes disconnected-but-reserved players. Counted from the roster, not
	// PlayerManager, for the same race as GetKnownPlayerIds.
	int GetConnectedPlayerCount()
	{
		int count = 0;
		for (int i = 0; i < m_mPlayerNames.Count(); i++)
		{
			if (!m_aDisconnectedPlayerIds.Contains(m_mPlayerNames.GetKey(i)))
				count++;
		}
		return count;
	}

	// Pull-sync path; idempotent.
	void SyncPlayerFromServer(int playerId, string name, bool ready, bool disconnected)
	{
		ApplySetPlayerName(playerId, name);
		ApplySetPlayerReady(playerId, ready);
		ApplyNotifyConnection(playerId, !disconnected);

		m_aRosterSyncPending.RemoveItem(playerId);
	}

	// Per-player deltas can be missed mid stream-in, and an additive pull could never drop
	// a ghost: snapshot the local roster when a pull starts, prune what the server did not re-send.

	int GetReplicatedRosterCount()	{ return m_iRosterCount; }
	int GetReplicatedRosterChecksum()	{ return m_iRosterChecksum; }
	bool IsRosterSyncInFlight()		{ return m_bRosterSyncInFlight; }

	int ComputeLocalRosterChecksum()
	{
		int checksum = 0;
		for (int i = 0; i < m_mPlayerNames.Count(); i++)
		{
			string name = m_mPlayerNames.GetElement(i);
			checksum += m_mPlayerNames.GetKey(i) * 131 + name.Hash();
		}
		return checksum;
	}

	void BeginRosterSync()
	{
		m_aRosterSyncPending.Clear();
		GetKnownPlayerIds(m_aRosterSyncPending);
		m_bRosterSyncInFlight = true;
	}

	void EndRosterSync()
	{
		foreach (int playerId : m_aRosterSyncPending)
			ApplyRemovePlayer(playerId);

		m_aRosterSyncPending.Clear();
		m_bRosterSyncInFlight = false;
	}

	string GetWebsiteOccupant(int slotRplId)
	{
		string label;
		if (m_mWebsiteOccupants.Find(slotRplId, label))
			return label;
		return "";
	}

	int CountWebsiteOccupants()
	{
		return m_mWebsiteOccupants.Count();
	}

	array<ref LL_SlotData> GetSlotsForFaction(string factionKey)
	{
		array<ref LL_SlotData> result = {};
		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_sFactionKey == factionKey)
				result.Insert(slot);
		}
		return result;
	}

	int CountPlayersInFaction(string factionKey)
	{
		int count = 0;
		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_sFactionKey == factionKey && slot.m_iPlayerId >= 0)
				count++;
		}
		return count;
	}

	// The leader slot is the first registered slot of a group (mission-design order).
	bool IsPlayerGroupLeader(int playerId)
	{
		LL_SlotData mySlot = FindSlotByPlayerId(playerId);
		if (!mySlot)
			return false;

		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_iGroupId != mySlot.m_iGroupId || slot.m_sFactionKey != mySlot.m_sFactionKey)
				continue;

			return slot == mySlot;
		}

		return false;
	}

	void RegisterSlot_S(LL_SlotData slot, IEntity entity = null)
	{
		if (!Replication.IsServer())
			return;

		if (FindSlotByRplId(slot.m_iRplId))
		{
			Print(string.Format("[LL_Lobby] Slot %1 already registered, skipping", slot.m_iRplId), LogLevel.WARNING);
			return;
		}

		Print(string.Format("[LL_Lobby] Registering slot: rplId=%1 name=%2 faction=%3",
			slot.m_iRplId, slot.m_sName, slot.m_sFactionKey), LogLevel.NORMAL);

		if (entity)
			m_mSlotEntities.Set(slot.m_iRplId, entity);

		ApplyRegisterSlot(slot);

		// Primitives rather than LL_SlotData: codec discovery for custom RPC argument types
		// is not guaranteed. m_iSortKey is the 8th argument, the engine cap.
		Rpc(RpcDo_RegisterSlot,
			slot.m_iRplId, slot.m_sName, slot.m_sFactionKey,
			slot.m_iGroupId, slot.m_sGroupName,
			slot.m_sIconPath, slot.m_sIconName, slot.m_iSortKey);

		// Separate RPC because the registration RPC is at the 8-argument cap. Same reliable
		// channel, so it always lands after the slot exists.
		if (slot.m_sPrefabName != "")
			Rpc(RpcDo_SetSlotPrefab, slot.m_iRplId, slot.m_sPrefabName);
	}

	void UnregisterSlot_S(int rplId)
	{
		if (!Replication.IsServer())
			return;

		if (!FindSlotByRplId(rplId))
			return;

		Print(string.Format("[LL_Lobby] Unregistering slot: rplId=%1", rplId), LogLevel.NORMAL);

		m_mSlotEntities.Remove(rplId);

		ApplyUnregisterSlot(rplId);
		Rpc(RpcDo_UnregisterSlot, rplId);
	}

	IEntity GetSlotEntity_S(int rplId)
	{
		IEntity entity;
		if (m_mSlotEntities.Find(rplId, entity))
			return entity;
		return null;
	}

	// On the authority (including offline Workbench play, where slots carry fallback ids
	// Replication.FindItem cannot resolve) the entity map answers; pure clients use the
	// replication lookup.
	IEntity ResolveSlotEntity(int rplId)
	{
		IEntity entity = GetSlotEntity_S(rplId);
		if (entity)
			return entity;

		RplComponent rplComp = RplComponent.Cast(Replication.FindItem(rplId));
		if (rplComp)
			return rplComp.GetEntity();

		return null;
	}

	// The single chokepoint for entering a body. Since game version 1.7.0.54 a possessed
	// loadtime body no longer streams to clients, so a slot still holding its pre-placed body
	// gets a fresh runtime body first (RespawnSlotCharacter_S) and lands back here on it.
	// A body kept alive across a disconnect is already runtime and is re-possessed directly.
	void PossessSlot_S(int playerId, int slotRplId)
	{
		if (!Replication.IsServer())
			return;

		IEntity entity = GetSlotEntity_S(slotRplId);
		if (!entity)
			return;

		LL_PlayableComponent playable = LL_PlayableComponent.Cast(
			entity.FindComponent(LL_PlayableComponent));
		if (playable && !playable.IsRuntimeSpawned())
		{
			RespawnSlotCharacter_S(slotRplId, true);
			return;
		}

		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
			return;

		playerController.SetInitialMainEntity(entity);

		// Frequency is owner-authoritative; the owning client tunes its own radio.
		int frequency = GetSquadFrequencyForSlot(slotRplId);
		if (frequency > 0)
		{
			LL_LobbyPlayerComponent comp = LL_LobbyPlayerComponent.GetByPlayerId(playerId);
			if (comp)
				comp.TuneSquadRadio_S(frequency);
		}
	}

	// Squad radio nets. Vanilla assigns them through SCR_GroupsManagerComponent, which
	// this game mode does not run.

	// Squad radio frequency band (kHz) — matches the vanilla playable-group pool.
	protected static const int LL_SQUAD_FREQ_MIN = 38000;
	protected static const int LL_SQUAD_FREQ_MAX = 54000;
	protected static const int LL_SQUAD_FREQ_STEP = 1000;

	// Per faction; the same numbers on two factions stay separate through the per-faction
	// encryption keys. Idempotent: a group that already holds a frequency keeps it.
	void AssignSquadFrequencies_S()
	{
		if (!Replication.IsServer())
			return;

		SCR_FactionManager factionMgr = SCR_FactionManager.Cast(GetGame().GetFactionManager());

		map<string, int> nextIndexByFaction = new map<string, int>();
		map<string, int> reservedByFaction = new map<string, int>();
		set<int> seenGroups = new set<int>();

		foreach (LL_SlotData slot : GetSlots())
		{
			if (slot.m_iGroupId == -1 || seenGroups.Contains(slot.m_iGroupId))
				continue;
			seenGroups.Insert(slot.m_iGroupId);

			RplComponent groupRpl = RplComponent.Cast(Replication.FindItem(slot.m_iGroupId));
			if (!groupRpl)
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(groupRpl.GetEntity());
			if (!group)
				continue;

			if (group.GetRadioFrequency() > 0)
				continue;
			// The faction's platoon/HQ net must never be handed to a squad; resolved once per faction.
			int reserved = -1;
			if (!reservedByFaction.Find(slot.m_sFactionKey, reserved))
			{
				reserved = -1;
				if (factionMgr)
				{
					SCR_Faction faction = SCR_Faction.Cast(factionMgr.GetFactionByKey(slot.m_sFactionKey));
					if (faction)
						reserved = faction.GetFactionRadioFrequency();
				}
				reservedByFaction.Set(slot.m_sFactionKey, reserved);
			}

			int index = 0;
			nextIndexByFaction.Find(slot.m_sFactionKey, index);

			int frequency = LL_SQUAD_FREQ_MIN + index * LL_SQUAD_FREQ_STEP;
			if (frequency == reserved)
			{
				index++;
				frequency = LL_SQUAD_FREQ_MIN + index * LL_SQUAD_FREQ_STEP;
			}

			if (frequency > LL_SQUAD_FREQ_MAX)
			{
				Print(string.Format("[LL_Lobby] Out of squad radio frequencies for faction '%1' (band %2-%3 step %4) — reusing max",
					slot.m_sFactionKey, LL_SQUAD_FREQ_MIN, LL_SQUAD_FREQ_MAX, LL_SQUAD_FREQ_STEP), LogLevel.WARNING);
				frequency = LL_SQUAD_FREQ_MAX;
			}

			group.SetRadioFrequency(frequency);
			nextIndexByFaction.Set(slot.m_sFactionKey, index + 1);
		}
	}

	// Lobby display name of the squad on a frequency. The in-game radio's own channel-name
	// lookup only knows groups registered with the groups manager this mode does not run.
	string GetSquadChannelName(int frequency, string factionKey)
	{
		if (frequency <= 0)
			return "";

		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_iGroupId == -1)
				continue;
			if (factionKey != "" && slot.m_sFactionKey != factionKey)
				continue;

			RplComponent groupRpl = RplComponent.Cast(Replication.FindItem(slot.m_iGroupId));
			if (!groupRpl)
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(groupRpl.GetEntity());
			if (!group || group.GetRadioFrequency() != frequency)
				continue;

			return slot.m_sGroupName;
		}

		return "";
	}

	// 0 when the slot has no group or the group has no frequency.
	int GetSquadFrequencyForSlot(int slotRplId)
	{
		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot || slot.m_iGroupId == -1)
			return 0;

		RplComponent groupRpl = RplComponent.Cast(Replication.FindItem(slot.m_iGroupId));
		if (!groupRpl)
			return 0;
		SCR_AIGroup group = SCR_AIGroup.Cast(groupRpl.GetEntity());
		if (!group)
			return 0;

		return group.GetRadioFrequency();
	}

	bool TakeSlot_S(int playerId, int slotRplId)
	{
		if (!Replication.IsServer())
			return false;

		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
		{
			Print(string.Format("[LL_Lobby] TakeSlot failed: slot %1 not found", slotRplId), LogLevel.WARNING);
			return false;
		}

		if (!slot.IsAvailable())
		{
			Print(string.Format("[LL_Lobby] TakeSlot failed: slot %1 not available (player=%2, locked=%3, destroyed=%4)",
				slotRplId, slot.m_iPlayerId, slot.m_bLocked, slot.IsDestroyed()), LogLevel.WARNING);
			return false;
		}

		LL_SlotData currentSlot = FindSlotByPlayerId(playerId);
		if (currentSlot)
			LeaveSlot_S(playerId);

		Print(string.Format("[LL_Lobby] Player %1 taking slot %2 (%3)", playerId, slotRplId, slot.m_sName), LogLevel.NORMAL);

		ApplyTakeSlot(playerId, slotRplId);
		Rpc(RpcDo_TakeSlot, playerId, slotRplId);

		// Vanilla map-marker filtering and nametags read the player-level faction, which
		// exists independently of any controlled entity.
		SetPlayerEngineFaction_S(playerId, slot.m_sFactionKey);

		// In GAME a taken slot is entered immediately; before GAME possession waits for the transition.
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && gameMode.GetLobbyState() == SCR_EGameModeState.GAME)
			PossessSlot_S(playerId, slotRplId);

		return true;
	}

	void LeaveSlot_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		LL_SlotData slot = FindSlotByPlayerId(playerId);
		if (!slot)
			return;

		int slotRplId = slot.m_iRplId;
		Print(string.Format("[LL_Lobby] Player %1 leaving slot %2", playerId, slotRplId), LogLevel.NORMAL);

		ApplyLeaveSlot(playerId, slotRplId);
		Rpc(RpcDo_LeaveSlot, playerId, slotRplId);

		SetPlayerReady_S(playerId, false);

		SetPlayerEngineFaction_S(playerId, "");
	}

	// A controller cannot be detached from its main entity in place (SetControlledEntity(null)
	// is unimplemented); deleting the body is the only detach. So a live body is swapped for
	// a fresh AI body through the fresh-spawn path, which deletes the old one. A dead slot
	// keeps its corpse and is only unassigned.
	void ReleaseSlotInGame_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		LL_SlotData slot = FindSlotByPlayerId(playerId);
		if (!slot)
			return;

		int slotRplId = slot.m_iRplId;
		bool bodyAlive = !slot.IsDestroyed();

		LeaveSlot_S(playerId);

		if (bodyAlive)
			RespawnSlotCharacter_S(slotRplId, true);

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode)
			gameMode.SendPlayerToSpectator_S(playerId);
	}

	// SCR_FactionManager.SGetPlayerFaction reads the controller's affiliation component,
	// not the controlled entity.
	protected void SetPlayerEngineFaction_S(int playerId, FactionKey factionKey)
	{
		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!pc)
			return;

		SCR_PlayerFactionAffiliationComponent factionAffiliation = SCR_PlayerFactionAffiliationComponent.Cast(
			pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!factionAffiliation)
		{
			Print("[LL_Lobby] Player controller has no SCR_PlayerFactionAffiliationComponent — faction markers will not filter", LogLevel.WARNING);
			return;
		}

		Faction faction = null;
		if (factionKey != "")
			faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);

		// SetFaction_S re-notifies the faction manager even on a no-op set.
		if (factionAffiliation.GetAffiliatedFaction() == faction)
			return;

		factionAffiliation.SetFaction_S(faction);

		// The client pruned this faction's markers on the switch; re-send them.
		if (faction)
		{
			LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.Cast(
				pc.FindComponent(LL_LobbyPlayerComponent));
			if (lobbyPlayer)
				lobbyPlayer.SyncFactionMarkers_S(faction);
		}
	}

	void SetSlotDamageState_S(int slotRplId, int damageState)
	{
		if (!Replication.IsServer())
			return;

		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
			return;

		if (slot.m_iDamageState == damageState)
			return;

		Print(string.Format("[LL_Lobby] Slot %1 damage state: %2", slotRplId, damageState), LogLevel.NORMAL);

		ApplySetDamageState(slotRplId, damageState);
		Rpc(RpcDo_SetDamageState, slotRplId, damageState);
	}

	// Spawns a fresh body for the slot at the current body's transform and drops the old one.
	// allowAlive=false is the Game Master revive (dead slots only); allowAlive=true is the
	// possession path replacing a live loadtime body. Routed through TakeSlot_S so the fresh
	// body registers as a normal slot and the assignment broadcast exits the spectator.
	void RespawnSlotCharacter_S(int slotRplId, bool allowAlive = false)
	{
		if (!Replication.IsServer())
			return;

		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
		{
			Print(string.Format("[LL_Lobby] Respawn failed: slot %1 not found", slotRplId), LogLevel.WARNING);
			return;
		}

		// The authority re-checks the dead-slot rule against a tampered request.
		if (!allowAlive && !slot.IsDestroyed())
		{
			Print(string.Format("[LL_Lobby] Respawn skipped: slot %1 is not dead", slotRplId), LogLevel.NORMAL);
			return;
		}

		int playerId = slot.m_iPlayerId;

		IEntity oldBody = GetSlotEntity_S(slotRplId);
		if (!oldBody)
		{
			Print(string.Format("[LL_Lobby] Respawn failed: slot %1 has no live entity", slotRplId), LogLevel.WARNING);
			return;
		}

		EntityPrefabData prefabData = oldBody.GetPrefabData();
		if (!prefabData)
			return;

		Resource resource = Resource.Load(prefabData.GetPrefabName());
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[LL_Lobby] Respawn failed: prefab failed to load for slot %1", slotRplId), LogLevel.WARNING);
			return;
		}

		// LL_PlayableComponent skips ungrouped bodies, so without the squad the fresh body
		// would never register as a slot.
		SCR_AIGroup group = null;
		RplComponent groupRpl = RplComponent.Cast(Replication.FindItem(slot.m_iGroupId));
		if (groupRpl)
			group = SCR_AIGroup.Cast(groupRpl.GetEntity());

		if (!group)
		{
			Print(string.Format("[LL_Lobby] Respawn failed: could not resolve squad (group %1) for slot %2", slot.m_iGroupId, slotRplId), LogLevel.WARNING);
			return;
		}

		bool wasLeader = group.GetLeaderEntity() == oldBody;

		EntitySpawnParams params = new EntitySpawnParams();
		oldBody.GetWorldTransform(params.Transform);

		IEntity newBody = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!newBody)
		{
			Print(string.Format("[LL_Lobby] Respawn failed: could not spawn body for slot %1", slotRplId), LogLevel.WARNING);
			return;
		}

		// Marked runtime now so PossessSlot_S possesses it directly.
		LL_PlayableComponent newPlayable = LL_PlayableComponent.Cast(
			newBody.FindComponent(LL_PlayableComponent));
		if (newPlayable)
		{
			newPlayable.MarkRuntimeSpawned();
			// Inherit the replaced slot's in-squad position; a new, higher RplId would drop it
			// to the bottom of the squad.
			newPlayable.SetSortOrder(slot.m_iSortKey);
		}

		// A freshly spawned character's AI agent is not wired to its control component on
		// the spawn frame.
		GetGame().GetCallqueue().Call(RespawnAssignGroup_S, newBody, group, wasLeader);

		// Registration is deferred and retried inside the playable component, so poll for it.
		GetGame().GetCallqueue().CallLater(RespawnFinish_S, 600, false, playerId, slotRplId, newBody, 0);
	}

	protected void RespawnAssignGroup_S(IEntity newBody, SCR_AIGroup group, bool makeLeader)
	{
		if (!newBody || !group)
			return;

		group.AddAIEntityToGroup(newBody);

		AIControlComponent control = AIControlComponent.Cast(newBody.FindComponent(AIControlComponent));
		if (!control)
			return;

		if (makeLeader)
		{
			AIAgent agent = control.GetControlAIAgent();
			if (agent)
				group.SetNewLeader(agent);
		}

		control.DeactivateAI();
	}

	// playerId is -1 for a bot slot.
	protected void RespawnFinish_S(int playerId, int oldSlotRplId, IEntity newBody, int attempt)
	{
		if (!newBody)
			return;

		LL_PlayableComponent newPlayable = LL_PlayableComponent.Cast(newBody.FindComponent(LL_PlayableComponent));
		int newSlotRplId = -1;
		if (newPlayable)
			newSlotRplId = newPlayable.GetRplId();

		// GetRplId is only valid once registered; FindSlotByRplId confirms the slot made the list.
		bool ready = newPlayable && FindSlotByRplId(newSlotRplId) != null;
		if (!ready)
		{
			if (attempt < 60)
			{
				GetGame().GetCallqueue().CallLater(RespawnFinish_S, 250, false, playerId, oldSlotRplId, newBody, attempt + 1);
				return;
			}

			Print(string.Format("[LL_Lobby] Respawn aborted: fresh body never registered a slot (old slot %1) — cleaning up", oldSlotRplId), LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(newBody);
			return;
		}

		// TakeSlot_S auto-leaves the dead slot, possesses the new one and broadcasts the
		// assignment the spectator screen exits on. A bot slot simply stays selectable.
		if (playerId > 0)
		{
			bool ok = TakeSlot_S(playerId, newSlotRplId);
			if (!ok)
			{
				Print(string.Format("[LL_Lobby] Respawn: hand-off failed for player %1 → slot %2 — cleaning up", playerId, newSlotRplId), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(newBody);
				return;
			}
		}

		// The corpse's OnDelete unregisters the dead slot everywhere.
		IEntity corpse = GetSlotEntity_S(oldSlotRplId);
		if (corpse)
			SCR_EntityHelper.DeleteEntityAndChildren(corpse);

		Print(string.Format("[LL_Lobby] Revived slot: dead slot %1 → fresh slot %2 (player %3, -1 = bot)", oldSlotRplId, newSlotRplId, playerId), LogLevel.NORMAL);
	}

	void SetSlotLocked_S(int slotRplId, bool locked)
	{
		if (!Replication.IsServer())
			return;

		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
			return;

		if (slot.m_bLocked == locked)
			return;

		ApplySetSlotLocked(slotRplId, locked);
		Rpc(RpcDo_SetSlotLocked, slotRplId, locked);
	}

	// One group RPC fanned out locally, not one RPC per slot.

	void SetGroupLocked_S(int groupId, bool locked)
	{
		if (!Replication.IsServer())
			return;

		ApplySetGroupLocked(groupId, locked);
		Rpc(RpcDo_SetGroupLocked, groupId, locked);
	}

	// True only when the group has slots and all of them are locked.
	bool IsGroupLocked(int groupId)
	{
		bool any = false;
		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_iGroupId != groupId)
				continue;

			any = true;
			if (!slot.m_bLocked)
				return false;
		}
		return any;
	}

	void SetPlayerName_S(int playerId, string name)
	{
		if (!Replication.IsServer())
			return;

		if (GetPlayerName(playerId) == name)
			return;

		ApplySetPlayerName(playerId, name);
		Rpc(RpcDo_SetPlayerName, playerId, name);

		UpdateRosterCount_S();
	}

	void SetPlayerReady_S(int playerId, bool ready)
	{
		if (!Replication.IsServer())
			return;

		if (IsPlayerReady(playerId) == ready)
			return;

		ApplySetPlayerReady(playerId, ready);
		Rpc(RpcDo_SetPlayerReady, playerId, ready);
	}

	void RemovePlayer_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		m_mPlayerReconnectKeys.Remove(playerId);

		ApplyRemovePlayer(playerId);
		Rpc(RpcDo_RemovePlayer, playerId);

		UpdateRosterCount_S();
	}

	// One scalar, bumped only on change.
	protected void UpdateRosterCount_S()
	{
		if (!Replication.IsServer())
			return;

		int count = m_mPlayerNames.Count();
		int checksum = ComputeLocalRosterChecksum();
		if (m_iRosterCount == count && m_iRosterChecksum == checksum)
			return;

		m_iRosterCount = count;
		m_iRosterChecksum = checksum;
		Replication.BumpMe();
	}

	void NotifyPlayerConnection_S(int playerId, bool connected)
	{
		if (!Replication.IsServer())
			return;

		ApplyNotifyConnection(playerId, connected);
		Rpc(RpcDo_NotifyPlayerConnection, playerId, connected);
	}

	void RegisterVehicle_S(LL_VehicleData vehicle)
	{
		if (!Replication.IsServer())
			return;

		if (FindVehicleByRplId(vehicle.m_iRplId))
			return;

		Print(string.Format("[LL_Lobby] Registering vehicle: rplId=%1 name=%2", vehicle.m_iRplId, vehicle.m_sName), LogLevel.NORMAL);

		vehicle.m_iOrderIndex = m_iNextVehicleOrder_S;
		m_iNextVehicleOrder_S++;

		ApplyRegisterVehicle(vehicle);
		Rpc(RpcDo_RegisterVehicle,
			vehicle.m_iRplId, vehicle.m_sName, vehicle.m_sFactionKey,
			vehicle.m_iGroupId, vehicle.m_bLocked, vehicle.m_iOrderIndex,
			vehicle.m_sIconPath, vehicle.m_sPrefabName);
	}

	void UnregisterVehicle_S(int rplId)
	{
		if (!Replication.IsServer())
			return;

		if (!FindVehicleByRplId(rplId))
			return;

		ApplyUnregisterVehicle(rplId);
		Rpc(RpcDo_UnregisterVehicle, rplId);
	}

	// Apply* mutate local state on every machine: the server calls Apply + Rpc, clients
	// call Apply from the RPC handler.

	// A slot can reach a client twice (JIP snapshot and a broadcast queued during streaming),
	// and the engine does not guarantee how those interleave, so insertion is guarded and
	// the list is re-sorted by server-assigned keys. Returns false on a duplicate.
	protected bool InsertSlotOrdered(LL_SlotData slot)
	{
		if (FindSlotByRplId(slot.m_iRplId))
			return false;

		// Full re-sort, not a single-pass insert: a single pass assumes the array is already
		// sorted, which one out-of-order arrival breaks for good.
		m_aSlots.Insert(slot);
		SortSlots();
		return true;
	}

	// Stable insertion sort.
	protected void SortSlots()
	{
		int n = m_aSlots.Count();
		for (int i = 1; i < n; i++)
		{
			// Must be a strong ref: the shift overwrites index i and a weak ref would let the
			// object be freed mid-sort.
			ref LL_SlotData cur = m_aSlots[i];
			int j = i - 1;
			while (j >= 0 && SlotOrderedBefore(cur, m_aSlots[j]))
			{
				m_aSlots[j + 1] = m_aSlots[j];
				j--;
			}
			m_aSlots[j + 1] = cur;
		}
	}

	// The entire ordering: faction, then group name (number-aware), then m_iSortKey, which
	// for loadtime bodies is the RplId (editor placement order) and is inherited on respawn.
	// Removals must use RemoveOrdered; Remove swaps with the last element.
	protected bool SlotOrderedBefore(LL_SlotData a, LL_SlotData b)
	{
		if (a.m_sFactionKey != b.m_sFactionKey)
			return CompareNatural(a.m_sFactionKey, b.m_sFactionKey) < 0;

		if (a.m_iGroupId != b.m_iGroupId)
			return CompareNatural(a.m_sGroupName, b.m_sGroupName) < 0;

		return a.m_iSortKey < b.m_iSortKey;
	}

	// Number-aware compare: digit runs compare as whole numbers.
	protected static int CompareNatural(string a, string b)
	{
		int lenA = a.Length();
		int lenB = b.Length();
		int ia = 0;
		int ib = 0;

		while (ia < lenA && ib < lenB)
		{
			int ca = a.ToAscii(ia);
			int cb = b.ToAscii(ib);

			bool digitA = ca >= 48 && ca <= 57;
			bool digitB = cb >= 48 && cb <= 57;

			if (digitA && digitB)
			{
				int numA = 0;
				while (ia < lenA)
				{
					int charA = a.ToAscii(ia);
					if (charA < 48 || charA > 57)
						break;
					numA = numA * 10 + (charA - 48);
					ia++;
				}

				int numB = 0;
				while (ib < lenB)
				{
					int charB = b.ToAscii(ib);
					if (charB < 48 || charB > 57)
						break;
					numB = numB * 10 + (charB - 48);
					ib++;
				}

				if (numA != numB)
					return numA - numB;

				continue;
			}

			if (ca != cb)
				return ca - cb;

			ia++;
			ib++;
		}

		return (lenA - ia) - (lenB - ib);
	}

	protected void ApplyRegisterSlot(LL_SlotData slot)
	{
		if (!InsertSlotOrdered(slot))
			return;

		m_OnSlotRegistered.Invoke(slot);
	}

	// Late prefab name from RegisterSlot_S's follow-up RPC.
	protected void ApplySetSlotPrefab(int rplId, string prefabName)
	{
		LL_SlotData slot = FindSlotByRplId(rplId);
		if (!slot)
			return;

		slot.m_sPrefabName = prefabName;
		m_OnSlotUpdated.Invoke(slot);
	}

	protected void ApplyUnregisterSlot(int rplId)
	{
		for (int i = m_aSlots.Count() - 1; i >= 0; i--)
		{
			if (m_aSlots[i].m_iRplId == rplId)
			{
				// array.Remove is a swap-with-last and would scramble the sorted order.
				m_aSlots.RemoveOrdered(i);
				break;
			}
		}
		m_OnSlotUnregistered.Invoke(rplId);
	}

	protected void ApplyTakeSlot(int playerId, int slotRplId)
	{
		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
			return;

		slot.m_iPlayerId = playerId;
		m_OnPlayerAssigned.Invoke(playerId, slotRplId);
		m_OnSlotUpdated.Invoke(slot);
	}

	protected void ApplyLeaveSlot(int playerId, int slotRplId)
	{
		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
			return;

		slot.m_iPlayerId = -1;
		m_OnPlayerUnassigned.Invoke(playerId, slotRplId);
		m_OnSlotUpdated.Invoke(slot);
	}

	protected void ApplySetDamageState(int slotRplId, int damageState)
	{
		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
			return;

		slot.m_iDamageState = damageState;
		m_OnSlotUpdated.Invoke(slot);
	}

	protected void ApplySetSlotLocked(int slotRplId, bool locked)
	{
		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (!slot)
			return;

		slot.m_bLocked = locked;
		m_OnSlotUpdated.Invoke(slot);
	}

	protected void ApplySetGroupLocked(int groupId, bool locked)
	{
		foreach (LL_SlotData slot : m_aSlots)
		{
			if (slot.m_iGroupId != groupId || slot.m_bLocked == locked)
				continue;

			slot.m_bLocked = locked;
			m_OnSlotUpdated.Invoke(slot);
		}
	}

	protected void ApplySetPlayerName(int playerId, string name)
	{
		m_mPlayerNames.Set(playerId, name);
		m_OnPlayerNameUpdated.Invoke(playerId, name);
	}

	protected void ApplySetPlayerReady(int playerId, bool ready)
	{
		m_mPlayerReady.Set(playerId, ready);
		m_OnPlayerReadyChanged.Invoke(playerId, ready);
	}

	protected void ApplyRemovePlayer(int playerId)
	{
		m_mPlayerNames.Remove(playerId);
		m_mPlayerReady.Remove(playerId);
		m_aDisconnectedPlayerIds.RemoveItem(playerId);
		m_OnPlayerRemoved.Invoke(playerId);
	}

	// Single choke point so the replicated set and the recolor event always agree.
	protected void ApplyNotifyConnection(int playerId, bool connected)
	{
		if (connected)
			m_aDisconnectedPlayerIds.RemoveItem(playerId);
		else if (!m_aDisconnectedPlayerIds.Contains(playerId))
			m_aDisconnectedPlayerIds.Insert(playerId);

		m_OnPlayerConnectionChanged.Invoke(playerId, connected);
	}

	protected bool InsertVehicleOrdered(LL_VehicleData vehicle)
	{
		if (FindVehicleByRplId(vehicle.m_iRplId))
			return false;

		int insertAt = m_aVehicles.Count();
		while (insertAt > 0 && m_aVehicles[insertAt - 1].m_iOrderIndex > vehicle.m_iOrderIndex)
			insertAt--;

		m_aVehicles.InsertAt(vehicle, insertAt);
		return true;
	}

	protected void ApplyRegisterVehicle(LL_VehicleData vehicle)
	{
		if (!InsertVehicleOrdered(vehicle))
			return;

		m_OnVehicleRegistered.Invoke(vehicle);
	}

	protected void ApplyUnregisterVehicle(int rplId)
	{
		for (int i = m_aVehicles.Count() - 1; i >= 0; i--)
		{
			if (m_aVehicles[i].m_iRplId == rplId)
			{
				// Remove would swap the last element in and break the order.
				m_aVehicles.RemoveOrdered(i);
				break;
			}
		}
		m_OnVehicleUnregistered.Invoke(rplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterSlot(int rplId, string name, string factionKey,
		int groupId, string groupName, string iconPath, string iconName, int sortKey)
	{
		// Constructor defaults (no player, alive, unlocked) are right for a fresh registration.
		LL_SlotData slot = new LL_SlotData();
		slot.m_iRplId = rplId;
		slot.m_sName = name;
		slot.m_sFactionKey = factionKey;
		slot.m_iGroupId = groupId;
		slot.m_sGroupName = groupName;
		slot.m_sIconPath = iconPath;
		slot.m_sIconName = iconName;
		slot.m_iSortKey = sortKey;
		ApplyRegisterSlot(slot);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetSlotPrefab(int rplId, string prefabName)
	{
		ApplySetSlotPrefab(rplId, prefabName);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UnregisterSlot(int rplId)
	{
		ApplyUnregisterSlot(rplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_TakeSlot(int playerId, int slotRplId)
	{
		ApplyTakeSlot(playerId, slotRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LeaveSlot(int playerId, int slotRplId)
	{
		ApplyLeaveSlot(playerId, slotRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetDamageState(int slotRplId, int damageState)
	{
		ApplySetDamageState(slotRplId, damageState);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetSlotLocked(int slotRplId, bool locked)
	{
		ApplySetSlotLocked(slotRplId, locked);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetGroupLocked(int groupId, bool locked)
	{
		ApplySetGroupLocked(groupId, locked);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerName(int playerId, string name)
	{
		ApplySetPlayerName(playerId, name);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerReady(int playerId, bool ready)
	{
		ApplySetPlayerReady(playerId, ready);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemovePlayer(int playerId)
	{
		ApplyRemovePlayer(playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_NotifyPlayerConnection(int playerId, bool connected)
	{
		ApplyNotifyConnection(playerId, connected);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterVehicle(int rplId, string name, string factionKey,
		int groupId, bool locked, int orderIndex, string iconPath, string prefabName)
	{
		LL_VehicleData vehicle = new LL_VehicleData();
		vehicle.m_iRplId = rplId;
		vehicle.m_sName = name;
		vehicle.m_sFactionKey = factionKey;
		vehicle.m_iGroupId = groupId;
		vehicle.m_bLocked = locked;
		vehicle.m_iOrderIndex = orderIndex;
		vehicle.m_sIconPath = iconPath;
		vehicle.m_sPrefabName = prefabName;
		ApplyRegisterVehicle(vehicle);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UnregisterVehicle(int rplId)
	{
		ApplyUnregisterVehicle(rplId);
	}

	// Transient HUD notice (/message and mission triggers). Not state: a late joiner misses
	// it. Routed through the manager because the game-mode entity has no RplComponent of
	// its own to send RPCs with. The title may be a #key; each client translates it.
	void BroadcastAdminMessage_S(string text, string title = "#LL-AdminMessage_Announcement")
	{
		if (!Replication.IsServer() || text == "")
			return;

		// Rpc never loops back to the sender; the local apply covers the listen host.
		ApplyShowAdminMessage(text, title);
		Rpc(RpcDo_ShowAdminMessage, text, title);
	}

	protected void ApplyShowAdminMessage(string text, string title)
	{
		if (!GetGame().GetWorkspace())
			return;

		LL_AdminMessageHud.ShowMessage(text, title);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ShowAdminMessage(string text, string title)
	{
		ApplyShowAdminMessage(text, title);
	}

	// Same notice narrowed to a faction and/or squads. The audience travels with the one
	// broadcast and each client gates against its own replicated slot: no per-player fan-out.
	void BroadcastTargetedMessage_S(string text, string title, FactionKey factionKey, notnull array<int> groupIds)
	{
		if (!Replication.IsServer() || text == "")
			return;

		if (factionKey == "" && groupIds.IsEmpty())
		{
			BroadcastAdminMessage_S(text, title);
			return;
		}

		ApplyShowTargetedMessage(text, title, factionKey, groupIds);
		Rpc(RpcDo_ShowTargetedMessage, text, title, factionKey, groupIds);
	}

	protected void ApplyShowTargetedMessage(string text, string title, FactionKey factionKey, array<int> groupIds)
	{
		if (!GetGame().GetWorkspace())
			return;

		// Unslotted viewers (spectating admins, Game Master) pass every filter.
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;

		LL_SlotData slot = FindSlotByPlayerId(pc.GetPlayerId());
		if (slot)
		{
			if (factionKey != "" && slot.m_sFactionKey != factionKey)
				return;
			if (groupIds && !groupIds.IsEmpty() && !groupIds.Contains(slot.m_iGroupId))
				return;
		}

		LL_AdminMessageHud.ShowMessage(text, title);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ShowTargetedMessage(string text, string title, FactionKey factionKey, array<int> groupIds)
	{
		ApplyShowTargetedMessage(text, title, factionKey, groupIds);
	}

	// The engine only translates a string that is entirely one #key, so the zone label and
	// faction name travel as parts and are translated on the receiving client. %1 = zone,
	// %2 = the new owner.
	void BroadcastZoneFlip_S(string messageKey, string zoneLabel, FactionKey owner, string title = "#LL-AdminMessage_Announcement")
	{
		if (!Replication.IsServer() || messageKey == "")
			return;

		ApplyShowZoneFlip(messageKey, zoneLabel, owner, title);
		Rpc(RpcDo_ShowZoneFlip, messageKey, zoneLabel, owner, title);
	}

	protected void ApplyShowZoneFlip(string messageKey, string zoneLabel, FactionKey owner, string title)
	{
		if (!GetGame().GetWorkspace())
			return;

		string zone = WidgetManager.Translate(zoneLabel);
		string faction = WidgetManager.Translate(LL_TriggerComponent.FactionName(owner));

		LL_AdminMessageHud.ShowMessage(WidgetManager.Translate(messageKey, zone, faction), title);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ShowZoneFlip(string messageKey, string zoneLabel, FactionKey owner, string title)
	{
		ApplyShowZoneFlip(messageKey, zoneLabel, owner, title);
	}

	// Replaced wholesale per import: Begin clears, chunks add "rplId\tlabel" lines, End fires
	// the UI event. Reliable RPCs from one sender keep their order. Begin+End = clear.

	void BeginWebsiteOccupants_S()
	{
		if (!Replication.IsServer())
			return;

		ApplyBeginWebsiteOccupants();
		Rpc(RpcDo_BeginWebsiteOccupants);
	}

	void AddWebsiteOccupantsChunk_S(string chunk)
	{
		if (!Replication.IsServer())
			return;

		ApplyWebsiteOccupantsChunk(chunk);
		Rpc(RpcDo_WebsiteOccupantsChunk, chunk);
	}

	int EndWebsiteOccupants_S()
	{
		if (!Replication.IsServer())
			return 0;

		ApplyEndWebsiteOccupants();
		Rpc(RpcDo_EndWebsiteOccupants);

		return m_mWebsiteOccupants.Count();
	}

	protected void ApplyBeginWebsiteOccupants()
	{
		m_mWebsiteOccupants.Clear();
	}

	protected void ApplyWebsiteOccupantsChunk(string chunk)
	{
		LL_WebsiteSlotting.DecodeLabelChunk(chunk, m_mWebsiteOccupants);
	}

	protected void ApplyEndWebsiteOccupants()
	{
		m_OnWebsiteOccupantsChanged.Invoke();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_BeginWebsiteOccupants()
	{
		ApplyBeginWebsiteOccupants();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_WebsiteOccupantsChunk(string chunk)
	{
		ApplyWebsiteOccupantsChunk(chunk);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_EndWebsiteOccupants()
	{
		ApplyEndWebsiteOccupants();
	}

	// Statistics: the only stats traffic. The published view is broadcast once (chunked)
	// and the season standings once per session; both stream to JIP via RplSave. Hide keeps
	// the payload and drops the flag.

	protected string m_sStatsViewJson;
	protected bool m_bStatsPublished;
	protected string m_sStatsViewIncoming;
	protected string m_sSeasonJson;
	protected string m_sSeasonIncoming;
	protected string m_sSiteUrl;

	protected ref ScriptInvoker m_OnStatsPublished = new ScriptInvoker();	// (bool published) — true force-opens the stats screen, false force-closes
	protected ref ScriptInvoker m_OnSeasonUpdated = new ScriptInvoker();

	ScriptInvoker GetOnStatsPublished()
	{
		return m_OnStatsPublished;
	}

	ScriptInvoker GetOnSeasonUpdated()
	{
		return m_OnSeasonUpdated;
	}

	bool IsStatsPublished()
	{
		return m_bStatsPublished;
	}

	string GetStatsViewJson()
	{
		return m_sStatsViewJson;
	}

	string GetSeasonJson()
	{
		return m_sSeasonJson;
	}

	string GetSiteUrl()
	{
		return m_sSiteUrl;
	}

	void SetSiteUrl_S(string url)
	{
		if (!Replication.IsServer())
			return;
		if (m_sSiteUrl == url)
			return;

		Rpc(RpcDo_SiteUrl, url);
		m_sSiteUrl = url;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SiteUrl(string url)
	{
		m_sSiteUrl = url;
	}

	// Broadcast RPCs never run on the sending machine (a dedicated server or sessionless
	// Workbench play), unlike Server/Owner-receiver RPCs, so every broadcast is paired with
	// a local apply.
	void PublishStatsView_S(string viewJson)
	{
		if (!Replication.IsServer())
			return;

		array<string> chunks = {};
		LL_StatsManager.SplitChunks(viewJson, chunks);

		Rpc(RpcDo_StatsViewBegin);
		ApplyStatsViewBegin();
		foreach (string chunk : chunks)
		{
			Rpc(RpcDo_StatsViewChunk, chunk);
			ApplyStatsViewChunk(chunk);
		}
		Rpc(RpcDo_StatsViewEnd);
		ApplyStatsViewEnd();
	}

	void HideStatsView_S()
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_StatsHide);
		ApplyStatsHide();
	}

	// Owner-targeted season self-heal landing; same effect as the broadcast path.
	void ClientApplySeason(string siteUrl, string seasonJson)
	{
		if (siteUrl != "")
			m_sSiteUrl = siteUrl;

		if (seasonJson != "")
		{
			m_sSeasonJson = seasonJson;
			m_OnSeasonUpdated.Invoke();
		}
	}

	// Stored raw; clients parse on render.
	void SetSeasonStandings_S(string seasonJson)
	{
		if (!Replication.IsServer())
			return;

		array<string> chunks = {};
		LL_StatsManager.SplitChunks(seasonJson, chunks);

		Rpc(RpcDo_SeasonBegin);
		ApplySeasonBegin();
		foreach (string chunk : chunks)
		{
			Rpc(RpcDo_SeasonChunk, chunk);
			ApplySeasonChunk(chunk);
		}
		Rpc(RpcDo_SeasonEnd);
		ApplySeasonEnd();
	}

	protected void ApplyStatsViewBegin()
	{
		m_sStatsViewIncoming = "";
	}

	protected void ApplyStatsViewChunk(string chunk)
	{
		m_sStatsViewIncoming += chunk;
	}

	protected void ApplyStatsViewEnd()
	{
		m_sStatsViewJson = m_sStatsViewIncoming;
		m_sStatsViewIncoming = "";
		m_bStatsPublished = true;
		m_OnStatsPublished.Invoke(true);
	}

	protected void ApplyStatsHide()
	{
		m_bStatsPublished = false;
		m_OnStatsPublished.Invoke(false);
	}

	protected void ApplySeasonBegin()
	{
		m_sSeasonIncoming = "";
	}

	protected void ApplySeasonChunk(string chunk)
	{
		m_sSeasonIncoming += chunk;
	}

	protected void ApplySeasonEnd()
	{
		m_sSeasonJson = m_sSeasonIncoming;
		m_sSeasonIncoming = "";
		m_OnSeasonUpdated.Invoke();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_StatsViewBegin()
	{
		ApplyStatsViewBegin();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_StatsViewChunk(string chunk)
	{
		ApplyStatsViewChunk(chunk);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_StatsViewEnd()
	{
		ApplyStatsViewEnd();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_StatsHide()
	{
		ApplyStatsHide();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SeasonBegin()
	{
		ApplySeasonBegin();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SeasonChunk(string chunk)
	{
		ApplySeasonChunk(chunk);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SeasonEnd()
	{
		ApplySeasonEnd();
	}

	// JIP snapshot. RplLoad must mirror this write order exactly.

	override event protected bool RplSave(ScriptBitWriter writer)
	{
		int slotCount = m_aSlots.Count();
		writer.WriteInt(slotCount);

		foreach (LL_SlotData slot : m_aSlots)
		{
			writer.WriteInt(slot.m_iRplId);
			writer.WriteString(slot.m_sName);
			writer.WriteString(slot.m_sFactionKey);
			writer.WriteInt(slot.m_iPlayerId);
			writer.WriteInt(slot.m_iGroupId);
			writer.WriteString(slot.m_sGroupName);
			writer.WriteString(slot.m_sIconPath);
			writer.WriteString(slot.m_sIconName);
			writer.WriteString(slot.m_sPrefabName);
			writer.WriteInt(slot.m_iDamageState);
			writer.WriteBool(slot.m_bLocked);
			writer.WriteInt(slot.m_iSortKey);
		}

		int vehicleCount = m_aVehicles.Count();
		writer.WriteInt(vehicleCount);

		foreach (LL_VehicleData vehicle : m_aVehicles)
		{
			writer.WriteInt(vehicle.m_iRplId);
			writer.WriteString(vehicle.m_sName);
			writer.WriteString(vehicle.m_sFactionKey);
			writer.WriteString(vehicle.m_sIconPath);
			writer.WriteString(vehicle.m_sPrefabName);
			writer.WriteInt(vehicle.m_iGroupId);
			writer.WriteBool(vehicle.m_bLocked);
			writer.WriteInt(vehicle.m_iOrderIndex);
		}

		int nameCount = m_mPlayerNames.Count();
		writer.WriteInt(nameCount);

		for (int i = 0; i < nameCount; i++)
		{
			writer.WriteInt(m_mPlayerNames.GetKey(i));
			writer.WriteString(m_mPlayerNames.GetElement(i));
		}

		int readyCount = m_mPlayerReady.Count();
		writer.WriteInt(readyCount);

		for (int i = 0; i < readyCount; i++)
		{
			writer.WriteInt(m_mPlayerReady.GetKey(i));
			writer.WriteBool(m_mPlayerReady.GetElement(i));
		}

		int disconnectedCount = m_aDisconnectedPlayerIds.Count();
		writer.WriteInt(disconnectedCount);

		for (int i = 0; i < disconnectedCount; i++)
		{
			writer.WriteInt(m_aDisconnectedPlayerIds[i]);
		}

		int webCount = m_mWebsiteOccupants.Count();
		writer.WriteInt(webCount);

		for (int i = 0; i < webCount; i++)
		{
			writer.WriteInt(m_mWebsiteOccupants.GetKey(i));
			writer.WriteString(m_mWebsiteOccupants.GetElement(i));
		}

		// One JSON payload can exceed a single serialized string.
		writer.WriteBool(m_bStatsPublished);
		WriteChunkedString(writer, m_sStatsViewJson);
		WriteChunkedString(writer, m_sSeasonJson);
		WriteChunkedString(writer, m_sSiteUrl);

		Print(string.Format("[LL_Lobby] RplSave: %1 slots, %2 vehicles, %3 names",
			slotCount, vehicleCount, nameCount), LogLevel.NORMAL);

		return true;
	}

	protected static void WriteChunkedString(ScriptBitWriter writer, string value)
	{
		array<string> chunks = {};
		LL_StatsManager.SplitChunks(value, chunks);

		writer.WriteInt(chunks.Count());
		foreach (string chunk : chunks)
			writer.WriteString(chunk);
	}

	protected static void ReadChunkedString(ScriptBitReader reader, out string value)
	{
		value = "";

		int count;
		reader.ReadInt(count);
		for (int i = 0; i < count; i++)
		{
			string chunk;
			reader.ReadString(chunk);
			value += chunk;
		}
	}

	override event protected bool RplLoad(ScriptBitReader reader)
	{
		int slotCount;
		reader.ReadInt(slotCount);

		for (int i = 0; i < slotCount; i++)
		{
			LL_SlotData slot = new LL_SlotData();
			reader.ReadInt(slot.m_iRplId);
			reader.ReadString(slot.m_sName);
			reader.ReadString(slot.m_sFactionKey);
			reader.ReadInt(slot.m_iPlayerId);
			reader.ReadInt(slot.m_iGroupId);
			reader.ReadString(slot.m_sGroupName);
			reader.ReadString(slot.m_sIconPath);
			reader.ReadString(slot.m_sIconName);
			reader.ReadString(slot.m_sPrefabName);
			reader.ReadInt(slot.m_iDamageState);
			reader.ReadBool(slot.m_bLocked);
			reader.ReadInt(slot.m_iSortKey);

			// Broadcasts queued during streaming can apply around RplLoad in either order.
			InsertSlotOrdered(slot);
		}

		int vehicleCount;
		reader.ReadInt(vehicleCount);

		for (int i = 0; i < vehicleCount; i++)
		{
			LL_VehicleData vehicle = new LL_VehicleData();
			reader.ReadInt(vehicle.m_iRplId);
			reader.ReadString(vehicle.m_sName);
			reader.ReadString(vehicle.m_sFactionKey);
			reader.ReadString(vehicle.m_sIconPath);
			reader.ReadString(vehicle.m_sPrefabName);
			reader.ReadInt(vehicle.m_iGroupId);
			reader.ReadBool(vehicle.m_bLocked);
			reader.ReadInt(vehicle.m_iOrderIndex);

			InsertVehicleOrdered(vehicle);
		}

		int nameCount;
		reader.ReadInt(nameCount);

		for (int i = 0; i < nameCount; i++)
		{
			int playerId;
			string name;
			reader.ReadInt(playerId);
			reader.ReadString(name);
			m_mPlayerNames.Set(playerId, name);
		}

		int readyCount;
		reader.ReadInt(readyCount);

		for (int i = 0; i < readyCount; i++)
		{
			int readyPlayerId;
			bool ready;
			reader.ReadInt(readyPlayerId);
			reader.ReadBool(ready);
			m_mPlayerReady.Set(readyPlayerId, ready);
		}

		int disconnectedCount;
		reader.ReadInt(disconnectedCount);

		for (int i = 0; i < disconnectedCount; i++)
		{
			int disconnectedPlayerId;
			reader.ReadInt(disconnectedPlayerId);
			if (!m_aDisconnectedPlayerIds.Contains(disconnectedPlayerId))
				m_aDisconnectedPlayerIds.Insert(disconnectedPlayerId);
		}

		int webCount;
		reader.ReadInt(webCount);

		for (int i = 0; i < webCount; i++)
		{
			int webSlotRplId;
			string webLabel;
			reader.ReadInt(webSlotRplId);
			reader.ReadString(webLabel);
			m_mWebsiteOccupants.Set(webSlotRplId, webLabel);
		}

		reader.ReadBool(m_bStatsPublished);
		ReadChunkedString(reader, m_sStatsViewJson);
		ReadChunkedString(reader, m_sSeasonJson);
		ReadChunkedString(reader, m_sSiteUrl);

		Print(string.Format("[LL_Lobby] RplLoad: %1 slots, %2 vehicles, %3 names",
			slotCount, vehicleCount, nameCount), LogLevel.NORMAL);

		// Deferred a frame: listeners may not be initialised mid-stream.
		GetGame().GetCallqueue().CallLater(FireJIPEvents, 0, false);

		return true;
	}

	protected void FireJIPEvents()
	{
		foreach (LL_SlotData slot : m_aSlots)
		{
			m_OnSlotRegistered.Invoke(slot);
		}

		foreach (LL_VehicleData vehicle : m_aVehicles)
		{
			m_OnVehicleRegistered.Invoke(vehicle);
		}

		for (int i = 0; i < m_mPlayerNames.Count(); i++)
		{
			m_OnPlayerNameUpdated.Invoke(m_mPlayerNames.GetKey(i), m_mPlayerNames.GetElement(i));
		}

		for (int i = 0; i < m_mPlayerReady.Count(); i++)
		{
			m_OnPlayerReadyChanged.Invoke(m_mPlayerReady.GetKey(i), m_mPlayerReady.GetElement(i));
		}

		foreach (int disconnectedPlayerId : m_aDisconnectedPlayerIds)
		{
			m_OnPlayerConnectionChanged.Invoke(disconnectedPlayerId, false);
		}

		if (!m_mWebsiteOccupants.IsEmpty())
			m_OnWebsiteOccupantsChanged.Invoke();

		if (m_bStatsPublished)
			m_OnStatsPublished.Invoke(true);
		if (m_sSeasonJson != "")
			m_OnSeasonUpdated.Invoke();

		Print("[LL_Lobby] JIP events fired", LogLevel.NORMAL);
	}

	// Backend GUID when the server has one. On a no-backend dedicated server
	// SCR_PlayerIdentityUtils returns empty and skips its name fallback, so the debug name
	// key covers that case. "" means no reservation is possible.
	protected string ResolveReconnectKey_S(int playerId)
	{
		if (playerId <= 0 || !Replication.IsServer())
			return "";

		// IsNull covers both null forms ("" and all zeros).
		UUID id = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (!id.IsNull())
			return id;

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && gameMode.IsNameReconnectAllowed())
		{
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm)
			{
				string name = pm.GetPlayerName(playerId);
				if (name != "")
					return "name:" + name;
			}
		}

		return "";
	}

	// Called from both connect and audit: audit may never fire on a no-backend server, and
	// the GUID may not be ready at connect.
	protected void CacheReconnectKey_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		string key = ResolveReconnectKey_S(playerId);
		if (key != "")
			m_mPlayerReconnectKeys.Set(playerId, key);
	}

	// The GUID becomes reliable here on real servers.
	override void OnPlayerAuditSuccess(int playerId)
	{
		super.OnPlayerAuditSuccess(playerId);
		CacheReconnectKey_S(playerId);
	}

	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);
		Print(string.Format("[LL_Lobby] LobbyManager: player %1 connected", playerId), LogLevel.NORMAL);

		if (!Replication.IsServer())
			return;

		PlayerManager pm = GetGame().GetPlayerManager();

		// Registered here, not in the controller component's OnPostInit: on dedicated servers
		// that component initialises before the player id is assigned.
		if (pm)
			SetPlayerName_S(playerId, pm.GetPlayerName(playerId));

		CacheReconnectKey_S(playerId);

		NotifyPlayerConnection_S(playerId, true);

		if (!pm)
			return;

		string key;
		if (!m_mPlayerReconnectKeys.Find(playerId, key) || key == "")
			return;

		// -1 is the sentinel; valid RplIds can be negative as ints.
		int reservedSlotRplId = GetReconnectSlot(key);
		if (reservedSlotRplId == -1)
			return;

		LL_SlotData slot = FindSlotByRplId(reservedSlotRplId);
		if (slot && !slot.m_bLocked)
		{
			// The slot is still held by the old playerId during the reservation window.
			int oldPlayerId = slot.m_iPlayerId;
			if (oldPlayerId >= 0)
			{
				LeaveSlot_S(oldPlayerId);
				RemovePlayer_S(oldPlayerId);
			}

			Print(string.Format("[LL_Lobby] Reconnect: reassigning player %1 to slot %2 (key: %3)",
				playerId, reservedSlotRplId, key), LogLevel.NORMAL);

			// The controller component needs a moment to be ready.
			GetGame().GetCallqueue().CallLater(ReconnectPlayer, 500, false, playerId, reservedSlotRplId);
		}
		else
		{
			Print(string.Format("[LL_Lobby] Reconnect: slot %1 no longer available for player %2",
				reservedSlotRplId, playerId), LogLevel.WARNING);
		}
	}

	// VoN follows from the player-assigned event TakeSlot_S fires.
	protected void ReconnectPlayer(int playerId, int slotRplId)
	{
		// Bracketed in the log: a watchdog crash leaves no callstack.
		Print(string.Format("[LL_Lobby] Reconnect: re-possessing player %1 → slot %2", playerId, slotRplId), LogLevel.NORMAL);
		bool ok = TakeSlot_S(playerId, slotRplId);
		Print(string.Format("[LL_Lobby] Reconnect: player %1 re-possess done (success=%2)", playerId, ok), LogLevel.NORMAL);
	}

	// Admin login mid-session: rows re-read SCR_Global.IsAdmin on the recolor event.
	override void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		super.OnPlayerRoleChange(playerId, roleFlags);

		bool connected = true;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
			connected = pm.IsPlayerConnected(playerId);

		m_OnPlayerConnectionChanged.Invoke(playerId, connected);
	}

	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (!Replication.IsServer())
			return;

		LL_SlotData slot = FindSlotByPlayerId(playerId);
		if (slot)
		{
			// The backend identity may already be gone at disconnect.
			string key;
			if (!m_mPlayerReconnectKeys.Find(playerId, key) || key == "")
				key = ResolveReconnectKey_S(playerId);

			if (key != "")
			{
				m_mDisconnectedPlayers.Set(key, slot.m_iRplId);

				int generation = 0;
				m_mReconnectGeneration.Find(key, generation);
				generation++;
				m_mReconnectGeneration.Set(key, generation);

				// A value <= 0 holds the slot indefinitely: no timer is scheduled.
				LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
				int reconnectTime = 120000;
				if (gameMode)
					reconnectTime = gameMode.GetReconnectTime();

				if (reconnectTime > 0)
				{
					GetGame().GetCallqueue().CallLater(ClearReconnectReservation, reconnectTime, false, key, generation);
					Print(string.Format("[LL_Lobby] Reserved slot %1 for player %2 (key: %3) — releases in %4s unless they reconnect",
						slot.m_iRplId, playerId, key, reconnectTime / 1000), LogLevel.NORMAL);
				}
				else
				{
					Print(string.Format("[LL_Lobby] Reserved slot %1 for player %2 (key: %3) — held indefinitely (no timeout)",
						slot.m_iRplId, playerId, key), LogLevel.NORMAL);
				}

				// The slot stays occupied by the old playerId so nobody can take it meanwhile.
				SetPlayerReady_S(playerId, false);
				NotifyPlayerConnection_S(playerId, false);
			}
			else
			{
				LeaveSlot_S(playerId);
				RemovePlayer_S(playerId);
			}
		}
		else
		{
			RemovePlayer_S(playerId);
		}

		Print(string.Format("[LL_Lobby] LobbyManager: player %1 disconnected", playerId), LogLevel.NORMAL);
	}

	// Reconnect timer expiry.
	protected void ClearReconnectReservation(string key, int generation)
	{
		// A newer reservation bumped the generation; this timer is stale.
		int currentGeneration;
		if (!m_mReconnectGeneration.Find(key, currentGeneration) || currentGeneration != generation)
			return;

		int slotRplId;
		if (!m_mDisconnectedPlayers.Find(key, slotRplId))
			return;

		m_mDisconnectedPlayers.Remove(key);
		Print(string.Format("[LL_Lobby] Reconnect reservation expired for key: %1", key), LogLevel.NORMAL);

		LL_SlotData slot = FindSlotByRplId(slotRplId);
		if (slot && slot.m_iPlayerId >= 0)
		{
			int oldPlayerId = slot.m_iPlayerId;
			LeaveSlot_S(oldPlayerId);
			RemovePlayer_S(oldPlayerId);
		}
	}

	int GetReconnectSlot(string playerGUID)
	{
		int slotRplId;
		if (m_mDisconnectedPlayers.Find(playerGUID, slotRplId))
		{
			m_mDisconnectedPlayers.Remove(playerGUID);
			return slotRplId;
		}
		return -1;
	}
}