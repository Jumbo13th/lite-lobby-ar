// A static map icon placed in the world by the mission maker. No replication: the
// attributes are baked into the world file, and each client reads its own faction to
// decide whether to draw it.

class LL_ManualMarkerClass : GenericEntityClass
{
}

class LL_ManualMarker : GenericEntity
{
	[Attribute("{E23427CAC80DA8B7}UI/Textures/Icons/icons_mapMarkersUI.imageset", UIWidgets.ResourceNamePicker, "Icon imageset (or a direct .edds texture).", category: "Lite Lobby")]
	protected ResourceName m_sImageSet;

	[Attribute("circle-2", UIWidgets.EditBox, "Quad (sprite) name inside the imageset.", category: "Lite Lobby")]
	protected string m_sQuadName;

	[Attribute("1 1 1 1", UIWidgets.ColorPicker, "Icon tint.", category: "Lite Lobby")]
	protected ref Color m_MarkerColor;

	[Attribute("5.0", UIWidgets.EditBox, "Marker size. Meters when 'Use world scale' is on (scales with zoom), pixels otherwise.", category: "Lite Lobby")]
	protected float m_fWorldSize;

	[Attribute("1", UIWidgets.CheckBox, "Size in meters that scales with map zoom (on) vs a fixed pixel size (off).", category: "Lite Lobby")]
	protected bool m_bUseWorldScale;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBoxMultiline, desc: "Hover tooltip text. Supports rich text markup.", category: "Lite Lobby")]
	protected string m_sDescription;

	[Attribute("0", UIWidgets.EditBox, "Draw order; higher draws on top.", category: "Lite Lobby")]
	protected int m_iZOrder;

	[Attribute("0", UIWidgets.ComboBox, "Lobby states in which this marker is hidden (e.g. hide during briefing, show in game).", "", ParamEnumArray.FromEnum(SCR_EGameModeState), category: "Lite Lobby")]
	protected ref array<SCR_EGameModeState> m_aHideOnGameModeStates;

	[Attribute("1", UIWidgets.CheckBox, "Visible to everyone — all factions, and players who haven't picked a slot yet. Disable to restrict to the faction list below.", category: "Lite Lobby")]
	protected bool m_bShowForAnyFaction;

	[Attribute("", UIWidgets.Auto, "Faction keys that may see this marker (used when 'show for all' is off). A player with no slot has no faction to match, so they never see a restricted marker.", category: "Lite Lobby")]
	protected ref array<FactionKey> m_aVisibleForFactions;

	protected Widget m_wRoot;
	protected SCR_MapEntity m_MapEntity;
	protected LL_ManualMarkerComponent m_hManualMarkerComponent;
	protected ResourceName m_sMarkerLayout = "{1C0F8A2B3C4D5E6F}UI/Map/ManualMapMarkerBase.layout";

	// Show-for-all includes players without a slot; otherwise a player with no faction
	// never sees a restricted marker.
	bool IsVisibleFor(FactionKey factionKey)
	{
		if (m_bShowForAnyFaction)
			return true;

		if (factionKey == "")
			return false;

		return m_aVisibleForFactions && m_aVisibleForFactions.Contains(factionKey);
	}

	// Game-stage gate plus the faction rule.
	bool IsCurrentlyVisible()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && m_aHideOnGameModeStates && m_aHideOnGameModeStates.Contains(gameMode.GetState()))
			return false;

		return IsVisibleFor(GetLocalFactionKey());
	}

	// The engine player-level faction, which the lobby keeps in sync with the slot.
	protected FactionKey GetLocalFactionKey()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return "";

		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(
			pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation)
			return "";

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			return "";

		return faction.GetFactionKey();
	}

	override protected void EOnPostFrame(IEntity owner, float timeSlice)
	{
		if (!m_wRoot || !m_hManualMarkerComponent || !m_MapEntity)
			return;

		float wX, wY, screenX, screenY, screenXEnd, screenYEnd;
		vector worldPosition = GetOrigin();
		wX = worldPosition[0];
		wY = worldPosition[2];
		m_MapEntity.WorldToScreen(wX, wY, screenX, screenY, true);
		m_MapEntity.WorldToScreen(wX + m_fWorldSize, wY + m_fWorldSize, screenXEnd, screenYEnd, true);

		float screenXD = GetGame().GetWorkspace().DPIUnscale(screenX);
		float screenYD = GetGame().GetWorkspace().DPIUnscale(screenY);
		float sizeXD = m_fWorldSize;
		float sizeYD = m_fWorldSize;
		if (m_bUseWorldScale)
		{
			sizeXD = GetGame().GetWorkspace().DPIUnscale(screenXEnd - screenX);
			sizeYD = GetGame().GetWorkspace().DPIUnscale(screenY - screenYEnd);
		}
		sizeYD *= m_hManualMarkerComponent.GetYScale();

		// Default marker art points right.
		m_hManualMarkerComponent.SetSlot(screenXD, screenYD, sizeXD, sizeYD, GetYawPitchRoll()[0] - 90);
	}

	void CreateMapWidget(MapConfiguration mapConfig)
	{
		if (m_wRoot)
			return;

		if (!IsCurrentlyVisible())
			return;

		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = m_MapEntity.GetMapMenuRoot();
		if (!mapFrame)
			return;

		m_wRoot = GetGame().GetWorkspace().CreateWidgets(m_sMarkerLayout, mapFrame);
		if (!m_wRoot)
			return;

		m_wRoot.SetZOrder(m_iZOrder);
		m_hManualMarkerComponent = LL_ManualMarkerComponent.Cast(m_wRoot.FindHandler(LL_ManualMarkerComponent));
		if (!m_hManualMarkerComponent)
		{
			DeleteMapWidget(mapConfig);
			return;
		}

		m_hManualMarkerComponent.SetImage(m_sImageSet, m_sQuadName);
		m_hManualMarkerComponent.SetDescription(m_sDescription);
		m_hManualMarkerComponent.SetColor(m_MarkerColor);
		m_hManualMarkerComponent.OnMouseLeave(null, null, 0, 0);

		SetEventMask(EntityEvent.POSTFRAME);
	}

	void DeleteMapWidget(MapConfiguration mapConfig)
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		m_hManualMarkerComponent = null;

		ClearEventMask(EntityEvent.POSTFRAME);
	}

	void LL_ManualMarker(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	override protected void EOnInit(IEntity owner)
	{
		if (!m_aVisibleForFactions)
			m_aVisibleForFactions = new array<FactionKey>();

		m_MapEntity = SCR_MapEntity.GetMapInstance();
		if (!m_MapEntity)
			return;

		m_MapEntity.GetOnMapOpen().Insert(CreateMapWidget);
		m_MapEntity.GetOnMapClose().Insert(DeleteMapWidget);
	}
}