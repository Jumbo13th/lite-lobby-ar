// The lobby's own session record: what the game's save cannot know about a round. Written
// only in the game phase; applied by the game mode in one step once the persistence
// system is active and every saved body has registered again. Plain objects, so the
// context's field reflection writes and reads them.

class LL_SquadRecord
{
	// The squad entity's own name, not the roster callsign: the body is re-attached by it.
	string name;
	int frequency;
}

class LL_SlotRecord
{
	string name;
	UUID body;
	string squad;
	string holderKey;
	string holderName;
	bool kia;
	bool locked;
	int sortKey;
}

class LL_SessionRecord
{
	int version;
	int state;
	float freezeRemaining;
	float hardFreezeRemaining;
	bool dayAdvance;
	float elapsedSeconds;
	int missionEndDuration;
	float missionEndStartedAt;
	// The recorder's continuation, so the statistics describe the same moment as the world.
	ref LL_StatsContinuation stats;
	ref array<ref LL_SquadRecord> squads = {};
	ref array<ref LL_SlotRecord> slots = {};
}

// Proxy state the persistence system instantiates from the config; the serializer reads
// the live lobby instead.
class LL_SessionData : PersistentState
{
}

class LL_SessionSerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	override static typename GetTargetType()
	{
		return LL_SessionData;
	}

	override ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!gm || !mgr || gm.GetState() != SCR_EGameModeState.GAME)
			return ESerializeResult.DEFAULT;

		LL_SessionRecord record = new LL_SessionRecord();
		record.version = VERSION;
		record.state = gm.GetState();
		record.freezeRemaining = gm.GetFreezeTimeRemaining();
		record.hardFreezeRemaining = gm.GetSavedHardFreezeRemaining();
		record.dayAdvance = gm.GetPreHoldDayAdvance();
		record.elapsedSeconds = gm.GetElapsedTime();
		record.missionEndDuration = gm.GetMissionEndDuration();
		record.missionEndStartedAt = gm.GetMissionEndStartedAt();

		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			record.stats = stats.CaptureContinuation_S();

		set<int> seenGroups = new set<int>();
		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			SCR_AIGroup group = null;
			RplComponent groupRpl = RplComponent.Cast(Replication.FindItem(slot.m_iGroupId));
			if (groupRpl)
				group = SCR_AIGroup.Cast(groupRpl.GetEntity());

			string squadName = "";
			if (group)
			{
				squadName = group.GetName();
				if (!seenGroups.Contains(slot.m_iGroupId))
				{
					seenGroups.Insert(slot.m_iGroupId);
					LL_SquadRecord squad = new LL_SquadRecord();
					squad.name = squadName;
					squad.frequency = group.GetRadioFrequency();
					record.squads.Insert(squad);
				}
			}

			LL_SlotRecord entry = new LL_SlotRecord();
			entry.name = slot.m_sName;
			entry.squad = squadName;
			entry.kia = slot.IsDestroyed();
			entry.locked = slot.m_bLocked;
			entry.sortKey = slot.m_iSortKey;

			// A null id is written, not skipped: the load refuses on it.
			entry.body = UUID.NULL_UUID;
			IEntity body = mgr.GetSlotEntity_S(slot.m_iRplId);
			if (body)
				entry.body = GetSystem().GetId(body);
			if (entry.body.IsNull())
				Print(string.Format("[LL_Lobby] Snapshot: slot '%1' body is not tracked by the persistence system", slot.m_sName), LogLevel.WARNING);

			if (slot.m_iPlayerId >= 0)
			{
				entry.holderKey = mgr.GetReconnectKeyForPlayer_S(slot.m_iPlayerId);
				entry.holderName = mgr.GetPlayerName(slot.m_iPlayerId);
			}

			record.slots.Insert(entry);
		}

		context.WriteValue("version", VERSION);
		bool prev = context.EnableTypeDiscriminator(false);
		context.WriteValue("session", record);
		context.EnableTypeDiscriminator(prev);
		return ESerializeResult.OK;
	}

	// Returns true even on a bad record: the refusal is the lobby's own, with its cause.
	override bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		if (!gm)
			return true;

		int version;
		context.ReadValue("version", version);
		if (version != VERSION)
		{
			gm.ReportResumeFailure_S(string.Format("unknown session record version %1", version));
			return true;
		}

		LL_SessionRecord record = new LL_SessionRecord();
		bool prev = context.EnableTypeDiscriminator(false);
		bool ok = context.ReadValue("session", record);
		context.EnableTypeDiscriminator(prev);
		if (!ok)
		{
			gm.ReportResumeFailure_S("session record unreadable");
			return true;
		}

		gm.SetResumeRecord_S(record);

		foreach (LL_SlotRecord slot : record.slots)
		{
			if (slot.body.IsNull())
			{
				gm.ReportResumeFailure_S(string.Format("slot %1 has no persistence id", slot.name));
				continue;
			}

			// No wait limit: the finaliser's deadline bounds the whole resume.
			PersistenceWhenAvailableTask task(OnBodyAvailable, slot);
			GetSystem().WhenAvailable(slot.body, task);
		}

		return true;
	}

	protected static void OnBodyAvailable(Managed instance, PersistenceDeferredDeserializeTask task, bool expired, Managed context)
	{
		LL_SlotRecord record = LL_SlotRecord.Cast(context);
		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		if (!record || !gm)
			return;

		IEntity body = IEntity.Cast(instance);
		if (expired || !body)
		{
			gm.ReportResumeFailure_S(string.Format("slot %1 body never became available", record.name));
			return;
		}

		LL_PlayableComponent playable = LL_PlayableComponent.Cast(body.FindComponent(LL_PlayableComponent));
		if (!playable)
		{
			gm.ReportResumeFailure_S(string.Format("slot %1 body has no playable component", record.name));
			return;
		}

		Print(string.Format("[LL_Lobby] Resume: slot %1 body available at %2", record.name, body.GetOrigin().ToString()), LogLevel.NORMAL);
		playable.BeginResume(record.squad, record.sortKey, record.kia);
	}
}
