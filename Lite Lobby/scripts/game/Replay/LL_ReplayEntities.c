class LL_ReplayEntityRecord : Managed
{
	int m_iEid;
	IEntity m_Entity;
	EntityID m_EntityId;
	string m_sKind;
	string m_sDefinition;
	int m_iLastX;
	int m_iLastZ;
	int m_iLastAlt;
	int m_iLastYaw;
	bool m_bWritten;
	bool m_bRemoved;
	bool m_bUnconscious;
	int m_iSeatVehicle = -1;
	string m_sSeatRole;
	string m_sSeatName;
	LL_TrackerComponent m_Tracker;
	ref LL_ReplayCharacterHooks m_Hooks;
}

//! The shot, life-state and seat events do not name the character they belong to, so each
//! recorded character carries one of these to forward them with its recording id.
class LL_ReplayCharacterHooks : Managed
{
	protected int m_iEid;
	protected IEntity m_Character;
	protected LL_ReplayEntities m_Entities;
	protected LL_ReplayCombat m_Combat;
	protected EventHandlerManagerComponent m_EventManager;
	protected SCR_CharacterControllerComponent m_Controller;
	protected SCR_CompartmentAccessComponent m_Access;

	void Attach(int eid, notnull SCR_ChimeraCharacter character, LL_ReplayEntities entities, LL_ReplayCombat combat)
	{
		m_iEid = eid;
		m_Character = character;
		m_Entities = entities;
		m_Combat = combat;

		m_EventManager = EventHandlerManagerComponent.Cast(character.FindComponent(EventHandlerManagerComponent));
		if (m_EventManager)
		{
			m_EventManager.RegisterScriptHandler("OnProjectileShot", this, OnProjectileShot);
			m_EventManager.RegisterScriptHandler("OnGrenadeThrown", this, OnGrenadeThrown);
		}

		m_Controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (m_Controller)
			m_Controller.m_OnLifeStateChanged.Insert(OnLifeStateChanged);

		m_Access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (m_Access)
		{
			m_Access.GetOnCompartmentEntered().Insert(OnCompartmentEntered);
			m_Access.GetOnCompartmentLeft().Insert(OnCompartmentLeft);
		}
	}

	void Detach()
	{
		if (m_EventManager)
		{
			m_EventManager.RemoveScriptHandler("OnProjectileShot", this, OnProjectileShot);
			m_EventManager.RemoveScriptHandler("OnGrenadeThrown", this, OnGrenadeThrown);
		}

		if (m_Controller)
			m_Controller.m_OnLifeStateChanged.Remove(OnLifeStateChanged);

		if (m_Access)
		{
			m_Access.GetOnCompartmentEntered().Remove(OnCompartmentEntered);
			m_Access.GetOnCompartmentLeft().Remove(OnCompartmentLeft);
		}
	}

	void OnProjectileShot(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		if (m_Combat)
			m_Combat.OnShot(m_iEid, m_Character, weapon, entity, false);
	}

	void OnGrenadeThrown(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		if (m_Combat)
			m_Combat.OnShot(m_iEid, m_Character, weapon, entity, true);
	}

	void OnLifeStateChanged(ECharacterLifeState previousLifeState, ECharacterLifeState newLifeState, bool isJIP)
	{
		if (m_Combat)
			m_Combat.OnLifeState(m_iEid, m_Character, previousLifeState, newLifeState);
	}

	void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (m_Entities)
			m_Entities.OnSeatEntered(m_iEid, targetEntity, manager, mgrID, slotID);
	}

	void OnCompartmentLeft(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (m_Entities)
			m_Entities.OnSeatLeft(m_iEid);
	}
}

//! Recording ids, movement, seats, the roster and the players. Characters come from the
//! game's own list and vehicles from the editable-entity registry, so no world query runs.
class LL_ReplayEntities : Managed
{
	protected static const int CHARACTER_SLICES = 10;
	protected static const int VEHICLE_SLICES = 5;

	protected LL_ReplayWriter m_Writer;
	protected LL_ReplayCombat m_Combat;
	protected SCR_EditableEntityCore m_Core;

	protected int m_iNextEid = 1;
	protected ref array<ref LL_ReplayEntityRecord> m_aCharacters = {};
	protected ref array<ref LL_ReplayEntityRecord> m_aVehicles = {};
	protected ref map<EntityID, LL_ReplayEntityRecord> m_mByEntity = new map<EntityID, LL_ReplayEntityRecord>();
	protected ref map<int, LL_ReplayEntityRecord> m_mByEid = new map<int, LL_ReplayEntityRecord>();
	protected ref map<string, int> m_mRestoredEids = new map<string, int>();
	protected int m_iDiscoveryCursor;
	protected int m_iCharacterCursor;
	protected int m_iVehicleCursor;

	protected int m_iNextPref = 1;
	protected int m_iNextGid = 1;
	protected ref map<string, int> m_mPrefByIdentity = new map<string, int>();
	protected ref map<int, int> m_mPrefByPlayer = new map<int, int>();
	protected ref map<int, string> m_mNameByPref = new map<int, string>();
	protected ref map<int, bool> m_mConnectedByPref = new map<int, bool>();
	protected ref map<int, int> m_mControlledByPref = new map<int, int>();
	protected ref map<int, int> m_mGidByGroupKey = new map<int, int>();
	protected ref map<int, string> m_mGroupLines = new map<int, string>();
	protected ref map<int, string> m_mSlotLines = new map<int, string>();
	protected ref map<int, int> m_mOccupantBySlot = new map<int, int>();

	void Init(LL_ReplayWriter writer, LL_ReplayCombat combat)
	{
		m_Writer = writer;
		m_Combat = combat;
	}

	void Start_S()
	{
		m_Core = SCR_EditableEntityCore.Cast(SCR_EditableEntityCore.GetInstance(SCR_EditableEntityCore));
		if (m_Core)
		{
			set<SCR_EditableEntityComponent> entities = new set<SCR_EditableEntityComponent>();
			m_Core.GetAllEntities(entities);
			foreach (SCR_EditableEntityComponent editable : entities)
			{
				OnEditableRegistered(editable);
			}

			m_Core.Event_OnEntityRegistered.Insert(OnEditableRegistered);
			m_Core.Event_OnEntityUnregistered.Insert(OnEditableUnregistered);
		}

		Print(string.Format("[LL_Lobby] Replay: %1 vehicle(s) registered at start", m_aVehicles.Count()), LogLevel.NORMAL);

		array<LL_TrackerComponent> trackers = LL_TrackerComponent.GetAll();
		if (trackers)
		{
			foreach (LL_TrackerComponent tracker : trackers)
			{
				if (tracker)
					AddTracker_S(tracker);
			}
		}
	}

	void Stop_S()
	{
		if (m_Core)
		{
			m_Core.Event_OnEntityRegistered.Remove(OnEditableRegistered);
			m_Core.Event_OnEntityUnregistered.Remove(OnEditableUnregistered);
			m_Core = null;
		}

		foreach (LL_ReplayEntityRecord record : m_aCharacters)
		{
			if (record.m_Hooks)
				record.m_Hooks.Detach();
		}
	}

	//! One tenth of the characters and one fifth of the vehicles and trackers per call.
	void Tick_S()
	{
		DiscoverCharacters_S();

		if (LL_GameModeCoop.IsHardFreezeActive())
			return;

		m_iCharacterCursor = SampleSlice(m_aCharacters, m_iCharacterCursor, CHARACTER_SLICES);
		m_iVehicleCursor = SampleSlice(m_aVehicles, m_iVehicleCursor, VEHICLE_SLICES);
	}

	void SecondStep_S()
	{
		PollRoster_S();
		PollPlayers_S();
	}

	int EidOf(IEntity entity)
	{
		if (!entity)
			return -1;

		LL_ReplayEntityRecord record;
		if (m_mByEntity.Find(entity.GetID(), record) && record && !record.m_bRemoved)
			return record.m_iEid;

		return -1;
	}

	LL_ReplayEntityRecord RecordOf(int eid)
	{
		LL_ReplayEntityRecord record;
		m_mByEid.Find(eid, record);
		return record;
	}

	//! The identity can arrive seconds after the connection. Until then the player has a
	//! reference of their own, which is replaced by an earlier one of the same identity once known.
	int PrefOfPlayer(int playerId)
	{
		if (playerId <= 0)
			return -1;

		string identity;
		LL_LobbyManager lobby = LL_LobbyManager.GetInstance();
		if (lobby)
			identity = lobby.GetReconnectKeyForPlayer_S(playerId);

		int pref;
		bool known = m_mPrefByPlayer.Find(playerId, pref);
		if (identity == "" && known)
			return pref;

		int identityPref;
		if (identity != "" && m_mPrefByIdentity.Find(identity, identityPref))
		{
			m_mPrefByPlayer.Set(playerId, identityPref);
			return identityPref;
		}

		if (!known)
		{
			pref = m_iNextPref;
			m_iNextPref++;
			m_mPrefByPlayer.Set(playerId, pref);
		}

		if (identity != "")
		{
			m_mPrefByIdentity.Set(identity, pref);
			m_Writer.AddIdentity(pref, identity);
		}

		return pref;
	}

	void OnSeatEntered(int eid, IEntity target, BaseCompartmentManagerComponent manager, int mgrID, int slotID)
	{
		LL_ReplayEntityRecord record = RecordOf(eid);
		if (!record || !target)
			return;

		int vehicleEid = EidOf(SCR_EntityHelper.GetMainParent(target, true));
		if (vehicleEid < 0)
			vehicleEid = EidOf(target);
		if (vehicleEid < 0)
			return;

		string role = "cargo";
		string seatName;
		if (manager)
		{
			BaseCompartmentSlot slot = manager.FindCompartment(slotID, mgrID);
			if (slot)
			{
				ECompartmentType type = slot.GetType();
				if (type == ECompartmentType.PILOT)
					role = "driver";
				else if (type == ECompartmentType.TURRET)
					role = "gunner";

				seatName = slot.GetCompartmentName(true);
			}
		}

		record.m_iSeatVehicle = vehicleEid;
		record.m_sSeatRole = role;
		record.m_sSeatName = seatName;
		m_Writer.Add(SeatLine(record));
	}

	void OnSeatLeft(int eid)
	{
		LL_ReplayEntityRecord record = RecordOf(eid);
		if (!record || record.m_iSeatVehicle < 0)
			return;

		record.m_iSeatVehicle = -1;
		m_Writer.Add(string.Format("V %1 -", eid));
	}

	void WriteRosterSnapshot_S()
	{
		foreach (int gid, string groupLine : m_mGroupLines)
		{
			m_Writer.AddSnapshotLine(groupLine);
		}

		foreach (int pref, string name : m_mNameByPref)
		{
			m_Writer.AddSnapshotLine(string.Format("N %1 %2", pref, LL_ReplayWriter.Str(name)));
		}

		foreach (int sortKey, string slotLine : m_mSlotLines)
		{
			m_Writer.AddSnapshotLine(slotLine);
		}

		foreach (int slotKey, int occupant : m_mOccupantBySlot)
		{
			m_Writer.AddSnapshotLine(string.Format("O %1 %2", slotKey, LL_ReplayWriter.IntOr(occupant, occupant > 0)));
		}

		foreach (int connectedPref, bool connected : m_mConnectedByPref)
		{
			if (connected)
				m_Writer.AddSnapshotLine(string.Format("D %1 1", connectedPref));
			else
				m_Writer.AddSnapshotLine(string.Format("D %1 0", connectedPref));
		}

		foreach (int controllerPref, int controlled : m_mControlledByPref)
		{
			m_Writer.AddSnapshotLine(string.Format("C %1 %2", controllerPref, LL_ReplayWriter.IntOr(controlled, controlled > 0)));
		}
	}

	//! The live records at the moment a snapshot starts; it is written over several ticks.
	void CollectSnapshotRecords(notnull array<LL_ReplayEntityRecord> outRecords)
	{
		foreach (LL_ReplayEntityRecord character : m_aCharacters)
		{
			if (!character.m_bRemoved)
				outRecords.Insert(character);
		}

		foreach (LL_ReplayEntityRecord vehicle : m_aVehicles)
		{
			if (!vehicle.m_bRemoved)
				outRecords.Insert(vehicle);
		}
	}

	void WriteSnapshotRecord_S(LL_ReplayEntityRecord record)
	{
		if (!record || record.m_bRemoved)
			return;

		m_Writer.AddSnapshotLine(record.m_sDefinition);
		if (record.m_bWritten)
			m_Writer.AddSnapshotLine(PositionLine(record));
		if (record.m_bUnconscious)
			m_Writer.AddSnapshotLine(string.Format("L %1 u - - - -", record.m_iEid));
		if (record.m_iSeatVehicle > 0)
			m_Writer.AddSnapshotLine(SeatLine(record));
	}

	//! Trackers are left out: a tracker shares its target's persistent id, and the target must
	//! get its own recording id back.
	void CaptureContinuation(notnull LL_ReplayContinuation continuation)
	{
		continuation.nextEid = m_iNextEid;
		continuation.nextPref = m_iNextPref;

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
		{
			array<LL_ReplayEntityRecord> records = {};
			CollectSnapshotRecords(records);
			foreach (LL_ReplayEntityRecord record : records)
			{
				if (!record.m_Entity || record.m_Tracker)
					continue;

				string persistentId = persistence.GetId(record.m_Entity);
				if (persistentId == "")
					continue;

				LL_ReplayEntityPair pair = new LL_ReplayEntityPair();
				pair.persistentId = persistentId;
				pair.eid = record.m_iEid;
				continuation.entities.Insert(pair);
			}
		}

		foreach (string identity, int pref : m_mPrefByIdentity)
		{
			LL_ReplayPlayerPair player = new LL_ReplayPlayerPair();
			player.identity = identity;
			player.pref = pref;
			continuation.players.Insert(player);
		}
	}

	//! A restored character or vehicle gets its old recording id back when its persistent id
	//! is found here on the first pass that sees it.
	void RestoreContinuation(notnull LL_ReplayContinuation continuation)
	{
		m_iNextEid = continuation.nextEid;
		m_iNextPref = continuation.nextPref;

		foreach (LL_ReplayEntityPair pair : continuation.entities)
		{
			m_mRestoredEids.Set(pair.persistentId, pair.eid);
		}

		foreach (LL_ReplayPlayerPair player : continuation.players)
		{
			m_mPrefByIdentity.Set(player.identity, player.pref);
		}
	}

	protected void DiscoverCharacters_S()
	{
		array<SCR_ChimeraCharacter> characters = SCR_CharacterRegistrationComponent.GetChimeraCharacters();
		if (!characters)
			return;

		int count = characters.Count();
		if (count == 0)
			return;

		int step = count / CHARACTER_SLICES + 1;
		for (int n = 0; n < step; n++)
		{
			if (m_iDiscoveryCursor >= count)
				m_iDiscoveryCursor = 0;

			SCR_ChimeraCharacter character = characters[m_iDiscoveryCursor];
			m_iDiscoveryCursor++;

			if (character && !m_mByEntity.Contains(character.GetID()))
				AddCharacter_S(character);
		}
	}

	protected void AddCharacter_S(notnull SCR_ChimeraCharacter character)
	{
		LL_ReplayEntityRecord record = NewRecord(character, "man");

		string side = "-";
		Faction faction = character.GetFaction();
		if (faction)
			side = faction.GetFactionKey();

		int gid = CharacterGroup_S(character);
		record.m_sDefinition = string.Format("E %1 man %2 %3 %4 %5", record.m_iEid, LL_ReplayWriter.Tok(side),
			LL_ReplayWriter.IntOr(gid, gid > 0), LL_ReplayWriter.Tok(PrefabFileName(character)), LL_ReplayWriter.Str(EditableName(character)));

		m_aCharacters.Insert(record);
		m_Writer.Add(record.m_sDefinition);

		record.m_Hooks = new LL_ReplayCharacterHooks();
		record.m_Hooks.Attach(record.m_iEid, character, this, m_Combat);

		Sample(record);
	}

	protected void OnEditableRegistered(SCR_EditableEntityComponent editable)
	{
		if (!editable || editable.GetEntityType() != EEditableEntityType.VEHICLE)
			return;

		IEntity owner = editable.GetOwner();
		if (!owner || m_mByEntity.Contains(owner.GetID()))
			return;

		string kind = VehicleKind(owner);
		LL_ReplayEntityRecord record = NewRecord(owner, kind);

		string side = "-";
		Faction faction = editable.GetFaction();
		if (faction)
			side = faction.GetFactionKey();

		record.m_sDefinition = string.Format("E %1 %2 %3 - %4 %5", record.m_iEid, kind, LL_ReplayWriter.Tok(side),
			LL_ReplayWriter.Tok(PrefabFileName(owner)), LL_ReplayWriter.Str(editable.GetDisplayName()));

		m_aVehicles.Insert(record);
		m_Writer.Add(record.m_sDefinition);
		Sample(record);
	}

	protected void OnEditableUnregistered(SCR_EditableEntityComponent editable)
	{
		if (!editable)
			return;

		IEntity owner = editable.GetOwner();
		if (!owner)
			return;

		LL_ReplayEntityRecord record;
		if (m_mByEntity.Find(owner.GetID(), record) && record)
			Remove(record);
	}

	protected void AddTracker_S(notnull LL_TrackerComponent tracker)
	{
		LL_TrackerData data = new LL_TrackerData();
		tracker.ExportConfig(data);

		string sides = data.m_sVisibleFactions;
		if (data.m_bAnyFaction || sides == "")
			sides = "*";

		LL_ReplayEntityRecord record = new LL_ReplayEntityRecord();
		record.m_iEid = m_iNextEid;
		m_iNextEid++;
		record.m_sKind = "tracker";
		record.m_Tracker = tracker;
		record.m_Entity = tracker.GetOwner();
		record.m_sDefinition = string.Format("E %1 tracker %2 - %3 %4", record.m_iEid, LL_ReplayWriter.Tok(sides),
			LL_ReplayWriter.Tok(PrefabFileName(tracker.GetOwner())), LL_ReplayWriter.Str(data.m_sLabel));

		m_mByEid.Set(record.m_iEid, record);
		m_aVehicles.Insert(record);
		m_Writer.Add(record.m_sDefinition);
		Sample(record);
	}

	protected LL_ReplayEntityRecord NewRecord(notnull IEntity entity, string kind)
	{
		LL_ReplayEntityRecord record = new LL_ReplayEntityRecord();
		record.m_iEid = TakeEid(entity);
		record.m_Entity = entity;
		record.m_EntityId = entity.GetID();
		record.m_sKind = kind;
		m_mByEntity.Set(record.m_EntityId, record);
		m_mByEid.Set(record.m_iEid, record);
		return record;
	}

	protected int TakeEid(notnull IEntity entity)
	{
		if (!m_mRestoredEids.IsEmpty())
		{
			PersistenceSystem persistence = PersistenceSystem.GetInstance();
			if (persistence)
			{
				string persistentId = persistence.GetId(entity);
				int restored;
				if (persistentId != "" && m_mRestoredEids.Find(persistentId, restored))
				{
					m_mRestoredEids.Remove(persistentId);
					return restored;
				}
			}
		}

		int eid = m_iNextEid;
		m_iNextEid++;
		return eid;
	}

	protected int SampleSlice(array<ref LL_ReplayEntityRecord> records, int cursor, int slices)
	{
		int count = records.Count();
		if (count == 0)
			return 0;

		int step = count / slices + 1;
		for (int n = 0; n < step; n++)
		{
			if (cursor >= count)
			{
				Compact(records);
				count = records.Count();
				cursor = 0;
				if (count == 0)
					return 0;
			}

			LL_ReplayEntityRecord record = records[cursor];
			cursor++;
			if (record.m_bRemoved)
				continue;

			if (!record.m_Entity)
			{
				Remove(record);
				continue;
			}

			Sample(record);
		}

		return cursor;
	}

	protected void Sample(LL_ReplayEntityRecord record)
	{
		vector position;
		int yaw;
		if (record.m_Tracker)
		{
			float trackerYaw;
			if (!record.m_Tracker.SampleTarget(position, trackerYaw))
			{
				Remove(record);
				return;
			}

			yaw = Math.Round(trackerYaw);
		}
		else
		{
			if (!record.m_Entity)
				return;

			position = record.m_Entity.GetOrigin();
			vector angles = record.m_Entity.GetYawPitchRoll();
			yaw = Math.Round(angles[0]);
		}

		yaw = ((yaw % 360) + 360) % 360;
		int x = Math.Round(position[0]);
		int z = Math.Round(position[2]);
		int alt = Math.Round(position[1]);

		if (record.m_bWritten)
		{
			bool moved = Math.AbsInt(x - record.m_iLastX) >= 1 || Math.AbsInt(z - record.m_iLastZ) >= 1 || Math.AbsInt(alt - record.m_iLastAlt) >= 2;
			int turn = Math.AbsInt(yaw - record.m_iLastYaw);
			if (turn > 180)
				turn = 360 - turn;

			if (!moved && turn < 5)
				return;
		}

		record.m_iLastX = x;
		record.m_iLastZ = z;
		record.m_iLastAlt = alt;
		record.m_iLastYaw = yaw;
		record.m_bWritten = true;
		m_Writer.Add(PositionLine(record));
	}

	protected void Remove(LL_ReplayEntityRecord record)
	{
		if (record.m_bRemoved)
			return;

		record.m_bRemoved = true;
		if (record.m_Hooks)
		{
			record.m_Hooks.Detach();
			record.m_Hooks = null;
		}

		m_mByEntity.Remove(record.m_EntityId);
		m_Writer.Add(string.Format("Q %1", record.m_iEid));
	}

	protected void Compact(array<ref LL_ReplayEntityRecord> records)
	{
		for (int i = records.Count() - 1; i >= 0; i--)
		{
			if (records[i].m_bRemoved)
			{
				m_mByEid.Remove(records[i].m_iEid);
				records.Remove(i);
			}
		}
	}

	protected string PositionLine(LL_ReplayEntityRecord record)
	{
		return string.Format("P %1 %2 %3 %4 %5", record.m_iEid, record.m_iLastX, record.m_iLastZ, record.m_iLastAlt, record.m_iLastYaw);
	}

	protected string SeatLine(LL_ReplayEntityRecord record)
	{
		return string.Format("V %1 %2 %3 %4", record.m_iEid, record.m_iSeatVehicle, record.m_sSeatRole, LL_ReplayWriter.Str(record.m_sSeatName));
	}

	protected void PollRoster_S()
	{
		LL_LobbyManager lobby = LL_LobbyManager.GetInstance();
		if (!lobby)
			return;

		foreach (LL_SlotData slot : lobby.GetSlots())
		{
			int gid = GroupId(slot.m_iGroupId, slot.m_sFactionKey, slot.m_sGroupName);
			string slotLine = string.Format("S %1 %2 %3 %4", slot.m_iSortKey, LL_ReplayWriter.IntOr(gid, gid > 0),
				LL_ReplayWriter.Tok(slot.m_sFactionKey), LL_ReplayWriter.Str(slot.m_sName));

			string previousLine;
			if (!m_mSlotLines.Find(slot.m_iSortKey, previousLine) || previousLine != slotLine)
			{
				m_mSlotLines.Set(slot.m_iSortKey, slotLine);
				m_Writer.Add(slotLine);
			}

			int occupant = PrefOfPlayer(slot.m_iPlayerId);
			int previousOccupant;
			if (!m_mOccupantBySlot.Find(slot.m_iSortKey, previousOccupant) || previousOccupant != occupant)
			{
				m_mOccupantBySlot.Set(slot.m_iSortKey, occupant);
				m_Writer.Add(string.Format("O %1 %2", slot.m_iSortKey, LL_ReplayWriter.IntOr(occupant, occupant > 0)));
			}
		}
	}

	protected void PollPlayers_S()
	{
		PlayerManager players = GetGame().GetPlayerManager();
		LL_LobbyManager lobby = LL_LobbyManager.GetInstance();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);

		set<int> connected = new set<int>();
		foreach (int playerId : playerIds)
		{
			int pref = PrefOfPlayer(playerId);
			if (pref <= 0)
				continue;

			connected.Insert(pref);

			string name = players.GetPlayerName(playerId);
			if (lobby)
			{
				string lobbyName = lobby.GetPlayerName(playerId);
				if (lobbyName != "")
					name = lobbyName;
			}

			string previousName;
			if (!m_mNameByPref.Find(pref, previousName) || previousName != name)
			{
				m_mNameByPref.Set(pref, name);
				m_Writer.Add(string.Format("N %1 %2", pref, LL_ReplayWriter.Str(name)));
			}

			bool wasConnected;
			if (!m_mConnectedByPref.Find(pref, wasConnected) || !wasConnected)
			{
				m_mConnectedByPref.Set(pref, true);
				m_Writer.Add(string.Format("D %1 1", pref));
			}

			int eid = EidOf(players.GetPlayerControlledEntity(playerId));
			int previousEid;
			if (!m_mControlledByPref.Find(pref, previousEid) || previousEid != eid)
			{
				m_mControlledByPref.Set(pref, eid);
				m_Writer.Add(string.Format("C %1 %2", pref, LL_ReplayWriter.IntOr(eid, eid > 0)));
			}
		}

		array<int> gone = {};
		foreach (int knownPref, bool isConnected : m_mConnectedByPref)
		{
			if (isConnected && !connected.Contains(knownPref))
				gone.Insert(knownPref);
		}

		foreach (int goneRef : gone)
		{
			m_mConnectedByPref.Set(goneRef, false);
			m_Writer.Add(string.Format("D %1 0", goneRef));
		}
	}

	protected int CharacterGroup_S(notnull SCR_ChimeraCharacter character)
	{
		LL_LobbyManager lobby = LL_LobbyManager.GetInstance();
		if (lobby)
		{
			foreach (LL_SlotData slot : lobby.GetSlots())
			{
				if (lobby.ResolveSlotEntity(slot.m_iRplId) == character)
					return GroupId(slot.m_iGroupId, slot.m_sFactionKey, slot.m_sGroupName);
			}
		}

		AIAgent agent = SCR_AIUtils.GetAIAgent(character);
		if (!agent)
			return -1;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return -1;

		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		if (!rpl)
			return -1;

		string side;
		Faction faction = group.GetFaction();
		if (faction)
			side = faction.GetFactionKey();

		int groupKey = rpl.Id();
		return GroupId(groupKey, side, group.GetCustomNameWithOriginal());
	}

	protected int GroupId(int groupKey, string side, string callsign)
	{
		if (groupKey == -1)
			return -1;

		int gid;
		if (m_mGidByGroupKey.Find(groupKey, gid))
			return gid;

		gid = m_iNextGid;
		m_iNextGid++;
		m_mGidByGroupKey.Set(groupKey, gid);

		string groupLine = string.Format("GROUP %1 %2 %3", gid, LL_ReplayWriter.Tok(side), LL_ReplayWriter.Str(callsign));
		m_mGroupLines.Set(gid, groupLine);
		m_Writer.Add(groupLine);
		return gid;
	}

	protected static string VehicleKind(notnull IEntity owner)
	{
		IEntity usageOwner;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.FindOnNearestParent(owner, usageOwner);
		EAIVehicleType aiType = EAIVehicleType.INVALID;
		if (usage)
			aiType = usage.GetVehicleType();

		if (aiType == EAIVehicleType.AIRCRAFT_HELICOPTER)
			return "heli";
		if (aiType == EAIVehicleType.AIRCRAFT_PLANE)
			return "plane";
		if (aiType == EAIVehicleType.STATIC_ARTILLERY)
			return "mortar";
		if (aiType == EAIVehicleType.STATIC_WEAPON)
			return "static";
		if (aiType == EAIVehicleType.GROUND_VEHICLE_TRACKED)
			return "tracked";

		Vehicle vehicle = Vehicle.Cast(owner);
		if (vehicle)
		{
			if (aiType == EAIVehicleType.GROUND_VEHICLE_WHEELED)
			{
				EVehicleType type = vehicle.m_eVehicleType;
				if (type == EVehicleType.APC)
					return "apc";
				if (type == EVehicleType.CAR)
					return "car";
				if (type == EVehicleType.TRUCK || type == EVehicleType.FUEL_TRUCK || type == EVehicleType.COMM_TRUCK
					|| type == EVehicleType.SUPPLY_TRUCK || type == EVehicleType.REPAIR || type == EVehicleType.ARSENAL)
					return "truck";
			}

			if (vehicle.FindComponent(VehicleHelicopterSimulation))
				return "heli";

			return "vehicle";
		}

		if (Turret.Cast(owner))
			return "static";

		return "vehicle";
	}

	static string PrefabFileName(IEntity entity)
	{
		if (!entity)
			return "-";

		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return "-";

		string path = data.GetPrefabName();
		int slash = path.LastIndexOf("/");
		if (slash >= 0)
			path = path.Substring(slash + 1, path.Length() - slash - 1);

		int dot = path.LastIndexOf(".");
		if (dot > 0)
			path = path.Substring(0, dot);

		return path;
	}

	protected static string EditableName(notnull IEntity entity)
	{
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(entity.FindComponent(SCR_EditableEntityComponent));
		if (!editable)
			return "";

		return editable.GetDisplayName();
	}
}
