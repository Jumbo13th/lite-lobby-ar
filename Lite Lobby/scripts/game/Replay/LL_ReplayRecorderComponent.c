class LL_ReplayRecorderComponentClass : SCR_BaseGameModeComponentClass
{
}

class LL_ReplayConfig
{
	bool Enabled = false;
}

//! Server-only replay recorder of the game phase into $profile:LL_Replays/. Off until
//! $profile:LL_Replays.json enables it; nothing goes on the network but the admin's /replay reply.
class LL_ReplayRecorderComponent : SCR_BaseGameModeComponent
{
	protected static const string CONFIG_PATH = "$profile:LL_Replays.json";
	protected static const int TICK_MS = 100;
	//! Every 9 s, so a record reaches the disk inside 10 s even when the write takes a few ticks.
	protected static const int FLUSH_EVERY_TICKS = 90;
	protected static const int SNAPSHOT_EVERY_T = 3000;
	protected static const int SNAPSHOT_ENTITIES_PER_TICK = 100;

	static const int STATE_OFF = 0;
	static const int STATE_READY = 1;
	static const int STATE_RESUME = 2;
	static const int STATE_RECORDING = 3;
	static const int STATE_FAILED = 4;

	protected static LL_ReplayRecorderComponent s_Instance;
	//! Static because the session save can be read before this component initialises.
	protected static ref LL_ReplayContinuation s_PendingContinuation;
	protected static BaseWorld s_PendingWorld;

	protected int m_iState;
	protected string m_sSession;
	protected int m_iSegment;
	protected int m_iStartUnix;
	protected string m_sMintedSession;

	protected ref LL_ReplayWriter m_Writer;
	protected ref LL_ReplayEntities m_Entities;
	protected ref LL_ReplayCombat m_Combat;
	protected ref LL_ReplayMap m_Map;

	protected int m_iTicks;
	protected bool m_bFlushDue;
	protected int m_iLastSnapshotT;
	protected ref array<LL_ReplayEntityRecord> m_aSnapshotRecords = {};
	protected int m_iSnapshotCursor = -1;
	protected int m_iTickMsSum;
	protected int m_iTickMsMax;
	protected int m_iPeriodStartUnix;
	protected int m_iLastFreezeSeconds = -1;
	protected int m_iLastFreezeT = -1;
	protected bool m_bStopWritten;

	static LL_ReplayRecorderComponent GetInstance()
	{
		return s_Instance;
	}

	//! The status file is written one frame later, once every component's OnPostInit has run
	//! and the statistics' session exists.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer() || !GetGame().InPlayMode())
			return;

		s_Instance = this;
		if (!LoadConfig())
			return;

		m_iState = STATE_READY;
		LL_GameModeCoop coop = LL_GameModeCoop.Cast(owner);
		if (coop && coop.IsResumeExpected())
			m_iState = STATE_RESUME;

		GetGame().GetCallqueue().CallLater(WriteInitialStatus_S, 0, false);
	}

	override void OnDelete(IEntity owner)
	{
		StopParts_S();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		super.OnGameStateChanged(state);

		if (state == SCR_EGameModeState.GAME && (m_iState == STATE_READY || m_iState == STATE_RESUME) && !LL_GameModeCoop.IsResumePending())
			StartRecording_S(ResolveSession_S(), 1, -1);
	}

	override void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnControllableDestroyed(instigatorContextData);

		if (m_iState == STATE_RECORDING)
			m_Combat.OnDestroyed(instigatorContextData);
	}

	//! A clean stop ends the segment, not the recording: the session save may resume the game.
	//! The game can end more than once on the way out, and the stop is written once.
	override void OnGameEnd()
	{
		super.OnGameEnd();

		if (m_iState != STATE_RECORDING || m_bStopWritten)
			return;

		m_bStopWritten = true;
		AddNow("STOP shutdown");
		StopParts_S();
		FlushAll_S();
	}

	int GetRecorderState()
	{
		return m_iState;
	}

	string GetSession()
	{
		return m_sSession;
	}

	int GetSegment()
	{
		return m_iSegment;
	}

	int GetRecordedSeconds()
	{
		if (m_iStartUnix == 0)
			return 0;

		return System.GetUnixTime() - m_iStartUnix;
	}

	int GetKilobytes()
	{
		if (!m_Writer)
			return 0;

		return m_Writer.GetBytes() / 1024;
	}

	int GetSecondsSinceWrite()
	{
		if (!m_Writer || m_Writer.GetLastWriteUnix() == 0)
			return -1;

		return System.GetUnixTime() - m_Writer.GetLastWriteUnix();
	}

	//! Null while not recording, so a session save outside a recording carries no record.
	LL_ReplayContinuation CaptureContinuation_S()
	{
		if (m_iState != STATE_RECORDING)
			return null;

		LL_ReplayContinuation continuation = new LL_ReplayContinuation();
		continuation.session = m_sSession;
		continuation.segment = m_iSegment;
		continuation.snapshotT = CurrentT();
		m_Entities.CaptureContinuation(continuation);
		m_Combat.CaptureContinuation(continuation);
		return continuation;
	}

	static void SetPendingContinuation_S(LL_ReplayContinuation continuation)
	{
		s_PendingContinuation = continuation;
		s_PendingWorld = GetGame().GetWorld();
	}

	//! Called once the resumed world is complete and the mission clock is restored. Without a
	//! continuation a new session is minted: the statistics carry the crashed game's identity again.
	static void ResumeAfterSnapshot_S()
	{
		if (s_Instance)
			s_Instance.Resume_S();
	}

	static void RecordFreeze_S(float remaining)
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (!recorder || recorder.m_iState != STATE_RECORDING)
			return;

		int seconds = Math.Round(remaining);
		int t = recorder.CurrentT();
		if (seconds == recorder.m_iLastFreezeSeconds && t == recorder.m_iLastFreezeT)
			return;

		recorder.m_iLastFreezeSeconds = seconds;
		recorder.m_iLastFreezeT = t;
		recorder.AddNow("FLOW freeze " + seconds);
	}

	static void RecordFreezeEnd_S()
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (recorder && recorder.m_iState == STATE_RECORDING)
			recorder.AddNow("FLOW freeze-end");
	}

	static void RecordHardFreeze_S(int seconds)
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (!recorder || recorder.m_iState != STATE_RECORDING)
			return;

		int remaining = seconds;
		if (remaining < 0)
			remaining = 0;

		recorder.AddNow("FLOW hardfreeze " + remaining);
		recorder.m_Writer.SetClockHeld(true);
	}

	static void RecordHardFreezeEnd_S()
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (!recorder || recorder.m_iState != STATE_RECORDING)
			return;

		recorder.AddNow("FLOW hardfreeze-end");
		recorder.m_Writer.SetClockHeld(false);
	}

	static void RecordTrigger_S(string statKey, string label)
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (recorder && recorder.m_iState == STATE_RECORDING)
			recorder.AddNow("TR " + LL_ReplayWriter.Str(statKey) + " " + LL_ReplayWriter.Str(label));
	}

	static void RecordZoneCaptured_S(LL_TriggerZoneBase zone, string side)
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (!recorder || recorder.m_iState != STATE_RECORDING)
			return;

		recorder.m_Writer.SetTime(recorder.CurrentT());
		recorder.m_Map.RecordZoneCaptured_S(zone, side);
	}

	static void RecordZoneHolder_S(LL_TriggerZoneBase zone, string holder)
	{
		LL_ReplayRecorderComponent recorder = s_Instance;
		if (!recorder || recorder.m_iState != STATE_RECORDING)
			return;

		recorder.m_Writer.SetTime(recorder.CurrentT());
		recorder.m_Map.RecordZoneHolder_S(zone, holder);
	}

	protected bool LoadConfig()
	{
		if (!FileIO.FileExists(CONFIG_PATH))
			return false;

		LL_ReplayConfig config = new LL_ReplayConfig();
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(CONFIG_PATH) || !ctx.ReadValue("", config))
		{
			Print("[LL_Lobby] Replay: " + CONFIG_PATH + " cannot be read, recording stays off", LogLevel.WARNING);
			return false;
		}

		return config.Enabled;
	}

	protected void WriteInitialStatus_S()
	{
		if (m_iState == STATE_RESUME)
			LL_ReplayWriter.WriteStatus_S("resume", "", 0);
		else if (m_iState == STATE_READY)
			LL_ReplayWriter.WriteStatus_S("idle", ResolveSession_S(), 0);
	}

	protected void Resume_S()
	{
		if (m_iState != STATE_RESUME && m_iState != STATE_READY)
			return;

		EnsureParts();

		LL_ReplayContinuation continuation = s_PendingContinuation;
		bool sameWorld = s_PendingWorld == GetGame().GetWorld();
		s_PendingContinuation = null;
		s_PendingWorld = null;

		if (continuation && sameWorld)
		{
			m_Entities.RestoreContinuation(continuation);
			m_Combat.RestoreContinuation(continuation);
			StartRecording_S(continuation.session, continuation.segment + 1, continuation.snapshotT);
			return;
		}

		StartRecording_S(MintSession_S(), 1, -1);
	}

	protected void StartRecording_S(string session, int segment, int resumedT)
	{
		EnsureParts();

		m_sSession = session;
		m_iSegment = segment;
		m_iStartUnix = System.GetUnixTime();
		m_iPeriodStartUnix = m_iStartUnix;
		m_iTicks = 0;
		m_bFlushDue = false;

		m_Writer.Begin(session, segment);
		m_Writer.SetTime(CurrentT());
		m_Writer.SetClockHeld(LL_GameModeCoop.IsHardFreezeActive());
		WriteHeader_S(resumedT);
		m_Writer.Anchor();
		if (segment == 1)
			m_Writer.Add("FLOW game");

		m_Entities.Start_S();
		m_Combat.Start_S();
		m_Map.Start_S();
		m_Entities.Prime_S();

		m_iState = STATE_RECORDING;
		BeginSnapshot_S();

		GetGame().GetCallqueue().CallLater(Tick_S, TICK_MS, true);
		LL_ReplayWriter.WriteStatus_S("rec", session, segment);
		Print(string.Format("[LL_Lobby] Replay: recording %1 segment %2", session, segment), LogLevel.NORMAL);
	}

	protected void Fail_S()
	{
		StopParts_S();
		m_iState = STATE_FAILED;
		LL_ReplayWriter.WriteStatus_S("idle", m_sSession, 0);
	}

	protected void Tick_S()
	{
		if (m_iState != STATE_RECORDING)
			return;

		if (m_Writer.HasFailed())
		{
			Fail_S();
			return;
		}

		int startTick = System.GetTickCount();
		int t = CurrentT();
		m_Writer.SetTime(t);
		m_Entities.Tick_S();

		m_iTicks++;
		if (m_iTicks % 10 == 0)
		{
			m_Entities.SecondStep_S();
			m_Combat.SecondStep_S();
			m_Map.SecondStep_S();
		}

		if (m_iSnapshotCursor >= 0)
			SnapshotStep_S();
		else if (t - m_iLastSnapshotT >= SNAPSHOT_EVERY_T)
			BeginSnapshot_S();

		if (m_iTicks % FLUSH_EVERY_TICKS == 0)
		{
			m_Writer.Anchor();
			m_bFlushDue = true;
		}

		if (m_bFlushDue && m_Writer.FlushStep_S())
		{
			m_bFlushDue = false;
			if (!m_Writer.HasFailed())
				LL_ReplayWriter.WriteStatus_S("rec", m_sSession, m_iSegment);
		}

		int ms = System.GetTickCount() - startTick;
		m_iTickMsSum += ms;
		if (ms > m_iTickMsMax)
			m_iTickMsMax = ms;
	}

	//! A full-server snapshot is about a thousand lines, so it is built over several ticks.
	protected void BeginSnapshot_S()
	{
		m_Writer.BeginSnapshot();
		m_Combat.WriteSnapshotDictionaries_S();
		m_Entities.WriteRosterSnapshot_S();
		m_Map.WriteSnapshot_S();
		WriteFlowState_S();

		m_aSnapshotRecords.Clear();
		m_Entities.CollectSnapshotRecords(m_aSnapshotRecords);
		m_iSnapshotCursor = 0;
		m_iLastSnapshotT = m_Writer.GetTime();
		SnapshotStep_S();
	}

	protected void SnapshotStep_S()
	{
		int count = m_aSnapshotRecords.Count();
		int end = m_iSnapshotCursor + SNAPSHOT_ENTITIES_PER_TICK;
		if (end > count)
			end = count;

		for (int i = m_iSnapshotCursor; i < end; i++)
		{
			m_Entities.WriteSnapshotRecord_S(m_aSnapshotRecords[i]);
		}

		m_iSnapshotCursor = end;
		if (m_iSnapshotCursor < count)
			return;

		m_Writer.EndSnapshot();
		m_aSnapshotRecords.Clear();
		m_iSnapshotCursor = -1;
		LogSummary_S();
	}

	protected void WriteFlowState_S()
	{
		LL_GameModeCoop coop = LL_GameModeCoop.Cast(GetOwner());
		if (!coop)
			return;

		if (LL_GameModeCoop.IsHardFreezeActive())
		{
			int hardSeconds = Math.Round(coop.GetHardFreezeRemaining());
			if (hardSeconds < 0)
				hardSeconds = 0;

			m_Writer.AddSnapshotLine("FLOW hardfreeze " + hardSeconds);
			return;
		}

		if (coop.GetFreezeTimeRemaining() > 0)
		{
			int freezeSeconds = Math.Round(coop.GetFreezeTimeRemaining());
			m_Writer.AddSnapshotLine("FLOW freeze " + freezeSeconds);
		}
	}

	protected void LogSummary_S()
	{
		int flushMs;
		string writerSummary = m_Writer.TakeSummary(flushMs);

		int now = System.GetUnixTime();
		int seconds = now - m_iPeriodStartUnix;
		if (seconds < 1)
			seconds = 1;

		float average = m_iTickMsSum;
		average = average / seconds;

		Print(string.Format("[LL_Lobby] Replay: %1 seg %2 workMs=%3 avgMsPerSec=%4 maxTickMs=%5 %6 %7", m_sSession, m_iSegment,
			m_iTickMsSum, average.ToString(-1, 2), m_iTickMsMax, writerSummary, m_Combat.TakeShotCounts()), LogLevel.NORMAL);

		m_iTickMsSum = 0;
		m_iTickMsMax = 0;
		m_iPeriodStartUnix = now;
	}

	protected void WriteHeader_S(int resumedT)
	{
		m_Writer.AddHeader("LLR 1");
		m_Writer.AddHeader("H session " + m_sSession);
		m_Writer.AddHeader("H segment " + m_iSegment);

		string missionName;
		string author;
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (header)
		{
			missionName = header.m_sName;
			author = header.m_sAuthor;
		}

		m_Writer.AddHeader("H mission " + LL_ReplayWriter.Str(missionName));
		m_Writer.AddHeader("H author " + LL_ReplayWriter.Str(author));
		m_Writer.AddHeader("H world " + LL_ReplayWriter.Str(GetGame().GetWorldFile()));

		GenericWorldEntity worldEntity = GetGame().GetWorldEntity();
		if (worldEntity)
		{
			GenericTerrainEntity terrain = worldEntity.GetTerrain(0, 0);
			if (terrain)
			{
				vector mins;
				vector maxs;
				terrain.GetTerrainBoundBox(mins, maxs);
				int minX = Math.Round(mins[0]);
				int minZ = Math.Round(mins[2]);
				int maxX = Math.Round(maxs[0]);
				int maxZ = Math.Round(maxs[2]);
				m_Writer.AddHeader(string.Format("H bounds %1 %2 %3 %4", minX, minZ, maxX, maxZ));
			}
		}

		ChimeraWorld world = GetOwner().GetWorld();
		if (world)
		{
			TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
			if (weather)
			{
				int year, month, day, hours, minutes, secs;
				weather.GetDate(year, month, day);
				weather.GetHoursMinutesSeconds(hours, minutes, secs);
				m_Writer.AddHeader(string.Format("H date %1 %2 %3 %4 %5", year, month, day, hours, minutes));
			}
		}

		m_Writer.AddHeader("H started " + m_iStartUnix);
		if (resumedT >= 0)
			m_Writer.AddHeader("H resumed " + resumedT);

		FactionManager factionManager = GetGame().GetFactionManager();
		if (factionManager)
		{
			array<Faction> factions = {};
			factionManager.GetFactionsList(factions);
			foreach (Faction faction : factions)
			{
				m_Writer.AddHeader(string.Format("H side %1 %2 %3", LL_ReplayWriter.Tok(faction.GetFactionKey()),
					LL_ReplayWriter.Str(faction.GetFactionName()), LL_ReplayWriter.ColorHex(faction.GetFactionColor(), false)));
			}
		}

		m_Writer.AddHeader("H end");
	}

	protected void AddNow(string line)
	{
		m_Writer.SetTime(CurrentT());
		m_Writer.Add(line);
	}

	protected void EnsureParts()
	{
		if (m_Writer)
			return;

		m_Writer = new LL_ReplayWriter();
		m_Entities = new LL_ReplayEntities();
		m_Combat = new LL_ReplayCombat();
		m_Map = new LL_ReplayMap();
		m_Entities.Init(m_Writer, m_Combat);
		m_Combat.Init(m_Writer, m_Entities);
		m_Map.Init(m_Writer, m_Entities);
	}

	protected void StopParts_S()
	{
		GetGame().GetCallqueue().Remove(Tick_S);

		if (m_Entities)
			m_Entities.Stop_S();
		if (m_Combat)
			m_Combat.Stop_S();
	}

	protected void FlushAll_S()
	{
		if (!m_Writer)
			return;

		while (!m_Writer.FlushStep_S())
		{
		}
	}

	protected int CurrentT()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetOwner());
		if (!gameMode)
			return 0;

		return Math.Round(gameMode.GetElapsedTime() * 10);
	}

	//! The statistics' identity when they run, so both file families name the same game.
	protected string ResolveSession_S()
	{
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
		{
			string statsSession = stats.GetSessionId();
			if (statsSession != "")
				return statsSession;
		}

		return MintSession_S();
	}

	protected string MintSession_S()
	{
		if (m_sMintedSession != "")
			return m_sMintedSession;

		int y, mo, d, h, mi, s;
		System.GetYearMonthDay(y, mo, d);
		System.GetHourMinuteSecond(h, mi, s);

		string worldSlug = GetGame().GetWorldFile();
		int slash = worldSlug.LastIndexOf("/");
		if (slash >= 0)
			worldSlug = worldSlug.Substring(slash + 1, worldSlug.Length() - slash - 1);
		int dot = worldSlug.LastIndexOf(".");
		if (dot > 0)
			worldSlug = worldSlug.Substring(0, dot);

		m_sMintedSession = string.Format("%1%2%3-%4%5%6-%7", y, mo.ToString(2), d.ToString(2), h.ToString(2), mi.ToString(2), s.ToString(2), worldSlug);
		return m_sMintedSession;
	}
}
