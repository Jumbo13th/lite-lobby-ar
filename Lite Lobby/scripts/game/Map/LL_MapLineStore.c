// Server-side map lines: checks, storage and delivery to the author's faction. Lines are faction
// intel, so they never ride a broadcast or the join snapshot: each recipient gets an owner RPC.

class LL_MapLineSyncJob
{
	int m_iPlayerId;
	ref array<int> m_aLineIds = {};
}

class LL_MapLineStore
{
	// After a session resume every player retakes a slot at about the same time; at the
	// faction limit an unpaced sync is about 75 KB per player at once.
	protected static const int SYNC_TICK_MS = 250;
	protected static const int SYNC_LINES_PER_PLAYER = 10;
	protected static const int SYNC_LINES_PER_TICK = 100;

	protected ref array<ref LL_MapLine> m_aLines = {};
	protected int m_iNextId = 1;

	// Author key → world time (ms) of the last accepted line.
	protected ref map<string, float> m_mLastAccepted = new map<string, float>();

	protected ref array<ref LL_MapLineSyncJob> m_aJobs = {};

	// Player id → faction held ("" for none) / asked for since the last tick. A slot switch is a
	// leave and a take in one frame; settling per tick makes a same-side switch a no-op.
	protected ref map<int, string> m_mSyncedFaction = new map<int, string>();
	protected ref map<int, string> m_mPendingFaction = new map<int, string>();
	protected bool m_bTickScheduled;

	array<ref LL_MapLine> GetLines()
	{
		return m_aLines;
	}

	void Stop()
	{
		GetGame().GetCallqueue().Remove(SyncTick_S);
		m_bTickScheduled = false;
		m_aJobs.Clear();
	}

	//! Returns NONE when the line was stored and delivered.
	LL_EMapLineRefusal Add_S(int playerId, int color, float width, array<int> points)
	{
		if (!Replication.IsServer())
			return LL_EMapLineRefusal.NOT_ALLOWED;

		if (!CanEdit_S(playerId))
			return Refuse_S(playerId, LL_EMapLineRefusal.NOT_ALLOWED);

		Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		if (!faction)
			return Refuse_S(playerId, LL_EMapLineRefusal.NOT_ALLOWED);

		if (!LL_MapLine.IsDrawingPhase())
			return Refuse_S(playerId, LL_EMapLineRefusal.FREEZE_OVER);

		if (!points || points.Count() % 2 != 0 || points.Count() < 4 || points.Count() > LL_MapLine.MAX_POINTS * 2)
			return Refuse_S(playerId, LL_EMapLineRefusal.INVALID);

		vector worldMin, worldMax;
		GetGame().GetWorld().GetBoundBox(worldMin, worldMax);
		for (int i = 0; i < points.Count(); i += 2)
		{
			float x = LL_MapLine.ToMetres(points[i]);
			float y = LL_MapLine.ToMetres(points[i + 1]);
			if (x < worldMin[0] || x > worldMax[0] || y < worldMin[2] || y > worldMax[2])
				return Refuse_S(playerId, LL_EMapLineRefusal.INVALID);
		}

		if (color < 0 || color >= GetColorCount())
			return Refuse_S(playerId, LL_EMapLineRefusal.INVALID);

		// NaN fails only in this form.
		if (!(width >= LL_MapLine.MIN_WIDTH_M && width <= LL_MapLine.MAX_WIDTH_M))
			return Refuse_S(playerId, LL_EMapLineRefusal.INVALID);

		string authorKey = GetAuthorKey_S(playerId);
		FactionKey factionKey = faction.GetFactionKey();

		if (CountByAuthor(authorKey) >= LL_MapLine.MAX_LINES_PER_PLAYER)
			return Refuse_S(playerId, LL_EMapLineRefusal.PLAYER_LIMIT);

		if (CountByFaction(factionKey) >= LL_MapLine.MAX_LINES_PER_FACTION)
			return Refuse_S(playerId, LL_EMapLineRefusal.FACTION_LIMIT);

		float now = GetGame().GetWorld().GetWorldTime();
		float last;
		if (m_mLastAccepted.Find(authorKey, last) && now - last < LL_MapLine.SERVER_MIN_INTERVAL_S * 1000)
			return Refuse_S(playerId, LL_EMapLineRefusal.TOO_SOON);

		LL_MapLine line = new LL_MapLine();
		line.m_iId = m_iNextId;
		line.m_iColor = color;
		line.m_fWidth = width;
		line.m_aPoints.Copy(points);
		line.m_sFactionKey = factionKey;
		line.m_sAuthorKey = authorKey;
		line.m_sAuthorName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		ChimeraWorld world = GetGame().GetWorld();
		if (world)
			line.m_CreatedAt = world.GetServerTimestamp();

		m_iNextId++;
		m_aLines.Insert(line);
		m_mLastAccepted.Set(authorKey, now);

		Print(string.Format("[LL_MapAudit] Line %1 drawn by %2 (player %3, %4): %5 points from %6 %7, colour %8, width %9 m",
			line.m_iId, line.m_sAuthorName, playerId, factionKey, line.GetPointCount(),
			LL_MapLine.ToMetres(points[0]), LL_MapLine.ToMetres(points[1]), color, width), LogLevel.NORMAL);

		array<int> recipients = {};
		GetFactionPlayers_S(factionKey, recipients);
		foreach (int recipientId : recipients)
		{
			LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetByPlayerId(recipientId);
			if (lobbyPlayer)
				lobbyPlayer.SendMapLine_S(line, GetAuthorKey_S(recipientId) == authorKey);
		}

		ResyncMissedChange_S(factionKey);
		return LL_EMapLineRefusal.NONE;
	}

	//! Any faction player who may place markers, so players clean up a flood themselves; the
	//! audit log names who. A refused removal gets no answer: it reveals nothing.
	void Remove_S(int playerId, int lineId)
	{
		if (!Replication.IsServer() || !CanEdit_S(playerId))
			return;

		Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		if (!faction)
			return;

		int index = FindIndex(lineId);
		if (index < 0)
			return;

		LL_MapLine line = m_aLines[index];
		if (line.m_sFactionKey != faction.GetFactionKey())
			return;

		Print(string.Format("[LL_MapAudit] Line %1 by %2 (%3) deleted by %4 (player %5)",
			lineId, line.m_sAuthorName, line.m_sFactionKey, GetGame().GetPlayerManager().GetPlayerName(playerId), playerId), LogLevel.NORMAL);

		FactionKey factionKey = line.m_sFactionKey;
		m_aLines.RemoveOrdered(index);

		array<int> recipients = {};
		GetFactionPlayers_S(factionKey, recipients);
		foreach (int recipientId : recipients)
		{
			LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetByPlayerId(recipientId);
			if (lobbyPlayer)
				lobbyPlayer.SendMapLineRemoved_S(lineId);
		}

		ResyncMissedChange_S(factionKey);
	}

	//! Asks for the player's lines to be those of the faction (none: cleared), settled on the
	//! next tick. force re-sends even when the player already holds that faction's lines.
	void StartSync_S(int playerId, Faction faction, bool force = false)
	{
		if (!Replication.IsServer())
			return;

		string factionKey;
		if (faction)
			factionKey = faction.GetFactionKey();

		if (force)
			m_mSyncedFaction.Remove(playerId);

		m_mPendingFaction.Set(playerId, factionKey);
		ScheduleTick_S();
	}

	//! A reconnecting client starts with no lines, whatever this player id held before.
	void ForgetPlayer_S(int playerId)
	{
		m_mSyncedFaction.Remove(playerId);
		m_mPendingFaction.Remove(playerId);
	}

	//! A player who left the faction and retakes it before the tick missed this change, and the
	//! tick would settle the switch as nothing; forgetting what they hold makes it a full resync.
	protected void ResyncMissedChange_S(FactionKey factionKey)
	{
		foreach (int playerId, string pendingKey : m_mPendingFaction)
		{
			string syncedKey;
			if (!m_mSyncedFaction.Find(playerId, syncedKey) || syncedKey != factionKey)
				continue;

			Faction current = SCR_FactionManager.SGetPlayerFaction(playerId);
			if (!current || current.GetFactionKey() != factionKey)
				m_mSyncedFaction.Remove(playerId);
		}
	}

	//! A newer sync replaces the queued one. Owner RPCs arrive in order, so the clear lands after
	//! any line sent earlier and before this sync's lines.
	protected void ApplySync_S(int playerId, FactionKey factionKey)
	{
		for (int i = m_aJobs.Count() - 1; i >= 0; i--)
		{
			if (m_aJobs[i].m_iPlayerId == playerId)
				m_aJobs.RemoveOrdered(i);
		}

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetByPlayerId(playerId);
		if (!lobbyPlayer)
			return;

		lobbyPlayer.SendMapLinesReset_S();
		m_mSyncedFaction.Set(playerId, factionKey);

		if (factionKey.IsEmpty())
			return;

		LL_MapLineSyncJob job = new LL_MapLineSyncJob();
		job.m_iPlayerId = playerId;

		foreach (LL_MapLine line : m_aLines)
		{
			if (line.m_sFactionKey == factionKey)
				job.m_aLineIds.Insert(line.m_iId);
		}

		if (!job.m_aLineIds.IsEmpty())
			m_aJobs.Insert(job);
	}

	//! A saved line whose id was taken by a line drawn before the restore is renumbered, not
	//! lost. Returns how many were restored.
	int Restore_S(notnull array<ref LL_MapLine> lines)
	{
		foreach (LL_MapLine line : lines)
		{
			if (line.m_iId >= m_iNextId)
				m_iNextId = line.m_iId + 1;
		}

		foreach (LL_MapLine line : lines)
		{
			if (FindIndex(line.m_iId) >= 0)
			{
				line.m_iId = m_iNextId;
				m_iNextId++;
			}

			m_aLines.Insert(line);
		}

		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
			if (faction)
				StartSync_S(playerId, faction, true);
		}

		return lines.Count();
	}

	protected void ScheduleTick_S()
	{
		if (m_bTickScheduled)
			return;

		m_bTickScheduled = true;
		GetGame().GetCallqueue().CallLater(SyncTick_S, SYNC_TICK_MS, false);
	}

	protected void SyncTick_S()
	{
		m_bTickScheduled = false;

		foreach (int pendingId, string pendingKey : m_mPendingFaction)
		{
			string syncedKey;
			if (m_mSyncedFaction.Find(pendingId, syncedKey) && syncedKey == pendingKey)
				continue;

			ApplySync_S(pendingId, pendingKey);
		}

		m_mPendingFaction.Clear();

		int budget = SYNC_LINES_PER_TICK;
		for (int i = 0; i < m_aJobs.Count() && budget > 0; i++)
		{
			LL_MapLineSyncJob job = m_aJobs[i];
			LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetByPlayerId(job.m_iPlayerId);
			if (!lobbyPlayer)
			{
				job.m_aLineIds.Clear();
				continue;
			}

			string recipientKey = GetAuthorKey_S(job.m_iPlayerId);
			int sent;
			while (!job.m_aLineIds.IsEmpty() && sent < SYNC_LINES_PER_PLAYER && budget > 0)
			{
				int index = FindIndex(job.m_aLineIds[0]);
				job.m_aLineIds.RemoveOrdered(0);

				if (index < 0)
					continue;

				LL_MapLine line = m_aLines[index];
				lobbyPlayer.SendMapLine_S(line, line.m_sAuthorKey == recipientKey);
				sent++;
				budget--;
			}
		}

		for (int i = m_aJobs.Count() - 1; i >= 0; i--)
		{
			if (m_aJobs[i].m_aLineIds.IsEmpty())
				m_aJobs.RemoveOrdered(i);
		}

		if (!m_aJobs.IsEmpty())
			ScheduleTick_S();
	}

	protected bool CanEdit_S(int playerId)
	{
		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!pc)
			return false;

		SCR_MapMarkerSyncComponent markerSync = SCR_MapMarkerSyncComponent.Cast(pc.FindComponent(SCR_MapMarkerSyncComponent));
		return markerSync && markerSync.LL_CanEditSharedMarkers();
	}

	protected LL_EMapLineRefusal Refuse_S(int playerId, LL_EMapLineRefusal reason)
	{
		Print(string.Format("[LL_Lobby] Map line refused for player %1: %2", playerId, typename.EnumToString(LL_EMapLineRefusal, reason)), LogLevel.NORMAL);
		return reason;
	}

	protected string GetAuthorKey_S(int playerId)
	{
		string key;
		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		if (lobbyManager)
			key = lobbyManager.GetReconnectKeyForPlayer_S(playerId);

		if (key.IsEmpty())
			key = "player:" + playerId.ToString();

		return key;
	}

	protected void GetFactionPlayers_S(FactionKey factionKey, notnull array<int> outPlayers)
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
			if (faction && faction.GetFactionKey() == factionKey)
				outPlayers.Insert(playerId);
		}
	}

	protected int GetColorCount()
	{
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerManager || !markerManager.GetMarkerConfig())
			return 0;

		SCR_MapMarkerEntryPlaced placed = SCR_MapMarkerEntryPlaced.Cast(markerManager.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
		if (!placed)
			return 0;

		return placed.GetColorEntries().Count();
	}

	protected int FindIndex(int lineId)
	{
		foreach (int i, LL_MapLine line : m_aLines)
		{
			if (line.m_iId == lineId)
				return i;
		}

		return -1;
	}

	protected int CountByAuthor(string authorKey)
	{
		int count;
		foreach (LL_MapLine line : m_aLines)
		{
			if (line.m_sAuthorKey == authorKey)
				count++;
		}

		return count;
	}

	protected int CountByFaction(FactionKey factionKey)
	{
		int count;
		foreach (LL_MapLine line : m_aLines)
		{
			if (line.m_sFactionKey == factionKey)
				count++;
		}

		return count;
	}
}
