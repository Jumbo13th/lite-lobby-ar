// Player-placed markers: server-side faction gating and re-sends on faction switch.
// Vanilla filters faction markers client-side at receipt and prunes on faction change
// without ever re-sending, so a switching player would lose every faction marker until
// reconnect. The dedicated server keeps every marker (the receipt filter only runs on
// machines with UI). A listen host prunes its own arrays when the host switches.

modded class SCR_MapMarkerManagerComponent
{
	// Only the PLACED_CUSTOM entry is defined here.
	protected const ResourceName LL_MARKER_CONFIG = "{1D20A7B8C9D0E1F2}Configs/Map/LL_MapMarkerConfig.conf";

	// One entry is swapped rather than the whole config: the other entries carry prefab
	// and layout references only the stock config knows. Markers replicate icon and colour
	// as indices into this entry and every machine runs the same swap. Preference:
	// LL_PlacedMarkers on the game-mode prefab, then Configs/Map/LL_MapMarkerConfig.conf.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		LL_SwapPlacedMarkerEntry(owner);
	}

	protected void LL_SwapPlacedMarkerEntry(IEntity owner)
	{
		if (!m_MarkerCfg)
			return;

		SCR_MapMarkerEntryPlaced llPlaced;
		Resource container = BaseContainerTools.LoadContainer(LL_MARKER_CONFIG);
		if (container)
		{
			SCR_MapMarkerConfig llConfig = SCR_MapMarkerConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
			if (llConfig)
				llPlaced = SCR_MapMarkerEntryPlaced.Cast(llConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
		}

		// Empty lists on the component inherit the conf's part inside BuildPlacedEntry.
		LL_PlacedMarkers placedMarkers = LL_PlacedMarkers.Cast(owner.FindComponent(LL_PlacedMarkers));
		if (placedMarkers)
		{
			SCR_MapMarkerEntryPlaced built = placedMarkers.BuildPlacedEntry(llPlaced);
			if (built)
				llPlaced = built;
		}

		if (!llPlaced)
			return;

		array<ref SCR_MapMarkerEntryConfig> entries = m_MarkerCfg.GetMarkerEntryConfigs();
		foreach (int i, SCR_MapMarkerEntryConfig entry : entries)
		{
			if (entry.GetMarkerType() == SCR_EMapMarkerType.PLACED_CUSTOM)
			{
				entries[i] = llPlaced;
				return;
			}
		}

		entries.Insert(llPlaced);
	}

	// Vanilla broadcasts every faction marker to every client and hides it with a
	// client-side test evaluated once on arrival, so a player between factions keeps enemy
	// markers. Gating on the server closes that class of bug and is cheaper: the entitled
	// subset is a strict subset of the broadcast. Faction-less markers still broadcast.
	override void OnAskAddStaticMarker(SCR_MapMarkerBase markerData)
	{
		if (!markerData || markerData.GetMarkerFactionFlags() == 0)
		{
			super.OnAskAddStaticMarker(markerData);
			return;
		}

		FactionManager factionManager = GetGame().GetFactionManager();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!factionManager || !playerManager)
			return;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
			if (!faction || !markerData.IsFaction(factionManager.GetFactionIndex(faction)))
				continue;

			PlayerController pc = playerManager.GetPlayerController(playerId);
			if (!pc)
				continue;

			LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.Cast(pc.FindComponent(LL_LobbyPlayerComponent));
			if (lobbyPlayer)
				lobbyPlayer.SendStaticMarker_S(markerData);
		}
	}

	// Set while applying a marker the server addressed to this client: the client's own
	// affiliation replicates separately and may not have landed yet.
	protected bool m_bLLTrustedAdd;

	void LL_AddTrustedMarker(SCR_MapMarkerBase marker)
	{
		m_bLLTrustedAdd = true;
		OnAddSynchedMarker(marker);
		m_bLLTrustedAdd = false;
	}

	// Second line of defence: a faction marker reaching a faction-less client is dropped
	// (vanilla would show it). Pure clients only: on a listen host these arrays are the
	// server storage.
	override void OnAddSynchedMarker(SCR_MapMarkerBase marker)
	{
		if (!marker || LL_IsKnownStaticMarker(marker, m_aStaticMarkers, m_aDisabledMarkers))
			return;

		if (!m_bLLTrustedAdd
			&& RplSession.Mode() == RplMode.Client
			&& marker.GetMarkerFactionFlags() != 0
			&& !SCR_FactionManager.SGetLocalPlayerFaction())
			return;

		super.OnAddSynchedMarker(marker);
	}

	// Faction-less markers are excluded; clients never drop those.
	void LL_GetStaticMarkersOfFaction(int factionIndex, notnull out array<SCR_MapMarkerBase> outMarkers)
	{
		foreach (SCR_MapMarkerBase marker : m_aStaticMarkers)
		{
			if (marker && marker.GetMarkerFactionFlags() != 0 && marker.IsFaction(factionIndex))
				outMarkers.Insert(marker);
		}

		foreach (SCR_MapMarkerBase marker : m_aDisabledMarkers)
		{
			if (marker && marker.GetMarkerFactionFlags() != 0 && marker.IsFaction(factionIndex))
				outMarkers.Insert(marker);
		}
	}


	protected ref map<string, ref LL_MapPaintHistory> m_LL_Histories = new map<string, ref LL_MapPaintHistory>();
	protected bool m_LL_bHistoryCleanup;

	void ~SCR_MapMarkerManagerComponent()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(LL_PrunePaintHistory);
	}

	void LL_RecordPaintHistory(string factionKey, int owner, int kind, array<int> erasedOwners = null)
	{
		if (RplSession.Mode() == RplMode.Client || factionKey.IsEmpty())
			return;
		LL_MapPaintHistory history = m_LL_Histories.Get(factionKey);
		if (!history)
		{
			history = new LL_MapPaintHistory();
			m_LL_Histories.Set(factionKey, history);
		}
		string author = LL_MapPaintHistory.GetPlayerName(owner);
		history.Record(owner, author, kind, System.GetTickCount(), LL_MapPaintHistory.GetPlayerNames(erasedOwners), erasedOwners);
		if (!m_LL_bHistoryCleanup)
		{
			GetGame().GetCallqueue().CallLater(LL_PrunePaintHistory, 1000, true);
			m_LL_bHistoryCleanup = true;
		}
	}

	//! Cleanup also runs when nobody draws and every map window is closed.
	protected void LL_PrunePaintHistory()
	{
		int now = System.GetTickCount();
		array<string> emptyFactions = {};
		foreach (string factionKey, LL_MapPaintHistory history : m_LL_Histories)
		{
			history.Prune(now);
			if (history.IsEmpty())
				emptyFactions.Insert(factionKey);
		}
		foreach (string emptyFaction : emptyFactions)
			m_LL_Histories.Remove(emptyFaction);
		if (m_LL_Histories.IsEmpty())
		{
			GetGame().GetCallqueue().Remove(LL_PrunePaintHistory);
			m_LL_bHistoryCleanup = false;
		}
	}

	void LL_ServerPaintHistory_S(SCR_MapMarkerSyncComponent target, int request, string requestedFaction)
	{
		if (RplSession.Mode() == RplMode.Client || !target)
			return;
		SCR_PlayerController owner = SCR_PlayerController.Cast(target.GetOwner());
		if (!owner)
			return;
		string factionKey = LL_GetPlayerPaintFactionKey(owner.GetPlayerId());
		// The owned controller and its current server-side faction select the log.
		if (factionKey.IsEmpty() || factionKey != requestedFaction)
		{
			target.LL_SendPaintHistoryBegin(request, requestedFaction, false);
			target.LL_SendPaintHistoryEnd(request, requestedFaction);
			return;
		}
		array<ref LL_MapPaintHistoryEntry> entries = {};
		LL_MapPaintHistory history = m_LL_Histories.Get(factionKey);
		if (history)
			entries = history.Snapshot(System.GetTickCount());
		target.LL_SendPaintHistoryBegin(request, factionKey, true);
		for (int start = 0; start < entries.Count(); start += LL_MapPaintHistorySnapshot.BATCH_SIZE)
			target.LL_SendPaintHistoryBatch(request, factionKey, entries, start);
		target.LL_SendPaintHistoryEnd(request, factionKey);
	}

	protected ref LL_MapPaintReplica m_LL_PaintState;
	protected ref map<string, ref LL_MapPaintReplica> m_LL_mPaintStatesByFaction = new map<string, ref LL_MapPaintReplica>();
	protected string m_LL_sActiveFactionKey;
	protected int m_LL_iSnapshotRequest;
	protected int m_LL_iLastSnapshotRequest = -10000;
	protected int m_LL_iGapSince;

	LL_MapPaintReplica LL_GetPaintState()
	{
		string factionKey = LL_GetLocalPaintFactionKey();
		if (RplSession.Mode() == RplMode.Client)
		{
			if (!m_LL_PaintState || factionKey != m_LL_sActiveFactionKey)
			{
				m_LL_sActiveFactionKey = factionKey;
				m_LL_PaintState = new LL_MapPaintReplica();
				m_LL_iLastSnapshotRequest = -10000;
				m_LL_iGapSince = 0;
				SCR_MapMarkerSyncComponent sync = SCR_MapMarkerSyncComponent.LL_GetLocalPaintSync();
				if (sync)
					sync.LL_ClearPaintPreviews();
			}
			LL_ConfigurePaintGrid(m_LL_PaintState);
			return m_LL_PaintState;
		}

		if (factionKey != m_LL_sActiveFactionKey)
		{
			m_LL_sActiveFactionKey = factionKey;
			m_LL_PaintState = m_LL_mPaintStatesByFaction.Get(factionKey);
			if (!m_LL_PaintState)
			{
				m_LL_PaintState = new LL_MapPaintReplica();
				m_LL_mPaintStatesByFaction.Set(factionKey, m_LL_PaintState);
			}
		}
		if (!m_LL_PaintState)
			m_LL_PaintState = new LL_MapPaintReplica();
		m_LL_PaintState.SetAuthority();
		LL_ConfigurePaintGrid(m_LL_PaintState);
		return m_LL_PaintState;
	}

	//! This path runs for server-owned faction documents and closed-map clients too.
	protected void LL_ConfigurePaintGrid(LL_MapPaintReplica state)
	{
		if (m_MapEntity && state)
			state.m_Document.ConfigureSpatialGrid(m_MapEntity.Offset(), m_MapEntity.Size());
	}

	//! Current player's faction is used only to select its local view of the document.
	string LL_GetLocalPaintFactionKey()
	{
		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
			return string.Empty;
		Faction faction = factionManager.GetLocalPlayerFaction();
		if (!faction)
			return string.Empty;
		return faction.GetFactionKey();
	}

	protected string LL_GetPlayerPaintFactionKey(int playerId)
	{
		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
			return string.Empty;
		Faction faction = factionManager.GetPlayerFaction(playerId);
		if (!faction)
			return string.Empty;
		return faction.GetFactionKey();
	}

	protected LL_MapPaintReplica LL_GetPaintStateForFaction(string factionKey)
	{
		if (factionKey.IsEmpty())
			return null;
		LL_MapPaintReplica state = m_LL_mPaintStatesByFaction.Get(factionKey);
		if (!state)
		{
			state = new LL_MapPaintReplica();
			state.SetAuthority();
			m_LL_mPaintStatesByFaction.Set(factionKey, state);
		}
		LL_ConfigurePaintGrid(state);
		return state;
	}

	void LL_EnsurePaintSync()
	{
		LL_MapPaintReplica state = LL_GetPaintState();
		if (RplSession.Mode() != RplMode.Client)
			return;
		if (LL_GetLocalPaintFactionKey().IsEmpty())
			return;
		int now = System.GetTickCount();
		if (state.HasGap())
		{
			if (m_LL_iGapSince == 0)
				m_LL_iGapSince = now;
		}
		else
			m_LL_iGapSince = 0;
		if (state.IsReady() && (m_LL_iGapSince == 0 || now - m_LL_iGapSince < 1500))
			return;
		if (now - m_LL_iLastSnapshotRequest < 3000)
			return;
		SCR_MapMarkerSyncComponent sync = SCR_MapMarkerSyncComponent.LL_GetLocalPaintSync();
		if (!sync)
			return;
		m_LL_iSnapshotRequest++;
		state.AwaitSnapshot(m_LL_iSnapshotRequest);
		m_LL_iLastSnapshotRequest = now;
		sync.LL_RequestPaintSnapshot(m_LL_iSnapshotRequest);
	}

	//! The owner controller supplies player identity; it is never trusted from a client payload.
	void LL_ServerPaint_S(SCR_MapMarkerSyncComponent sender, int token, string requestedFactionKey, int kind, float size, int color, array<vector> points)
	{
		if (RplSession.Mode() == RplMode.Client || !sender)
			return;
		if (!LL_MapPaintProtocol.Validate(kind, size, points))
		{
			sender.LL_AcknowledgePaint(token);
			return;
		}
		SCR_PlayerController owner = SCR_PlayerController.Cast(sender.GetOwner());
		if (!owner)
		{
			sender.LL_AcknowledgePaint(token);
			return;
		}
		string factionKey = LL_GetPlayerPaintFactionKey(owner.GetPlayerId());
		if (factionKey.IsEmpty() || factionKey != requestedFactionKey)
		{
			sender.LL_AcknowledgePaint(token);
			return;
		}
		LL_MapPaintReplica state = LL_GetPaintStateForFaction(factionKey);
		if (!state)
		{
			sender.LL_AcknowledgePaint(token);
			return;
		}
		int ownerId = owner.GetPlayerId();
		color = LL_MapPaintProtocol.NormalizeColor(kind, color);
		array<int> erasedOwners;
		if (kind == LL_EMapPaintOperation.ERASE)
			erasedOwners = {};
		if (!LL_MapPaintProtocol.Apply(state.m_Document, kind, size, color, points, ownerId, erasedOwners))
		{
			sender.LL_AcknowledgePaint(token);
			return;
		}
		LL_RecordPaintHistory(factionKey, ownerId, kind, erasedOwners);
		state.AdvanceAuthority();
		SCR_Faction faction = SCR_Faction.Cast(SCR_FactionManager.SGetPlayerFaction(ownerId));
		array<int> factionPlayers = {};
		if (faction)
			faction.GetPlayersInFaction(factionPlayers);
		PlayerManager playerManager = GetGame().GetPlayerManager();
		foreach (int playerId : factionPlayers)
		{
			SCR_PlayerController playerController = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
			if (!playerController)
				continue;
			SCR_MapMarkerSyncComponent recipient = SCR_MapMarkerSyncComponent.Cast(playerController.FindComponent(SCR_MapMarkerSyncComponent));
			if (recipient)
				recipient.LL_SendPaintDelta(factionKey, state.GetRevision(), ownerId, token, kind, size, color, points);
		}
		// A listen-server player's copy is already authoritative.
		if (sender == SCR_MapMarkerSyncComponent.LL_GetLocalPaintSync())
			sender.LL_ResolvePaintPreview(token);
	}

	//! Snapshot calls are queued synchronously at one revision; live deltas are
	//! buffered client-side until the owner-only snapshot has finished.
	void LL_ServerPaintSnapshot_S(SCR_MapMarkerSyncComponent target, int request)
	{
		if (RplSession.Mode() == RplMode.Client)
			return;
		SCR_PlayerController owner = SCR_PlayerController.Cast(target.GetOwner());
		if (!owner)
			return;
		LL_MapPaintReplica state = LL_GetPaintStateForFaction(LL_GetPlayerPaintFactionKey(owner.GetPlayerId()));
		if (!state)
			return;
		target.LL_SendPaintSnapshotBegin(request, state.GetRevision());
		foreach (LL_MapPaintStroke stroke : state.m_Document.m_Strokes)
		{
			for (int start = 0; start < stroke.m_Points.Count(); start += LL_MapPaintProtocol.BATCH_POINTS)
			{
				array<vector> points = {};
				int end = Math.Min(start + LL_MapPaintProtocol.BATCH_POINTS, stroke.m_Points.Count());
				for (int i = start; i < end; i++)
					points.Insert(stroke.m_Points[i]);
				target.LL_SendPaintSnapshotChunk(request, start == 0, stroke.m_fWidth, stroke.m_iColor, points, stroke.m_iOwner, stroke.m_bAnchorLine);
			}
		}
		target.LL_SendPaintSnapshotEnd(request);
	}


	static bool LL_IsKnownStaticMarker(SCR_MapMarkerBase incoming, array<ref SCR_MapMarkerBase> activeMarkers, array<ref SCR_MapMarkerBase> disabledMarkers)
	{
		if (!incoming)
			return false;

		int markerID = incoming.GetMarkerID();
		foreach (SCR_MapMarkerBase activeMarker : activeMarkers)
		{
			if (activeMarker && (activeMarker == incoming || (markerID >= 0 && activeMarker.GetMarkerID() == markerID)))
				return true;
		}
		foreach (SCR_MapMarkerBase disabledMarker : disabledMarkers)
		{
			if (disabledMarker && (disabledMarker == incoming || (markerID >= 0 && disabledMarker.GetMarkerID() == markerID)))
				return true;
		}
		return false;
	}

	static array<ref SCR_MapMarkerBase> LL_GetStaticMarkerSnapshot(array<ref SCR_MapMarkerBase> activeMarkers, array<ref SCR_MapMarkerBase> disabledMarkers)
	{
		array<ref SCR_MapMarkerBase> snapshot = {};
		foreach (SCR_MapMarkerBase activeMarker : activeMarkers)
			snapshot.Insert(activeMarker);
		foreach (SCR_MapMarkerBase disabledMarker : disabledMarkers)
			snapshot.Insert(disabledMarker);
		return snapshot;
	}



	override protected void CheckMarkersUserRestrictions()
	{
		SocialComponent social = GetSocialComponent();
		if (!social || System.IsConsoleApp())
			return;

		// SetStaticMarkerDisabled moves entries between both live arrays. Hold references
		// to the original entries instead of indexing those arrays during this update.
		array<ref SCR_MapMarkerBase> snapshot = LL_GetStaticMarkerSnapshot(m_aStaticMarkers, m_aDisabledMarkers);
		bool contentAllowed = SocialComponent.IsPrivilegedTo(EUserInteraction.UserGeneratedContent);
		foreach (SCR_MapMarkerBase marker : snapshot)
		{
			if (!marker)
				continue;

			bool blocked = !contentAllowed || social.IsRestricted(marker.GetMarkerOwnerID(), EUserInteraction.UserGeneratedContent);
			// A synchronous callback may have removed this marker since the snapshot was taken.
			if (!m_aStaticMarkers.Contains(marker) && !m_aDisabledMarkers.Contains(marker))
				continue;

			marker.SetBlocked(blocked);
			SetStaticMarkerDisabled(marker, !marker.TestVisibleFrame(m_vVisibleFrameMin, m_vVisibleFrameMax));
		}
	}
}
