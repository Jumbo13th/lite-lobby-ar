// Server-side gate for player-placed marker broadcasts. A player without a slot has no
// engine faction, so their markers would carry faction flags 0 and reach both sides; a
// player whose character died is a spectator and must not feed or remove intel.

modded class SCR_MapMarkerSyncComponent
{
	// Vanilla's per-player cap is 10 and the mission header is read in the stock
	// OnPostInit, so the value is set after super. Must stay positive: <= 0 disables
	// placement. Markers persist across disconnects, so this also bounds accumulation.
	// Delete by anyone lets a faction clear a flood itself; RPC_AskRemoveStaticMarker adds the
	// faction gate the stock path lacks.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_iPlacedMarkerLimit = 100;
		m_bIsDeleteRestricted = false;
	}

	// A faction (slot taken) and a living slot character.
	bool LL_CanEditSharedMarkers()
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

	// Any placed marker of the requester's faction. The stock RPC refuses server-owned (restored)
	// markers, so those are removed the way the stock RemoveStaticMarker does on the authority.
	override protected void RPC_AskRemoveStaticMarker(int markerID)
	{
		if (!LL_CanEditSharedMarkers())
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
			return;

		SCR_MapMarkerBase marker = markerMgr.GetStaticMarkerByID(markerID);
		if (!marker)
			marker = markerMgr.GetDisabledMarkerByID(markerID);
		if (!marker)
			return;

		SCR_EMapMarkerType type = marker.GetType();
		if (type != SCR_EMapMarkerType.PLACED_CUSTOM && type != SCR_EMapMarkerType.PLACED_MILITARY)
			return;

		int requesterId = SCR_PlayerController.Cast(GetOwner()).GetPlayerId();
		Faction requesterFaction = SCR_FactionManager.SGetPlayerFaction(requesterId);
		FactionManager factionManager = GetGame().GetFactionManager();
		if (marker.GetMarkerFactionFlags() != 0 && (!requesterFaction || !factionManager || !marker.IsFaction(factionManager.GetFactionIndex(requesterFaction))))
			return;

		LL_LogMarkerRemoval(marker, requesterId, requesterFaction);

		if (marker.GetMarkerOwnerID() == -1)
		{
			markerMgr.OnRemoveSynchedMarker(markerID);
			markerMgr.OnAskRemoveStaticMarker(markerID);
			return;
		}

		super.RPC_AskRemoveStaticMarker(markerID);
	}

	protected void LL_LogMarkerRemoval(notnull SCR_MapMarkerBase marker, int requesterId, Faction requesterFaction)
	{
		int ownerId = marker.GetMarkerOwnerID();
		string owner = "server (restored)";
		if (ownerId != -1)
		{
			owner =GetGame().GetPlayerManager().GetPlayerName(ownerId);
			if (owner.IsEmpty())
				owner = "player " + ownerId.ToString();
		}

		string factionKey;
		if (requesterFaction)
			factionKey = requesterFaction.GetFactionKey();

		int pos[2];
		marker.GetWorldPos(pos);

		Print(string.Format("[LL_MapAudit] Marker %1 by %2 ('%3' at %4 %5) deleted by %6 (player %7, %8)",
			marker.GetMarkerID(), owner, marker.GetCustomText(), pos[0], pos[1],
			GetGame().GetPlayerManager().GetPlayerName(requesterId), requesterId, factionKey), LogLevel.NORMAL);
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
}
