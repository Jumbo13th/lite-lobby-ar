// Server-side recorder for unit statistics. Recording costs zero replication: events
// append in memory and the only traffic is the chunked publish and one season fetch per
// session. GAME starts recording, DEBRIEFING finalises it, a Game Master publish stamps
// the result. Keyed by identity GUID (player ids change on reconnect); unit tags come
// from the player-verification response. Snapshots land in $profile:LL_GameStats/;
// $profile:LL_GameStats.json holds the weights and the Enabled switch.

// Units-endpoint response slice; JsonApiStruct binds by member name.
class LL_StatsWebUnit : JsonApiStruct
{
	string name;
	string tag;

	void LL_StatsWebUnit()
	{
		RegV("name");
		RegV("tag");
	}
}

class LL_StatsWebUnitsResponse : JsonApiStruct
{
	ref array<ref LL_StatsWebUnit> units = {};

	void LL_StatsWebUnitsResponse()
	{
		RegV("units");
	}
}

class LL_StatsManagerClass : SCR_BaseGameModeComponentClass
{
}

class LL_StatsManager : SCR_BaseGameModeComponent
{
	protected static const string CONFIG_PATH = "$profile:LL_GameStats.json";
	protected static const string OUTPUT_DIR = "$profile:LL_GameStats";

	protected static LL_StatsManager s_Instance;

	protected ref LL_StatsConfig m_Config;
	protected bool m_bActive;
	protected bool m_bRecording;
	protected bool m_bFinalized;
	protected bool m_bEndResolved;
	protected bool m_bAssignHooked;

	protected string m_sSessionId;
	protected string m_sMissionName;
	protected string m_sWorld;
	protected string m_sStartedAt;
	protected string m_sEndedAt;
	protected int m_iGameStartTick;

	// Both maps hold the same objects: m_mByGuid is the truth, m_mByPlayerId a session
	// index into it, kept across disconnects because the survival sweep resolves holders
	// who already left.
	protected ref map<string, ref LL_StatsPlayer> m_mByGuid = new map<string, ref LL_StatsPlayer>();
	protected ref map<int, ref LL_StatsPlayer> m_mByPlayerId = new map<int, ref LL_StatsPlayer>();

	protected ref array<ref LL_StatsEvent> m_aEvents = {};

	// Keyed by the trigger's stat key (a plain string; object-keyed maps are avoided).
	protected ref map<string, ref LL_StatsZoneObjective> m_mZones = new map<string, ref LL_StatsZoneObjective>();

	protected string m_sWinner;
	protected ref array<ref LL_StatsCommander> m_aCommanders = {};

	// Advisory prefill for the commander pickers: faction → first slot holder, frozen at
	// GAME start. The player is frozen, not their unit tag; verification may still be
	// in flight.
	protected ref map<string, int> m_mFirstSlotHolders = new map<string, int>();
	protected bool m_bSuggestionFrozen;

	// Strong refs: the engine drops unreferenced RestCallbacks before the response arrives.
	protected ref RestCallback m_UnitsCallback;
	protected ref RestCallback m_SeasonCallback;
	protected string m_sWebsiteUnitsJson;
	protected int m_iUnitsNotifyPlayerId = -1;

	static LL_StatsManager GetInstance()
	{
		return s_Instance;
	}

	void ~LL_StatsManager()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		s_Instance = this;

		LoadConfig();
		if (!m_Config.Enabled)
		{
			Print("[LL_Lobby] Stats: disabled in " + CONFIG_PATH, LogLevel.NORMAL);
			return;
		}

		m_bActive = true;
		MintSessionIdentity();

		// Season standings + units roster fetch, once per session.
		// Delayed so the verification component, whose website config this rides, has
		// finished its own init. -1 = background fill, notify nobody.
		GetGame().GetCallqueue().CallLater(StartSeasonFetch, 10000, false);
		GetGame().GetCallqueue().CallLater(StartUnitsFetch, 12000, false, -1);

		Print(string.Format("[LL_Lobby] Stats: active — session %1 (autosave %2s)", m_sSessionId, m_Config.AutosaveSeconds), LogLevel.NORMAL);
	}

	// A missing file gets a template; the default is enabled because stats need no
	// external service.
	protected void LoadConfig()
	{
		m_Config = new LL_StatsConfig();

		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(CONFIG_PATH))
		{
			CreateTemplateConfig();
			return;
		}

		if (!ctx.ReadValue("", m_Config))
		{
			Print("[LL_Lobby] Stats: " + CONFIG_PATH + " is malformed — using defaults", LogLevel.ERROR);
			m_Config = new LL_StatsConfig();
			return;
		}

		if (m_Config.AutosaveSeconds < 10)
			m_Config.AutosaveSeconds = 10;
	}

	protected void CreateTemplateConfig()
	{
		FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.WRITE);
		if (!file)
			return;

		file.WriteLine("{");
		file.WriteLine("	\"_readme\": \"Lite Lobby unit statistics. Scoring weights use a legible 1-point scale (players can re-derive the score from the visible counts). Snapshots land in $profile:LL_GameStats/, one file family per server session: -live (autosave), -final (game end), -approved (GM publish). Set Enabled=false to turn the feature off.\",");
		file.WriteLine("	\"Enabled\": true,");
		file.WriteLine("	\"FragPoints\": 1,");
		file.WriteLine("	\"ZoneFragMultiplier\": 2,");
		file.WriteLine("	\"AiKillWeight\": 0,");
		file.WriteLine("	\"TeamkillPoints\": -2,");
		file.WriteLine("	\"SurvivorPoints\": 1,");
		file.WriteLine("	\"SideWinMultiplier\": 1.25,");
		file.WriteLine("	\"CommanderWinMultiplier\": 1.5,");
		file.WriteLine("	\"AutosaveSeconds\": 60");
		file.WriteLine("}");
		file.Close();

		Print("[LL_Lobby] Stats: wrote default config to " + CONFIG_PATH, LogLevel.NORMAL);
	}

	LL_StatsConfig GetConfig()
	{
		return m_Config;
	}

	bool IsRecording()
	{
		return m_bRecording;
	}

	// The only game-side source of unit attribution; the latest response wins.
	void RegisterVerifiedIdentity(int playerId, string callsign, string unitTag, string unitName)
	{
		if (!m_bActive)
			return;

		LL_StatsPlayer entry = FindOrCreatePlayer(playerId);
		entry.callsign = callsign;
		entry.unitTag = unitTag;
		entry.unitName = unitName;
	}

	protected LL_StatsPlayer FindOrCreatePlayer(int playerId)
	{
		LL_StatsPlayer entry;
		if (m_mByPlayerId.Find(playerId, entry))
			return entry;

		string guid = ResolveGuid(playerId);
		if (m_mByGuid.Find(guid, entry))
		{
			// Same key while its holder is still connected = two live players sharing an
			// engine name on a no-backend server, not a reconnect.
			if (!IsHeldByConnectedOther(entry, playerId))
			{
				m_mByPlayerId.Set(playerId, entry);
				return entry;
			}
			guid = string.Format("%1#%2", guid, playerId);
		}

		entry = new LL_StatsPlayer();
		entry.guid = guid;
		entry.name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		m_mByGuid.Set(guid, entry);
		m_mByPlayerId.Set(playerId, entry);
		return entry;
	}

	protected bool IsHeldByConnectedOther(notnull LL_StatsPlayer entry, int playerId)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		foreach (int pid, LL_StatsPlayer held : m_mByPlayerId)
		{
			if (pid != playerId && held == entry && pm.GetPlayerController(pid))
				return true;
		}
		return false;
	}

	protected string ResolveGuid(int playerId)
	{
		UUID uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (!uid.IsNull())
			return uid;

		// No backend identity: mirror the reconnect-key fallback.
		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (name == "")
		{
			// A pid the engine no longer names; a bare "name:" key would merge every such ghost.
			return string.Format("pid:%1", playerId);
		}

		return "name:" + name;
	}

	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		super.OnGameStateChanged(state);

		if (!m_bActive)
			return;

		if (state == SCR_EGameModeState.GAME)
			StartRecording_S();
		else if (state == SCR_EGameModeState.DEBRIEFING)
			FinalizeGame_S();
	}

	protected void StartRecording_S()
	{
		if (m_bRecording || m_bFinalized)
			return;

		m_bRecording = true;
		m_iGameStartTick = System.GetTickCount();
		m_sStartedAt = NowStamp();

		SweepParticipants_S();
		FreezeCommanderSuggestion_S();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr && !m_bAssignHooked)
		{
			m_bAssignHooked = true;
			mgr.GetOnPlayerAssigned().Insert(OnPlayerAssignedToSlot);
		}

		GetGame().GetCallqueue().CallLater(Autosave, m_Config.AutosaveSeconds * 1000, true);
		Autosave();

		Print("[LL_Lobby] Stats: recording started", LogLevel.NORMAL);
	}

	// Everyone holding a slot at GAME start is a participant (occupancy metric, not a score input).
	protected void SweepParticipants_S()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.m_iPlayerId < 0)
				continue;
			MarkParticipant(slot.m_iPlayerId, slot.m_sFactionKey);
		}
	}

	// The unit that took a side's top slot commands it. Captured once at GAME start,
	// because slot occupancy keeps moving afterwards.
	protected void FreezeCommanderSuggestion_S()
	{
		CollectFirstSlotHolders(m_mFirstSlotHolders);
		m_bSuggestionFrozen = true;
	}

	// First occupied slot per faction in lobby display order.
	protected void CollectFirstSlotHolders(notnull map<string, int> outHolders)
	{
		outHolders.Clear();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.m_iPlayerId < 0 || slot.m_sFactionKey == "")
				continue;
			if (outHolders.Contains(slot.m_sFactionKey))
				continue;

			outHolders.Set(slot.m_sFactionKey, slot.m_iPlayerId);
		}
	}

	// As "faction<TAB>tag" lines. Not re-derived from the view: a commanding unit that
	// fielded nobody has no computed row and would read back as "no commander".
	string GetCommandersEncoded()
	{
		return EncodeCommanders(m_aCommanders);
	}

	// Prefill as "faction<TAB>tag" lines; before GAME the slots are read live. A holder
	// with no known unit yields no line.
	string BuildSuggestedCommandersEncoded()
	{
		map<string, int> holders = m_mFirstSlotHolders;
		if (!m_bSuggestionFrozen)
		{
			holders = new map<string, int>();
			CollectFirstSlotHolders(holders);
		}

		array<ref LL_StatsCommander> suggested = {};
		foreach (string factionKey, int playerId : holders)
		{
			// Non-creating lookup: a panel read must not invent identities in the snapshot.
			LL_StatsPlayer entry;
			if (!m_mByPlayerId.Find(playerId, entry) || entry.unitTag == "")
				continue;

			LL_StatsCommander cmd = new LL_StatsCommander();
			cmd.faction = factionKey;
			cmd.unitTag = entry.unitTag;
			suggested.Insert(cmd);
		}

		return EncodeCommanders(suggested);
	}

	protected void OnPlayerAssignedToSlot(int playerId, int slotRplId)
	{
		if (!m_bRecording)
			return;

		string faction = "";
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			LL_SlotData slot = mgr.FindSlotByRplId(slotRplId);
			if (slot)
				faction = slot.m_sFactionKey;
		}
		MarkParticipant(playerId, faction);
	}

	// The slot's faction wins: an admin-driven mid-game side switch moves the player's
	// whole line, since attribution is per player-faction.
	protected void MarkParticipant(int playerId, string factionKey)
	{
		LL_StatsPlayer entry = FindOrCreatePlayer(playerId);
		entry.participated = true;
		if (factionKey != "" && entry.faction != factionKey)
			entry.faction = factionKey;
	}

	protected void FinalizeGame_S()
	{
		if (!m_bRecording || m_bFinalized)
			return;

		m_bRecording = false;
		m_bFinalized = true;
		m_sEndedAt = NowStamp();

		GetGame().GetCallqueue().Remove(Autosave);

		ResolveEndOfGame_S();

		WriteSnapshot("final");
		Print("[LL_Lobby] Stats: game finalized — " + SnapshotPath("final"), LogLevel.NORMAL);
	}

	// Survival is judged once, at the first of publish or DEBRIEFING. Zones re-resolve on
	// every publish: a mid-game publish awards an uncaptured zone to the defenders only
	// tentatively, and a later capture overrides it.
	protected void ResolveEndOfGame_S()
	{
		if (!m_bEndResolved)
		{
			m_bEndResolved = true;
			SweepSurvivors_S();
		}

		foreach (string key, LL_StatsZoneObjective job : m_mZones)
		{
			// Contested zones re-resolve against the current holder on every publish.
			if (job.contested)
			{
				// An empty holder means neutral: nobody is paid, and an earlier award is cleared.
				ResolveZoneObjective(job, job.holderFaction, LL_StatsEventType.ZONE_FLIP);
			}
			else if (!job.captured)
			{
				ResolveZoneObjective(job, job.defenderFaction, LL_StatsEventType.DEFENSE);
			}
		}
	}

	// Every slot whose body is alive at game end credits its holder, disconnected-but-
	// reserved holders included.
	protected void SweepSurvivors_S()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.m_iPlayerId < 0)
				continue;

			IEntity body = mgr.ResolveSlotEntity(slot.m_iRplId);
			if (!LL_TriggerComponent.IsCharacterAlive(body))
				continue;

			LL_StatsPlayer entry = FindOrCreatePlayer(slot.m_iPlayerId);
			LL_StatsEvent ev = AddEvent(LL_StatsEventType.SURVIVOR, entry.guid, "");
			ev.points = m_Config.SurvivorPoints;
		}
	}

	protected void Autosave()
	{
		if (!m_bRecording)
			return;
		WriteSnapshot("live");
	}

	override void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnControllableDestroyed(instigatorContextData);

		if (!m_bActive || !m_bRecording)
			return;

		// Deleted bodies are lifecycle cleanup, not deaths.
		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.DELETED | SCR_ECharacterDeathStatusRelations.DELETED_BY_EDITOR))
			return;

		int victimPid = instigatorContextData.GetVictimPlayerID();
		int killerPid = instigatorContextData.GetKillerPlayerID();
		bool victimIsPlayer = victimPid > 0;
		bool killerIsPlayer = killerPid > 0;

		string victimGuid = "";
		if (victimIsPlayer)
		{
			LL_StatsPlayer victim = FindOrCreatePlayer(victimPid);
			if (victim.faction == "")
				victim.faction = LL_TriggerComponent.GetEntityFactionKey(instigatorContextData.GetVictimEntity());
			victimGuid = victim.guid;
		}

		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER) && killerIsPlayer)
		{
			LL_StatsPlayer killer = FindOrCreatePlayer(killerPid);
			if (killer.faction == "")
				killer.faction = LL_TriggerComponent.GetEntityFactionKey(instigatorContextData.GetKillerEntity());

			if (victimIsPlayer)
			{
				// The multiplier judges where the victim fell.
				string zoneName;
				float mult = 0;
				IEntity victimEnt = instigatorContextData.GetVictimEntity();
				if (victimEnt)
					mult = LL_StatZoneComponent.GetZoneMultiplierAt(victimEnt.GetOrigin(), m_Config.ZoneFragMultiplier, zoneName);

				if (mult > 0)
				{
					LL_StatsEvent ev = AddEvent(LL_StatsEventType.ZONE_KILL, killer.guid, victimGuid);
					ev.source = zoneName;
					ev.points = m_Config.FragPoints * mult;
				}
				else
				{
					LL_StatsEvent ev = AddEvent(LL_StatsEventType.KILL, killer.guid, victimGuid);
					ev.points = m_Config.FragPoints;
				}
			}
			else
			{
				LL_StatsEvent ev = AddEvent(LL_StatsEventType.AI_KILL, killer.guid, "");
				ev.points = m_Config.FragPoints * m_Config.AiKillWeight;
			}
			return;
		}

		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER) && killerIsPlayer && victimIsPlayer)
		{
			LL_StatsPlayer killer = FindOrCreatePlayer(killerPid);
			LL_StatsEvent ev = AddEvent(LL_StatsEventType.TEAMKILL, killer.guid, victimGuid);
			ev.points = m_Config.TeamkillPoints;
			return;
		}

		// Suicide, AI, Game Master or environment: a plain loss, nobody gets credit.
		if (victimIsPlayer)
			AddEvent(LL_StatsEventType.DEATH, "", victimGuid);
	}

	// Personal killfeed, built on demand from the recorded events for the one player who
	// asked. The authoritative record can never disagree with the /stats report; a
	// client-side tally could, since a client only sees streamed characters.

	// Encoded as data, never a finished sentence: the client composes the headings in the
	// reader's language. One tagged line per present section so no field is ever empty:
	//   D|1  died;  K|name<tab>...  kills, newest first;  T|...  teamkills;  B|name  killer.
	// "" when there is nothing to show.
	string BuildPlayerKillfeedData(int playerId)
	{
		if (!m_bActive)
			return "";

		LL_StatsPlayer me;
		if (!m_mByPlayerId.Find(playerId, me))
			return "";

		array<string> kills = {};
		array<string> teamkills = {};
		string killedBy = "";
		bool died = false;

		// The events array is chronological and the panel reads newest first.
		for (int i = m_aEvents.Count() - 1; i >= 0; i--)
		{
			LL_StatsEvent ev = m_aEvents[i];

			if (ev.actor == me.guid && ev.victim != "")
			{
				if (ev.type == LL_StatsEventType.KILL || ev.type == LL_StatsEventType.ZONE_KILL)
					kills.Insert(KillfeedName(ev.victim));
				else if (ev.type == LL_StatsEventType.TEAMKILL)
					teamkills.Insert(KillfeedName(ev.victim));
			}

			// Most recent death only; a DEATH event names no actor, and that is the information.
			if (ev.victim == me.guid && !died)
			{
				died = true;
				if (ev.actor != "")
					killedBy = KillfeedName(ev.actor);
			}
		}

		if (kills.IsEmpty() && teamkills.IsEmpty() && !died)
			return "";

		array<string> lines = {};
		if (died)
			lines.Insert("D|1");
		if (!kills.IsEmpty())
			lines.Insert("K|" + Join(kills, "\t"));
		if (!teamkills.IsEmpty())
			lines.Insert("T|" + Join(teamkills, "\t"));
		if (killedBy != "")
			lines.Insert("B|" + killedBy);

		return Join(lines, "\n");
	}

	protected static string Join(notnull array<string> items, string separator)
	{
		string joined = "";
		foreach (int i, string item : items)
		{
			if (i > 0)
				joined += separator;
			joined += item;
		}
		return joined;
	}

	// "[TAG]Callsign" when the website knows them, else callsign, else engine name. Tabs
	// and newlines are this payload's separators and are stripped.
	protected string KillfeedName(string guid)
	{
		LL_StatsPlayer entry;
		if (!m_mByGuid.Find(guid, entry))
			return "?";

		string name = entry.callsign;
		if (name == "")
			name = entry.name;
		if (name == "")
			return "?";

		name.Replace("\t", " ");
		name.Replace("\n", " ");

		if (entry.unitTag != "")
			return string.Format("[%1]%2", entry.unitTag, name);
		return name;
	}

	// Timeline entry for triggers with no unit attribution of their own.
	void NotifyTriggerFired(notnull LL_TriggerComponent trigger, string label)
	{
		if (!m_bActive || !m_bRecording)
			return;

		LL_StatsEvent ev = AddEvent(LL_StatsEventType.TRIGGER, "", "");
		ev.source = trigger.GetStatKey();
		ev.detail = label;
	}

	// 1 Hz presence from the zone triggers, both sides. Dying keeps the seconds earned.
	void RecordZonePresence(notnull LL_TriggerZoneBase trigger, notnull array<IEntity> insideChars, float dtSeconds)
	{
		if (!m_bActive || !m_bRecording)
			return;
		if (trigger.GetStatPoints() <= 0)
			return;

		LL_StatsZoneObjective job = FindOrCreateZoneJob(trigger);
		// Presence accrues until the zone is captured; a tentative defense keeps collecting.
		if (job.captured)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		foreach (IEntity ent : insideChars)
		{
			int pid = pm.GetPlayerIdFromControlledEntity(ent);
			if (pid <= 0)
				continue;

			LL_StatsPlayer entry = FindOrCreatePlayer(pid);

			LL_StatsPresence found;
			foreach (LL_StatsPresence p : job.presence)
			{
				if (p.guid == entry.guid)
				{
					found = p;
					break;
				}
			}
			if (!found)
			{
				found = new LL_StatsPresence();
				found.guid = entry.guid;
				job.presence.Insert(found);
			}
			found.seconds += dtSeconds;
		}
	}

	void NotifyZoneCaptured(notnull LL_TriggerZoneCapture trigger)
	{
		if (!m_bActive || !m_bRecording)
			return;
		if (trigger.GetStatPoints() <= 0)
			return;

		LL_StatsZoneObjective job = FindOrCreateZoneJob(trigger);
		job.holderFaction = job.attackerFaction;
		// A capture overrides a tentative defense award; only a captured zone is final.
		if (!job.captured)
			ResolveZoneObjective(job, job.attackerFaction, LL_StatsEventType.CAPTURE);
	}

	// A change of hands is timeline only; the pool settles at publish against the holder.
	// Recorded even without a point pool.
	void NotifyZoneOwnerChanged(notnull LL_TriggerZoneBase trigger, FactionKey newOwner)
	{
		if (!m_bActive || !m_bRecording)
			return;

		LL_StatsZoneObjective job = FindOrCreateZoneJob(trigger);
		job.holderFaction = newOwner;
		job.flips++;

		LL_StatsEvent ev = AddEvent(LL_StatsEventType.ZONE_FLIP, "", "");
		ev.source = job.entityName;
		ev.detail = job.name;
		ev.faction = newOwner;
	}

	protected LL_StatsZoneObjective FindOrCreateZoneJob(notnull LL_TriggerZoneBase trigger)
	{
		string key = trigger.GetStatKey();

		LL_StatsZoneObjective job;
		if (m_mZones.Find(key, job))
			return job;

		job = new LL_StatsZoneObjective();
		job.name = trigger.GetZoneStatLabel();
		job.entityName = key;
		job.pool = trigger.GetStatPoints();
		job.maxPerPlayer = trigger.GetStatMaxPerPlayer();
		job.attackerFaction = trigger.GetAttackerFaction();
		job.defenderFaction = trigger.GetDefenderFaction();
		job.contested = trigger.IsContestedZone();
		job.holderFaction = trigger.GetZoneHolderFaction();
		m_mZones.Set(key, job);
		return job;
	}

	// The side a zone's pool belongs to as things stand; one place so resolution and the
	// computed rows cannot disagree.
	protected string ZoneWinFaction(notnull LL_StatsZoneObjective job)
	{
		if (job.awardedFaction != "")
			return job.awardedFaction;
		if (job.contested)
			return job.holderFaction;
		if (job.captured)
			return job.attackerFaction;
		return job.defenderFaction;
	}

	// Splits a zone pool among the winning side's units by presence-seconds, clamped per
	// unit to cap × distinct contributors; excess is discarded. Re-entrant: previous
	// awards are dropped first.
	protected void ResolveZoneObjective(notnull LL_StatsZoneObjective job, string winFaction, string eventType)
	{
		job.resolved = true;
		job.awardedFaction = winFaction;
		// `captured` is the sealed flag; a contested zone is never sealed or the minutes
		// before a publish would stop counting.
		if (!job.contested)
			job.captured = (eventType == LL_StatsEventType.CAPTURE);
		job.awards.Clear();

		// Neutral zone: the presence filter matches on faction, and an unslotted player
		// also carries an empty one.
		if (winFaction == "")
			return;

		map<string, ref LL_StatsUnitAward> byUnit = new map<string, ref LL_StatsUnitAward>();
		float totalSeconds = 0;

		foreach (LL_StatsPresence p : job.presence)
		{
			LL_StatsPlayer entry;
			if (!m_mByGuid.Find(p.guid, entry))
				continue;
			if (entry.unitTag == "" || entry.faction != winFaction)
				continue;

			LL_StatsUnitAward award;
			if (!byUnit.Find(entry.unitTag, award))
			{
				award = new LL_StatsUnitAward();
				award.unitTag = entry.unitTag;
				byUnit.Set(entry.unitTag, award);
			}
			award.seconds += p.seconds;
			award.contributors++;
			totalSeconds += p.seconds;
		}

		if (totalSeconds > 0)
		{
			foreach (string tag, LL_StatsUnitAward award : byUnit)
			{
				award.points = job.pool * award.seconds / totalSeconds;
				if (job.maxPerPlayer > 0)
					award.points = Math.Min(award.points, job.maxPerPlayer * award.contributors);
				job.awards.Insert(award);
			}
		}

		// A contested zone already narrated every change of hands.
		if (job.contested)
			return;

		// Announce each outcome once: a re-publish must not stack "Held" lines.
		if (job.announcedType != eventType)
		{
			job.announcedType = eventType;
			LL_StatsEvent ev = AddEvent(eventType, "", "");
			ev.source = job.entityName;
			ev.detail = job.name;
			ev.points = job.pool;
		}
	}

	// rawShare = pool / target count; the per-player cap is applied at compute time so the
	// website can re-clamp with its own unit mapping.
	void RecordKeyTargetDestroyed(notnull LL_TriggerComponent trigger, string targetLabel, int instigatorPlayerId, float rawShare)
	{
		if (!m_bActive || !m_bRecording)
			return;

		string actor = "";
		if (instigatorPlayerId > 0)
			actor = FindOrCreatePlayer(instigatorPlayerId).guid;

		LL_StatsEvent ev = AddEvent(LL_StatsEventType.KEY_TARGET, actor, "");
		ev.source = trigger.GetStatKey();
		ev.detail = targetLabel;
		ev.cap = trigger.GetStatMaxPerPlayer();
		if (actor != "")
			ev.points = rawShare;
	}

	// A player killed a member of a defeat-group objective: their unit earns the member's
	// share. Scoring only; the timeline gets one entry when the whole group falls.
	void RecordGroupMemberKill(notnull LL_TriggerComponent trigger, int killerPlayerId, float share, string groupFactionKey)
	{
		if (!m_bActive || !m_bRecording)
			return;
		if (killerPlayerId <= 0 || share <= 0)
			return;

		LL_StatsPlayer killer = FindOrCreatePlayer(killerPlayerId);

		// The group's own side earns nothing, or its defenders could farm the pool via AI
		// teamkills.
		if (groupFactionKey != "" && killer.faction == groupFactionKey)
			return;

		LL_StatsEvent ev = AddEvent(LL_StatsEventType.GROUP_KILL, killer.guid, "");
		ev.source = trigger.GetStatKey();
		ev.cap = trigger.GetStatMaxPerPlayer();
		ev.points = share;
	}

	void SetResult_S(string winnerFactionKey, notnull array<ref LL_StatsCommander> commanders)
	{
		if (!m_bActive)
			return;

		m_sWinner = winnerFactionKey;
		m_aCommanders = commanders;
	}

	void WriteApproved_S()
	{
		if (!m_bActive)
			return;

		// Publish resolves end-of-game rewards once; recording continues until DEBRIEFING.
		ResolveEndOfGame_S();

		WriteSnapshot("approved");
		Print("[LL_Lobby] Stats: approved snapshot written — " + SnapshotPath("approved"), LogLevel.NORMAL);
	}

	// Game Master publish: stamp the result, write the approved file, broadcast the view.
	// Re-publishable.
	void PublishFromPanel_S(string winnerFactionKey, string commandersEncoded)
	{
		if (!m_bActive)
			return;

		if (!m_bRecording && !m_bFinalized)
			return;

		array<ref LL_StatsCommander> commanders = {};
		DecodeCommanders(commandersEncoded, commanders);
		SetResult_S(winnerFactionKey, commanders);
		WriteApproved_S();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.PublishStatsView_S(BuildViewJson());
	}

	// Website fetches ride the player-verification config: one operator config, one REST
	// context. Not configured: the panel falls back to units seen in this game.

	void StartSeasonFetch()
	{
		if (!m_bActive || m_SeasonCallback)
			return;

		LL_PlayerVerificationComponent verify = LL_PlayerVerificationComponent.GetInstance();
		if (!verify)
			return;

		// Clients cannot read the server's config file, so the URL rides the lobby sync.
		LL_LobbyManager lobbyMgr = LL_LobbyManager.GetInstance();
		if (lobbyMgr)
			lobbyMgr.SetSiteUrl_S(verify.GetSiteDisplayUrl());

		if (verify.IsMockActive())
		{
			GetGame().GetCallqueue().CallLater(FinishMockSeasonFetch, LL_WebsiteMock.GetLatencyMs(), false);
			return;
		}

		RestContext restCtx = verify.GetWebsiteRestContext();
		string request = verify.BuildSeasonRequest();
		if (!restCtx || request == "")
			return;

		m_SeasonCallback = new RestCallback();
		m_SeasonCallback.SetOnSuccess(OnSeasonFetchDone);
		m_SeasonCallback.SetOnError(OnSeasonFetchDone);
		restCtx.GET(m_SeasonCallback, request);
	}

	protected void FinishMockSeasonFetch()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.SetSeasonStandings_S(LL_WebsiteMock.BuildSeasonJson());
		Print("[LL_Lobby] Stats: season standings served by the WEBSITE MOCK", LogLevel.NORMAL);
	}

	protected void OnSeasonFetchDone(RestCallback cb)
	{
		m_SeasonCallback = null;

		if (cb.GetHttpCode() != 200)
		{
			Print(string.Format("[LL_Lobby] Stats: season fetch failed (HTTP %1) — Season tab stays unavailable", cb.GetHttpCode()), LogLevel.WARNING);
			return;
		}

		string body = cb.GetData();
		// A rogue site must not flood clients.
		if (body == "" || body.Length() > 32000)
		{
			Print("[LL_Lobby] Stats: season response empty or oversized — ignored", LogLevel.WARNING);
			return;
		}

		// A misrouted 200 (proxy error page, login screen) must parse as a season response.
		LL_StatsSeasonResponse probe = new LL_StatsSeasonResponse();
		probe.ExpandFromRAW(body);
		if (probe.season.name == "" && probe.standings.IsEmpty())
		{
			string head = body;
			if (head.Length() > 120)
				head = head.Substring(0, 120);
			Print("[LL_Lobby] Stats: season response did not parse (first bytes: " + head + ") — ignored", LogLevel.WARNING);
			return;
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.SetSeasonStandings_S(body);

		Print("[LL_Lobby] Stats: season standings received and broadcast", LogLevel.NORMAL);
	}

	// notifyPlayerId: the admin whose panel asked, re-sent when the list arrives; -1 = nobody.
	void StartUnitsFetch(int notifyPlayerId)
	{
		if (!m_bActive)
			return;

		if (notifyPlayerId > 0)
			m_iUnitsNotifyPlayerId = notifyPlayerId;

		if (m_UnitsCallback)
			return;

		LL_PlayerVerificationComponent verify = LL_PlayerVerificationComponent.GetInstance();
		if (!verify)
		{
			NotifyUnitsRequester();
			return;
		}

		if (verify.IsMockActive())
		{
			GetGame().GetCallqueue().CallLater(FinishMockUnitsFetch, LL_WebsiteMock.GetLatencyMs(), false);
			return;
		}

		RestContext restCtx = verify.GetWebsiteRestContext();
		string request = verify.BuildUnitsRequest();
		if (!restCtx || request == "")
		{
			NotifyUnitsRequester();
			return;
		}

		m_UnitsCallback = new RestCallback();
		m_UnitsCallback.SetOnSuccess(OnUnitsFetchDone);
		m_UnitsCallback.SetOnError(OnUnitsFetchDone);
		restCtx.GET(m_UnitsCallback, request);
	}

	protected void FinishMockUnitsFetch()
	{
		m_sWebsiteUnitsJson = LL_WebsiteMock.BuildUnitsJson();
		NotifyUnitsRequester();
	}

	protected void OnUnitsFetchDone(RestCallback cb)
	{
		m_UnitsCallback = null;

		if (cb.GetHttpCode() == 200)
		{
			string body = cb.GetData();
			if (body != "" && body.Length() <= 64000)
				m_sWebsiteUnitsJson = body;
		}
		else
		{
			Print(string.Format("[LL_Lobby] Stats: units fetch failed (HTTP %1) — panel falls back to units seen in game", cb.GetHttpCode()), LogLevel.WARNING);
		}

		NotifyUnitsRequester();
	}

	protected void NotifyUnitsRequester()
	{
		int playerId = m_iUnitsNotifyPlayerId;
		m_iUnitsNotifyPlayerId = -1;
		if (playerId <= 0)
			return;

		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!pc)
			return;

		LL_LobbyPlayerComponent comp = LL_LobbyPlayerComponent.Cast(pc.FindComponent(LL_LobbyPlayerComponent));
		if (comp)
			comp.SendStatsPanelData_S();
	}

	// Website roster when the fetch succeeded, plus every unit tag seen in this game the
	// site did not list.
	string BuildUnitsOptionsJson()
	{
		array<string> tags = {};
		array<string> names = {};

		if (m_sWebsiteUnitsJson != "")
		{
			LL_StatsWebUnitsResponse web = new LL_StatsWebUnitsResponse();
			web.ExpandFromRAW(m_sWebsiteUnitsJson);
			foreach (LL_StatsWebUnit unit : web.units)
				AddUnitOption(tags, names, unit.tag.Trim(), unit.name.Trim());
		}

		foreach (string guid, LL_StatsPlayer entry : m_mByGuid)
			AddUnitOption(tags, names, entry.unitTag, entry.unitName);

		string json = "{\"units\":[";
		for (int i = 0; i < tags.Count(); i++)
		{
			if (i > 0)
				json += ",";
			json += string.Format("{\"tag\":\"%1\",\"name\":\"%2\"}", JsonEscape(tags[i]), JsonEscape(names[i]));
		}
		json += "]}";
		return json;
	}

	protected static void AddUnitOption(notnull array<string> tags, notnull array<string> names, string tag, string name)
	{
		if (tag == "")
			return;

		string lowerTag = tag;
		lowerTag.ToLower();
		foreach (string existing : tags)
		{
			string lowerExisting = existing;
			lowerExisting.ToLower();
			if (lowerExisting == lowerTag)
				return;
		}

		tags.Insert(tag);
		names.Insert(name);
	}

	array<ref LL_StatsUnitRow> Compute()
	{
		map<string, ref LL_StatsUnitRow> rows = new map<string, ref LL_StatsUnitRow>();
		array<ref LL_StatsUnitRow> ordered = {};

		// Key-target clamps: raw sums and distinct actors per (trigger, unit), folded after
		// the event pass.
		map<string, float> ktRaw = new map<string, float>();
		map<string, float> ktCap = new map<string, float>();
		map<string, ref array<string>> ktActors = new map<string, ref array<string>>();
		map<string, ref LL_StatsUnitRow> ktRow = new map<string, ref LL_StatsUnitRow>();

		foreach (LL_StatsEvent ev : m_aEvents)
		{
			LL_StatsPlayer actor;
			if (ev.actor != "")
				m_mByGuid.Find(ev.actor, actor);

			LL_StatsPlayer victim;
			if (ev.victim != "")
				m_mByGuid.Find(ev.victim, victim);

			if (victim && victim.unitTag != "")
				RowFor(rows, ordered, victim).deaths++;

			if (!actor || actor.unitTag == "")
				continue;

			LL_StatsUnitRow row = RowFor(rows, ordered, actor);

			if (ev.type == LL_StatsEventType.KILL)
			{
				row.kills++;
				row.basePoints += ev.points;
			}
			else if (ev.type == LL_StatsEventType.ZONE_KILL)
			{
				row.kills++;
				row.zoneKills++;
				row.basePoints += ev.points;
			}
			else if (ev.type == LL_StatsEventType.AI_KILL)
			{
				row.aiKills++;
				row.basePoints += ev.points;
			}
			else if (ev.type == LL_StatsEventType.TEAMKILL)
			{
				row.teamkills++;
				row.basePoints += ev.points;
			}
			else if (ev.type == LL_StatsEventType.SURVIVOR)
			{
				row.survivors++;
				row.basePoints += ev.points;
			}
			else if (ev.type == LL_StatsEventType.KEY_TARGET || ev.type == LL_StatsEventType.GROUP_KILL)
			{
				string k = ev.source + "|" + actor.unitTag;
				float raw;
				ktRaw.Find(k, raw);
				ktRaw.Set(k, raw + ev.points);
				ktCap.Set(k, ev.cap);
				ktRow.Set(k, row);

				array<string> actors;
				if (!ktActors.Find(k, actors))
				{
					actors = new array<string>();
					ktActors.Set(k, actors);
				}
				if (!actors.Contains(ev.actor))
					actors.Insert(ev.actor);
			}
		}

		foreach (string k, float raw : ktRaw)
		{
			float cap;
			ktCap.Find(k, cap);
			array<string> actors;
			ktActors.Find(k, actors);

			float awarded = raw;
			if (cap > 0 && actors)
				awarded = Math.Min(awarded, cap * actors.Count());

			LL_StatsUnitRow row;
			if (ktRow.Find(k, row))
				row.objectivePoints += awarded;
		}

		foreach (string key, LL_StatsZoneObjective job : m_mZones)
		{
			string winFaction = ZoneWinFaction(job);

			foreach (LL_StatsUnitAward award : job.awards)
			{
				LL_StatsUnitRow row = RowForTag(rows, ordered, award.unitTag, winFaction);
				row.objectivePoints += award.points;
			}
		}

		foreach (string guid, LL_StatsPlayer entry : m_mByGuid)
		{
			if (!entry.participated || entry.unitTag == "")
				continue;
			RowFor(rows, ordered, entry).participants++;
		}

		bool winnerDeclared = m_sWinner != "" && m_sWinner != "draw";
		foreach (LL_StatsUnitRow row : ordered)
		{
			foreach (LL_StatsCommander cmd : m_aCommanders)
			{
				if (cmd.unitTag == row.unitTag && cmd.faction == row.faction)
					row.isCommander = true;
			}

			float raw = row.basePoints + row.objectivePoints;

			// Multipliers only reward positive work; a teamkill-negative total is not deepened.
			if (winnerDeclared && row.faction == m_sWinner)
			{
				row.isWinnerSide = true;
				if (raw > 0)
				{
					row.multiplier = m_Config.SideWinMultiplier;
					if (row.isCommander)
						row.multiplier = m_Config.CommanderWinMultiplier;
				}
			}

			// A game never digs into the season total; the website mirrors both rules.
			row.finalPoints = Math.Max(0, raw * row.multiplier);
		}

		return ordered;
	}

	protected LL_StatsUnitRow RowFor(notnull map<string, ref LL_StatsUnitRow> rows, notnull array<ref LL_StatsUnitRow> ordered, notnull LL_StatsPlayer entry)
	{
		LL_StatsUnitRow row = RowForTag(rows, ordered, entry.unitTag, entry.faction);
		if (row.unitName == "" && entry.unitName != "")
			row.unitName = entry.unitName;
		return row;
	}

	protected LL_StatsUnitRow RowForTag(notnull map<string, ref LL_StatsUnitRow> rows, notnull array<ref LL_StatsUnitRow> ordered, string unitTag, string faction)
	{
		string key = unitTag + "|" + faction;

		LL_StatsUnitRow row;
		if (rows.Find(key, row))
			return row;

		row = new LL_StatsUnitRow();
		row.unitTag = unitTag;
		row.faction = faction;
		rows.Set(key, row);
		ordered.Insert(row);
		return row;
	}

	LL_StatsSnapshot BuildSnapshot(string phase)
	{
		LL_StatsSnapshot snap = new LL_StatsSnapshot();
		snap.sessionId = m_sSessionId;
		snap.phase = phase;
		snap.missionName = m_sMissionName;
		snap.world = m_sWorld;
		snap.startedAt = m_sStartedAt;
		snap.endedAt = m_sEndedAt;
		snap.savedAt = NowStamp();
		snap.winner = m_sWinner;
		snap.config = m_Config;
		snap.commanders = m_aCommanders;
		snap.events = m_aEvents;

		foreach (string guid, LL_StatsPlayer entry : m_mByGuid)
		{
			snap.players.Insert(entry);
			if (entry.faction != "" && !snap.factions.Contains(entry.faction))
				snap.factions.Insert(entry.faction);
		}

		foreach (string key, LL_StatsZoneObjective job : m_mZones)
		{
			snap.zones.Insert(job);
			if (job.attackerFaction != "" && !snap.factions.Contains(job.attackerFaction))
				snap.factions.Insert(job.attackerFaction);
			if (job.defenderFaction != "" && !snap.factions.Contains(job.defenderFaction))
				snap.factions.Insert(job.defenderFaction);
		}

		snap.computed = Compute();
		return snap;
	}

	// The client-facing payload: unit rows + side totals + timeline. No GUIDs.
	LL_StatsView BuildView()
	{
		LL_StatsView view = new LL_StatsView();
		view.sessionId = m_sSessionId;
		view.missionName = m_sMissionName;
		view.winner = m_sWinner;
		view.sideWinMultiplier = m_Config.SideWinMultiplier;
		view.commanderWinMultiplier = m_Config.CommanderWinMultiplier;

		array<ref LL_StatsUnitRow> computed = Compute();
		foreach (LL_StatsUnitRow src : computed)
			view.rows.Insert(ViewRowFrom(src));

		map<string, ref LL_StatsViewSide> sides = new map<string, ref LL_StatsViewSide>();
		FactionManager fm = GetGame().GetFactionManager();

		// Every faction with slots gets a side, so an empty side is still selectable as winner.
		LL_LobbyManager lobbyMgr = LL_LobbyManager.GetInstance();
		if (lobbyMgr)
		{
			foreach (LL_SlotData slot : lobbyMgr.GetSlots())
				TouchViewSide(view, sides, fm, slot.m_sFactionKey);
		}

		foreach (LL_StatsViewRow row : view.rows)
		{
			LL_StatsViewSide side = TouchViewSide(view, sides, fm, row.faction);
			if (!side)
				continue;
			side.totalPoints += row.finalPoints;
			if (row.isCommander)
				side.commanderTag = row.unitTag;
		}

		foreach (LL_StatsEvent ev : m_aEvents)
		{
			if (ev.type != LL_StatsEventType.CAPTURE && ev.type != LL_StatsEventType.DEFENSE
				&& ev.type != LL_StatsEventType.KEY_TARGET && ev.type != LL_StatsEventType.TRIGGER
				&& ev.type != LL_StatsEventType.ZONE_FLIP)
				continue;

			LL_StatsViewEvent tl = new LL_StatsViewEvent();
			tl.t = ev.t;
			tl.type = ev.type;
			tl.text = ev.detail;
			tl.faction = ev.faction;
			view.timeline.Insert(tl);
		}

		return view;
	}

	// Hand-built: JsonApiStruct auto-pack does not serialise registered object arrays;
	// ExpandFromRAW on the client does bind them.
	string BuildViewJson()
	{
		LL_StatsView view = BuildView();

		string json = "{";
		json += string.Format("\"schema\":\"%1\",\"sessionId\":\"%2\",\"missionName\":\"%3\",\"winner\":\"%4\",",
			JsonEscape(view.schema), JsonEscape(view.sessionId), JsonEscape(view.missionName), JsonEscape(view.winner));
		json += string.Format("\"sideWinMultiplier\":%1,\"commanderWinMultiplier\":%2,",
			FloatJson(view.sideWinMultiplier), FloatJson(view.commanderWinMultiplier));

		json += "\"sides\":[";
		for (int i = 0; i < view.sides.Count(); i++)
		{
			LL_StatsViewSide side = view.sides[i];
			if (i > 0)
				json += ",";
			json += string.Format("{\"faction\":\"%1\",\"displayName\":\"%2\",\"isWinner\":%3,\"commanderTag\":\"%4\",\"totalPoints\":%5}",
				JsonEscape(side.faction), JsonEscape(side.displayName), BoolJson(side.isWinner), JsonEscape(side.commanderTag), FloatJson(side.totalPoints));
		}
		json += "],";

		json += "\"rows\":[";
		for (int i = 0; i < view.rows.Count(); i++)
		{
			LL_StatsViewRow row = view.rows[i];
			if (i > 0)
				json += ",";
			json += string.Format("{\"unitTag\":\"%1\",\"unitName\":\"%2\",\"faction\":\"%3\",\"kills\":%4,\"zoneKills\":%5,\"aiKills\":%6,\"teamkills\":%7,\"deaths\":%8,",
				JsonEscape(row.unitTag), JsonEscape(row.unitName), JsonEscape(row.faction), row.kills, row.zoneKills, row.aiKills, row.teamkills, row.deaths);
			json += string.Format("\"survivors\":%1,\"participants\":%2,\"objectivePoints\":%3,\"basePoints\":%4,\"multiplier\":%5,\"finalPoints\":%6,\"isCommander\":%7,\"isWinnerSide\":%8}",
				row.survivors, row.participants, FloatJson(row.objectivePoints), FloatJson(row.basePoints), FloatJson(row.multiplier), FloatJson(row.finalPoints), BoolJson(row.isCommander), BoolJson(row.isWinnerSide));
		}
		json += "],";

		json += "\"timeline\":[";
		for (int i = 0; i < view.timeline.Count(); i++)
		{
			LL_StatsViewEvent ev = view.timeline[i];
			if (i > 0)
				json += ",";
			json += string.Format("{\"t\":%1,\"type\":\"%2\",\"text\":\"%3\",\"faction\":\"%4\"}",
				FloatJson(ev.t), JsonEscape(ev.type), JsonEscape(ev.text), JsonEscape(ev.faction));
		}
		json += "]}";

		return json;
	}

	protected static string FloatJson(float value)
	{
		return value.ToString(-1, 3);
	}

	protected static string BoolJson(bool value)
	{
		if (value)
			return "true";
		return "false";
	}

	protected LL_StatsViewSide TouchViewSide(notnull LL_StatsView view, notnull map<string, ref LL_StatsViewSide> sides, FactionManager fm, string factionKey)
	{
		if (factionKey == "")
			return null;

		LL_StatsViewSide side;
		if (sides.Find(factionKey, side))
			return side;

		side = new LL_StatsViewSide();
		side.faction = factionKey;
		side.displayName = factionKey;
		if (fm)
		{
			Faction f = fm.GetFactionByKey(factionKey);
			if (f)
				side.displayName = f.GetFactionName();
		}
		side.isWinner = (factionKey == m_sWinner);
		sides.Set(factionKey, side);
		view.sides.Insert(side);
		return side;
	}

	protected static LL_StatsViewRow ViewRowFrom(notnull LL_StatsUnitRow src)
	{
		LL_StatsViewRow row = new LL_StatsViewRow();
		row.unitTag = src.unitTag;
		row.unitName = src.unitName;
		row.faction = src.faction;
		row.kills = src.kills;
		row.zoneKills = src.zoneKills;
		row.aiKills = src.aiKills;
		row.teamkills = src.teamkills;
		row.deaths = src.deaths;
		row.survivors = src.survivors;
		row.participants = src.participants;
		row.objectivePoints = src.objectivePoints;
		row.basePoints = src.basePoints;
		row.multiplier = src.multiplier;
		row.finalPoints = src.finalPoints;
		row.isCommander = src.isCommander;
		row.isWinnerSide = src.isWinnerSide;
		return row;
	}

	protected void WriteSnapshot(string phase)
	{
		FileIO.MakeDirectory(OUTPUT_DIR);

		LL_StatsSnapshot snap = BuildSnapshot(phase);

		PrettyJsonSaveContext ctx = new PrettyJsonSaveContext();
		if (!ctx.WriteValue("", snap))
		{
			Print("[LL_Lobby] Stats: snapshot serialization failed", LogLevel.ERROR);
			return;
		}

		string path = SnapshotPath(phase);
		if (!ctx.SaveToFile(path))
			Print("[LL_Lobby] Stats: could not write " + path, LogLevel.ERROR);
	}

	protected string SnapshotPath(string phase)
	{
		return string.Format("%1/%2-%3.json", OUTPUT_DIR, m_sSessionId, phase);
	}

	protected LL_StatsEvent AddEvent(string type, string actor, string victim)
	{
		LL_StatsEvent ev = new LL_StatsEvent();
		ev.t = (System.GetTickCount() - m_iGameStartTick) / 1000.0;
		ev.type = type;
		ev.actor = actor;
		ev.victim = victim;
		m_aEvents.Insert(ev);
		return ev;
	}

	// One server session = one episode; a restart mints a new id.
	protected void MintSessionIdentity()
	{
		int y, mo, d, h, mi, s;
		System.GetYearMonthDay(y, mo, d);
		System.GetHourMinuteSecond(h, mi, s);

		m_sWorld = GetGame().GetWorldFile();

		string worldSlug = m_sWorld;
		int slash = worldSlug.LastIndexOf("/");
		if (slash >= 0)
			worldSlug = worldSlug.Substring(slash + 1, worldSlug.Length() - slash - 1);
		int dot = worldSlug.LastIndexOf(".");
		if (dot > 0)
			worldSlug = worldSlug.Substring(0, dot);

		m_sSessionId = string.Format("%1%2%3-%4%5%6-%7", y, mo.ToString(2), d.ToString(2), h.ToString(2), mi.ToString(2), s.ToString(2), worldSlug);

		m_sMissionName = worldSlug;
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (header && header.m_sName != "")
			m_sMissionName = header.m_sName;
	}

	protected static string NowStamp()
	{
		int y, mo, d, h, mi, s;
		System.GetYearMonthDay(y, mo, d);
		System.GetHourMinuteSecond(h, mi, s);
		return string.Format("%1-%2-%3 %4:%5:%6", y, mo.ToString(2), d.ToString(2), h.ToString(2), mi.ToString(2), s.ToString(2));
	}

	// Same ceiling as the website-slotting label chunks.
	static const int RPC_CHUNK_CHARS = 800;

	static void SplitChunks(string value, notnull array<string> outChunks)
	{
		int len = value.Length();
		for (int at = 0; at < len; at += RPC_CHUNK_CHARS)
		{
			int take = RPC_CHUNK_CHARS;
			if (at + take > len)
				take = len - at;
			outChunks.Insert(value.Substring(at, take));
		}
	}

	static string JsonEscape(string s)
	{
		s.Replace("\\", "\\\\");
		s.Replace("\"", "\\\"");
		s.Replace("\n", "\\n");
		s.Replace("\r", "\\r");
		s.Replace("\t", "\\t");
		return s;
	}

	// RPC signatures take scalars, not object arrays.
	static string EncodeCommanders(notnull array<ref LL_StatsCommander> commanders)
	{
		string encoded = "";
		foreach (LL_StatsCommander cmd : commanders)
		{
			if (cmd.faction == "" || cmd.unitTag == "")
				continue;
			if (encoded != "")
				encoded += "\n";
			encoded += cmd.faction + "\t" + cmd.unitTag;
		}
		return encoded;
	}

	static void DecodeCommanders(string encoded, notnull array<ref LL_StatsCommander> outCommanders)
	{
		array<string> lines = {};
		encoded.Split("\n", lines, true);

		foreach (string line : lines)
		{
			int tab = line.IndexOf("\t");
			if (tab < 1)
				continue;

			LL_StatsCommander cmd = new LL_StatsCommander();
			cmd.faction = line.Substring(0, tab);
			cmd.unitTag = line.Substring(tab + 1, line.Length() - tab - 1);
			if (cmd.unitTag != "")
				outCommanders.Insert(cmd);
		}
	}
}