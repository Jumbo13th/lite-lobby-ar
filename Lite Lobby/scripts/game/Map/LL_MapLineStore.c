// Server-side map lines: storage, every check a request has to pass, delivery to the
// author's faction only, and the paced re-delivery after a player's faction changes.
// Created by LL_LobbyManager on the authority. Lines are faction intel, so they never ride
// a broadcast or the join snapshot: each recipient gets its own owner RPC.

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

		// Removal stays open after the freeze: an outdated plan can still be cleared.
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

		// Written so that NaN fails too.
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
		if (m_mLastAccepted.Find(authorKey, last) && now - last < LL_MapLine.MIN_INTERVAL_S * 1000)
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

		return LL_EMapLineRefusal.NONE;
	}

	//! Any player of the line's faction who may place markers: a flood is cleaned up by the
	//! players themselves, and the audit line names who did it. Silently ignored when not
	//! allowed: a removal reveals nothing and needs no answer.
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
	}

	//! Clears the player's lines, then queues the faction's. A newer sync for the same
	//! player replaces the pending one; owner RPCs arrive in order, so the clear always
	//! lands before the lines of its own sync and after any line sent earlier.
	void StartSync_S(int playerId, Faction faction)
	{
		if (!Replication.IsServer())
			return;

		for (int i = m_aJobs.Count() - 1; i >= 0; i--)
		{
			if (m_aJobs[i].m_iPlayerId == playerId)
				m_aJobs.RemoveOrdered(i);
		}

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetByPlayerId(playerId);
		if (!lobbyPlayer)
			return;

		lobbyPlayer.SendMapLinesReset_S();

		if (!faction)
			return;

		LL_MapLineSyncJob job = new LL_MapLineSyncJob();
		job.m_iPlayerId = playerId;

		FactionKey factionKey = faction.GetFactionKey();
		foreach (LL_MapLine line : m_aLines)
		{
			if (line.m_sFactionKey == factionKey)
				job.m_aLineIds.Insert(line.m_iId);
		}

		if (job.m_aLineIds.IsEmpty())
			return;

		m_aJobs.Insert(job);
		ScheduleTick_S();
	}

	//! Lines from a resumed session, then a sync for everyone already in a faction.
	void Restore_S(notnull array<ref LL_MapLine> lines)
	{
		foreach (LL_MapLine line : lines)
		{
			if (FindIndex(line.m_iId) >= 0)
				continue;

			m_aLines.Insert(line);
			if (line.m_iId >= m_iNextId)
				m_iNextId = line.m_iId + 1;
		}

		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
			if (faction)
				StartSync_S(playerId, faction);
		}
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

				// Deleted while queued.
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

	//! The same rule as shared markers: a faction, a living slot character, no hard freeze.
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

	//! Survives reconnects; a session-scoped fallback only when the player has no key.
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
