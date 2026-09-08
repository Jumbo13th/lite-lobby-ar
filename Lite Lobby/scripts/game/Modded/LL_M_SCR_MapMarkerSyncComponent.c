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
}
