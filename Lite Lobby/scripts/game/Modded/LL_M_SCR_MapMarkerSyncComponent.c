// Server-side gate for player-placed marker broadcasts. A player without a slot has no
// engine faction, so their markers would carry faction flags 0 and reach both sides; a
// player whose character died is a spectator and must not feed or remove intel.

modded class SCR_MapMarkerSyncComponent
{
	// Vanilla's per-player cap is 10 and the mission header is read in the stock
	// OnPostInit, so the value is set after super. Must stay positive: <= 0 disables
	// placement. Markers persist across disconnects, so this also bounds accumulation.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_iPlacedMarkerLimit = 100;
	}

	// A faction (slot taken) and a living slot character.
	protected bool LL_CanEditSharedMarkers()
	{
		// Nothing is placed or moved while the world is held.
		if (LL_GameModeCoop.IsHardFreezeActive())
			return false;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetOwner());
		if (!playerController)
			return false;

		int playerId = playerController.GetPlayerId();

		if (!SCR_FactionManager.SGetPlayerFaction(playerId))
			return false;

		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		if (lobbyManager)
		{
			LL_SlotData slot = lobbyManager.FindSlotByPlayerId(playerId);
			if (slot && slot.IsDestroyed())
				return false;
		}

		return true;
	}

	override protected void RPC_AskAddStaticMarker(SCR_MapMarkerBase markerData)
	{
		if (!LL_CanEditSharedMarkers())
			return;

		super.RPC_AskAddStaticMarker(markerData);
	}

	override protected void RPC_AskRemoveStaticMarker(int markerID)
	{
		if (!LL_CanEditSharedMarkers())
			return;

		super.RPC_AskRemoveStaticMarker(markerID);
	}

	// A disconnecting player's markers are kept. The stock ClearOwnedMarkers loops with i--
	// and only advances when AskRemoveStaticMarker shrinks the list synchronously, but that
	// removal is a Server-receiver RPC, which never runs locally when the server sends it,
	// so the loop spins forever and the 120 s watchdog force-crashes the server with no
	// callstack. Keeping the markers is also the wanted behaviour: players time out and
	// reconnect constantly and their markers would not come back. Stale markers from
	// players who never return are faction-gated and admin-removable.
	override void ClearOwnedMarkers()
	{
		Print(string.Format("[LL_Lobby] ClearOwnedMarkers skipped — keeping %1 marker(s) for disconnecting player", m_OwnedMarkers.Count()), LogLevel.NORMAL);
	}


	protected int m_LL_iHistoryRequest;
	protected ref LL_MapPaintHistorySnapshot m_LL_HistorySnapshot;

	int LL_RequestPaintHistory()
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
			return -1;
		m_LL_iHistoryRequest++;
		m_LL_HistorySnapshot = new LL_MapPaintHistorySnapshot();
		m_LL_HistorySnapshot.m_iRequest = m_LL_iHistoryRequest;
		m_LL_HistorySnapshot.m_sFactionKey = manager.LL_GetLocalPaintFactionKey();
		if (RplSession.Mode() == RplMode.Client)
			Rpc(LL_RpcAskPaintHistory, m_LL_iHistoryRequest, m_LL_HistorySnapshot.m_sFactionKey);
		else
			LL_RpcAskPaintHistory(m_LL_iHistoryRequest, m_LL_HistorySnapshot.m_sFactionKey);
		return m_LL_iHistoryRequest;
	}

	void LL_CancelPaintHistory()
	{
		m_LL_HistorySnapshot = null;
	}

	LL_MapPaintHistorySnapshot LL_GetPaintHistorySnapshot(int request)
	{
		if (!m_LL_HistorySnapshot || m_LL_HistorySnapshot.m_iRequest != request || !m_LL_HistorySnapshot.m_bReady)
			return null;
		return m_LL_HistorySnapshot;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void LL_RpcAskPaintHistory(int request, string factionKey)
	{
		if (RplSession.Mode() == RplMode.Client)
			return;
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
		{
			LL_SendPaintHistoryBegin(request, factionKey, false);
			LL_SendPaintHistoryEnd(request, factionKey);
			return;
		}
		manager.LL_ServerPaintHistory_S(this, request, factionKey);
	}

	protected bool LL_MatchesPaintHistory(int request, string factionKey)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		return m_LL_HistorySnapshot && m_LL_HistorySnapshot.m_iRequest == request
			&& m_LL_HistorySnapshot.m_sFactionKey == factionKey && manager
			&& manager.LL_GetLocalPaintFactionKey() == factionKey;
	}

	void LL_SendPaintHistoryBegin(int request, string factionKey, bool available)
	{
		if (this == LL_GetLocalPaintSync())
			LL_RpcPaintHistoryBegin(request, factionKey, available);
		else
			Rpc(LL_RpcPaintHistoryBegin, request, factionKey, available);
	}

	void LL_SendPaintHistoryBatch(int request, string factionKey, array<ref LL_MapPaintHistoryEntry> entries, int start)
	{
		array<string> authors = {};
		array<int> tools = {};
		array<string> erasedAuthors = {};
		int end = Math.Min(start + LL_MapPaintHistorySnapshot.BATCH_SIZE, entries.Count());
		for (int index = start; index < end; index++)
		{
			LL_MapPaintHistoryEntry entry = entries[index];
			authors.Insert(entry.m_sAuthor);
			tools.Insert(entry.m_iTool);
			erasedAuthors.Insert(entry.m_sErasedAuthor);
		}
		if (this == LL_GetLocalPaintSync())
			LL_RpcPaintHistoryBatch(request, factionKey, authors, tools, erasedAuthors);
		else
			Rpc(LL_RpcPaintHistoryBatch, request, factionKey, authors, tools, erasedAuthors);
	}

	void LL_SendPaintHistoryEnd(int request, string factionKey)
	{
		if (this == LL_GetLocalPaintSync())
			LL_RpcPaintHistoryEnd(request, factionKey);
		else
			Rpc(LL_RpcPaintHistoryEnd, request, factionKey);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintHistoryBegin(int request, string factionKey, bool available)
	{
		if (!LL_MatchesPaintHistory(request, factionKey) || m_LL_HistorySnapshot.m_bReady)
			return;
		m_LL_HistorySnapshot.m_Entries.Clear();
		m_LL_HistorySnapshot.m_bAvailable = available;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintHistoryBatch(int request, string factionKey, array<string> authors, array<int> tools, array<string> erasedAuthors)
	{
		if (!LL_MatchesPaintHistory(request, factionKey) || m_LL_HistorySnapshot.m_bReady)
			return;
		m_LL_HistorySnapshot.AppendBatch(authors, tools, erasedAuthors);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintHistoryEnd(int request, string factionKey)
	{
		if (LL_MatchesPaintHistory(request, factionKey))
			m_LL_HistorySnapshot.m_bReady = true;
	}

	protected int m_LL_iToken;
	protected int m_LL_iRateTime;
	protected float m_LL_fTokens = 8;
	protected int m_LL_iSnapshotTime = -10000;
	protected ref array<ref LL_MapPaintPreview> m_LL_Previews = {};
	protected int m_LL_iPreviewVersion;

	static SCR_MapMarkerSyncComponent LL_GetLocalPaintSync()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;
		return SCR_MapMarkerSyncComponent.Cast(controller.FindComponent(SCR_MapMarkerSyncComponent));
	}

	array<ref LL_MapPaintPreview> LL_GetPaintPreviews() { return m_LL_Previews; }
	int LL_GetPreviewVersion() { return m_LL_iPreviewVersion; }

	bool LL_SubmitPaint(int kind, float size, int color, array<vector> points)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager || !manager.LL_GetPaintState().IsReady() || !LL_MapPaintProtocol.Validate(kind, size, points))
			return false;
		string factionKey = manager.LL_GetLocalPaintFactionKey();
		if (factionKey.IsEmpty())
			return false;
		if (m_LL_Previews.Count() >= 128)
			return false;
		color = LL_MapPaintProtocol.NormalizeColor(kind, color);
		m_LL_iToken++;
		if (LL_MapPaintProtocol.IsStroke(kind))
		{
			LL_MapPaintPreview preview = new LL_MapPaintPreview();
			preview.m_iToken = m_LL_iToken;
			preview.m_bAnchorContinuation = kind == LL_EMapPaintOperation.ANCHOR_CONTINUE;
			preview.m_Stroke = new LL_MapPaintStroke();
			preview.m_Stroke.m_fWidth = size;
			preview.m_Stroke.m_bAnchorLine = LL_MapPaintProtocol.IsAnchor(kind);
			preview.m_Stroke.m_iColor = color;
			PlayerController controller = GetGame().GetPlayerController();
			if (controller)
				preview.m_Stroke.m_iOwner = controller.GetPlayerId();
			if (kind == LL_EMapPaintOperation.CIRCLE)
				LL_MapPaintCircleGeometry.BuildPoints(points[0], points[1], preview.m_Stroke.m_Points);
			else
				preview.m_Stroke.m_Points.Copy(points);
			m_LL_Previews.Insert(preview);
			m_LL_iPreviewVersion++;
		}
		if (RplSession.Mode() == RplMode.Client)
			Rpc(LL_RpcAskPaint, m_LL_iToken, factionKey, kind, size, color, points);
		else
			LL_RpcAskPaint(m_LL_iToken, factionKey, kind, size, color, points);
		return true;
	}

	void LL_ResolvePaintPreview(int token)
	{
		for (int i = m_LL_Previews.Count() - 1; i >= 0; i--)
		{
			if (m_LL_Previews[i].m_iToken == token)
			{
				m_LL_Previews.Remove(i);
				m_LL_iPreviewVersion++;
				return;
			}
		}
	}

	void LL_ClearPaintPreviews()
	{
		if (m_LL_Previews.IsEmpty())
			return;
		m_LL_Previews.Clear();
		m_LL_iPreviewVersion++;
	}

	void LL_SendPaintDelta(string factionKey, int revision, int owner, int token, int kind, float size, int color, array<vector> points)
	{
		Rpc(LL_RpcPaintDelta, factionKey, revision, owner, token, kind, size, color, points);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintDelta(string factionKey, int revision, int owner, int token, int kind, float size, int color, array<vector> points)
	{
		if (RplSession.Mode() != RplMode.Client)
			return;
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager || factionKey.IsEmpty() || manager.LL_GetLocalPaintFactionKey() != factionKey)
			return;
		LL_MapPaintOperation operation = new LL_MapPaintOperation();
		operation.m_iRevision = revision;
		operation.m_iOwner = owner;
		operation.m_iToken = token;
		operation.m_Kind = kind;
		operation.m_fSize = size;
		operation.m_iColor = color;
		operation.m_Points.Copy(points);
		manager.LL_GetPaintState().Receive(operation);
		PlayerController controller = GetGame().GetPlayerController();
		if (controller && controller.GetPlayerId() == owner)
			LL_ResolvePaintPreview(token);
	}

	void LL_AcknowledgePaint(int token)
	{
		if (this == LL_GetLocalPaintSync())
			LL_ResolvePaintPreview(token);
		else
			Rpc(LL_RpcPaintAcknowledged, token);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintAcknowledged(int token)
	{
		LL_ResolvePaintPreview(token);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void LL_RpcAskPaint(int token, string factionKey, int kind, float size, int color, array<vector> points)
	{
		if (RplSession.Mode() == RplMode.Client)
			return;
		if (!LL_CanEditSharedMarkers())
		{
			LL_AcknowledgePaint(token);
			return;
		}
		int now = System.GetTickCount();
		m_LL_fTokens = Math.Min(8, m_LL_fTokens + Math.Max(0, now - m_LL_iRateTime) * 0.02);
		m_LL_iRateTime = now;
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (m_LL_fTokens < 1 || !manager)
		{
			LL_AcknowledgePaint(token);
			return;
		}
		m_LL_fTokens--;
		manager.LL_ServerPaint_S(this, token, factionKey, kind, size, color, points);
	}

	void LL_RequestPaintSnapshot(int request)
	{
		Rpc(LL_RpcAskPaintSnapshot, request);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void LL_RpcAskPaintSnapshot(int request)
	{
		if (RplSession.Mode() == RplMode.Client)
			return;
		int now = System.GetTickCount();
		if (now - m_LL_iSnapshotTime < 2000)
			return;
		m_LL_iSnapshotTime = now;
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			manager.LL_ServerPaintSnapshot_S(this, request);
	}

	void LL_SendPaintSnapshotBegin(int request, int revision)
	{
		Rpc(LL_RpcPaintSnapshotBegin, request, revision);
	}

	void LL_SendPaintSnapshotChunk(int request, bool first, float width, int color, array<vector> points, int owner, bool anchorLine)
	{
		Rpc(LL_RpcPaintSnapshotChunk, request, first, width, color, points, owner, anchorLine);
	}

	void LL_SendPaintSnapshotEnd(int request)
	{
		Rpc(LL_RpcPaintSnapshotEnd, request);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintSnapshotBegin(int request, int revision)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			manager.LL_GetPaintState().BeginSnapshot(request, revision);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintSnapshotChunk(int request, bool first, float width, int color, array<vector> points, int owner, bool anchorLine)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			manager.LL_GetPaintState().SnapshotChunk(request, first, width, color, points, owner, anchorLine);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void LL_RpcPaintSnapshotEnd(int request)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			manager.LL_GetPaintState().EndSnapshot(request);
	}
}
