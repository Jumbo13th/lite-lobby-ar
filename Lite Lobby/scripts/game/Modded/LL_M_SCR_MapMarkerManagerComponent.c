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
}