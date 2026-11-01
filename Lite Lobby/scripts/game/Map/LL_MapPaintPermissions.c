//! Shared marker permissions and drawing synchronization readiness.
class LL_MapPaintPermissions
{
	static bool IsPaintAllowed(SCR_MapEntity mapEntity, SCR_MapCursorModule cursor, SCR_MapMarkerSyncComponent sync, bool open, SCR_MapMarkerManagerComponent manager)
	{
		if (!open || !mapEntity || !cursor || mapEntity.GetMapConfig().MapEntityMode == EMapEntityMode.SPAWNSCREEN)
			return false;

		if (!GetPaintReadinessStatus(manager, sync).IsEmpty())
			return false;

		int restrictedCursorState = SCR_MapCursorModule.STATE_DRAW_RESTRICTED | EMapCursorState.CS_ROTATE;
		if (cursor.GetCursorState() == EMapCursorState.CS_DISABLE || (cursor.GetCursorState() & restrictedCursorState))
			return false;

		return CanPlaceMarkers(mapEntity, manager);
	}

	static bool CanPlaceMarkers(SCR_MapEntity mapEntity, SCR_MapMarkerManagerComponent manager)
	{
		SCR_MapMarkersUI markersUI = SCR_MapMarkersUI.Cast(mapEntity.GetMapUIComponent(SCR_MapMarkersUI));
		LL_MapMarkersUI lobbyMarkers = LL_MapMarkersUI.Cast(markersUI);
		if (LL_SpectatorMenu.IsMapOpen() || LL_GameModeCoop.IsHardFreezeActive() || (lobbyMarkers && !lobbyMarkers.LL_CanEditMarkers()))
			return false;

		// Workbench without a game-mode marker manager uses the local paint document.
		if (!manager)
			return RplSession.Mode() != RplMode.Client;

		if (!markersUI || markersUI.IsConfigDisabled())
			return false;

		SCR_MapMarkerConfig markerConfig = manager.GetMarkerConfig();
		if (!markerConfig)
			return false;

		bool hasPlaceableMarkerType = markerConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM)
			|| markerConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_MILITARY)
			|| markerConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_RECON);
		if (!hasPlaceableMarkerType)
			return false;

		// Vanilla disables public marker placement when the local user lacks this privilege.
		return SocialComponent.IsPrivilegedTo(EUserInteraction.UserGeneratedContent);
	}



	static string GetPaintReadinessStatus(SCR_MapMarkerManagerComponent manager, SCR_MapMarkerSyncComponent sync)
	{
		if (RplSession.Mode() != RplMode.Client)
		{
			if (!manager)
				return string.Empty; // Workbench preview uses the map entity's local document.

			LL_MapPaintReplica authorityState = manager.LL_GetPaintState();
			if (!authorityState || !authorityState.IsReady())
				return "#LL-MapPaint_NotReady";

			return string.Empty;
		}

		if (!manager)
			return "#LL-MapPaint_NoManager";

		if (manager.LL_GetLocalPaintFactionKey().IsEmpty())
			return "#LL-MapPaint_NoFaction";

		if (!sync)
			return "#LL-MapPaint_NoSync";

		LL_MapPaintReplica state = manager.LL_GetPaintState();
		if (!state || !state.IsReady())
			return "#LL-MapPaint_Waiting";

		return string.Empty;
	}

	static bool IsLocalPaintFallback(SCR_MapMarkerManagerComponent manager, SCR_MapMarkerSyncComponent sync)
	{
		if (RplSession.Mode() == RplMode.Client)
			return false;

		return !manager || !sync || manager.LL_GetLocalPaintFactionKey().IsEmpty();
	}
}
