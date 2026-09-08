// Draws a polygon zone on the map from a ShapeEntity, gated by faction and game state.
// No replication: the shape is baked into the world and each client draws its own map.

class LL_MapZoneComponentClass : ScriptComponentClass
{
}

class LL_MapZoneComponent : ScriptComponent
{
	[Attribute("{1D10A1B2C3D4E5F6}UI/Map/ZoneCanvas.layout", UIWidgets.ResourceNamePicker, "Canvas layout the zone is drawn into.", params: "layout", category: "Lite Lobby")]
	protected ResourceName m_sCanvasLayout;

	[Attribute("0.761 0.392 0.078 0.18", UIWidgets.ColorPicker, "Fill color.", category: "Lite Lobby")]
	protected ref Color m_FillColor;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional fill texture.", params: "edds", category: "Lite Lobby")]
	protected ResourceName m_sFillTexture;

	[Attribute("0.05", UIWidgets.EditBox, "Fill texture UV scale.", category: "Lite Lobby")]
	protected float m_fFillUVScale;

	[Attribute("0.761 0.392 0.078 1", UIWidgets.ColorPicker, "Outline color.", category: "Lite Lobby")]
	protected ref Color m_OutlineColor;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional outline texture.", params: "edds", category: "Lite Lobby")]
	protected ResourceName m_sOutlineTexture;

	[Attribute("1 0.01 0", UIWidgets.EditBox, "Outline texture UV scale (XY).", category: "Lite Lobby")]
	protected vector m_OutlineUVScale;

	[Attribute("4", UIWidgets.Slider, "Outline thickness.", params: "1 50 0.1", category: "Lite Lobby")]
	protected float m_fOutlineWidth;

	[Attribute("0", UIWidgets.CheckBox, "Draw an open line instead of a closed, filled zone (uses the shape's points as-is, no fill, ends not joined).", category: "Lite Lobby")]
	protected bool m_bLineMode;

	[Attribute("1", UIWidgets.CheckBox, "Visible to everyone. Disable to restrict to the faction list below.", category: "Lite Lobby")]
	protected bool m_bShowForAnyFaction;

	[Attribute("", UIWidgets.Auto, "Faction keys that may see this zone on the map (used when 'show for all' is off).", category: "Lite Lobby")]
	protected ref array<FactionKey> m_aVisibleForFactions;

	[Attribute("0", UIWidgets.ComboBox, "Game-mode states in which the zone is hidden on the map.", "", ParamEnumArray.FromEnum(SCR_EGameModeState), category: "Lite Lobby")]
	protected ref array<SCR_EGameModeState> m_aHideOnGameModeStates;

	[Attribute("0", UIWidgets.CheckBox, "Hide this zone from the map once the post-start freeze countdown ends (pair with LL_FreezeZoneComponent).", category: "Lite Lobby")]
	protected bool m_bHideAfterFreezeTime;

	protected ShapeEntity m_Shape;
	protected SCR_MapEntity m_MapEntity;
	protected CanvasWidget m_wCanvas;
	protected ref array<vector> m_aWorldPoints = {};

	protected ref PolygonDrawCommand m_FillCommand = new PolygonDrawCommand();
	protected ref LineDrawCommand m_OutlineCommand = new LineDrawCommand();
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Shape = ShapeEntity.Cast(owner);
		if (!m_Shape)
			return;

		if (!m_aVisibleForFactions)
			m_aVisibleForFactions = {};
		if (!m_FillColor)
			m_FillColor = new Color(1, 1, 1, 0.18);
		if (!m_OutlineColor)
			m_OutlineColor = new Color(1, 1, 1, 1);

		m_MapEntity = SCR_MapEntity.GetMapInstance();
		if (!m_MapEntity)
			return;

		m_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		m_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	// Show-for-all includes players without a slot; otherwise a player with no faction
	// never sees it. Also honours the game-stage gate.
	protected bool IsVisibleForLocalPlayer()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && m_aHideOnGameModeStates && m_aHideOnGameModeStates.Contains(gameMode.GetState()))
			return false;

		// The countdown also reads 0 before the game starts, so GAME is required too.
		if (m_bHideAfterFreezeTime)
		{
			LL_GameModeCoop coopMode = LL_GameModeCoop.Cast(GetGame().GetGameMode());
			if (coopMode && coopMode.GetLobbyState() == SCR_EGameModeState.GAME && coopMode.GetFreezeTimeRemaining() <= 0)
				return false;
		}

		if (m_bShowForAnyFaction)
			return true;

		FactionKey factionKey = GetLocalFactionKey();
		if (factionKey == "")
			return false;

		return m_aVisibleForFactions.Contains(factionKey);
	}

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

	void OnMapOpen(MapConfiguration config)
	{
		if (!IsVisibleForLocalPlayer())
			return;

		array<vector> pts = {};
		m_Shape.GetPointsPositions(pts);
		if (pts.Count() < 2)
			return;

		m_aWorldPoints.Clear();
		vector origin = m_Shape.GetOrigin();
		foreach (vector p : pts)
			m_aWorldPoints.Insert(p + origin);

		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = m_MapEntity.GetMapMenuRoot();
		if (!mapFrame)
			return;

		Widget layoutRoot = GetGame().GetWorkspace().CreateWidgets(m_sCanvasLayout, mapFrame);
		if (!layoutRoot)
			return;

		m_wCanvas = CanvasWidget.Cast(layoutRoot);
		if (!m_wCanvas)
			m_wCanvas = CanvasWidget.Cast(layoutRoot.FindAnyWidget("Canvas"));
		if (!m_wCanvas)
			return;

		if (m_sFillTexture != "")
		{
			m_FillCommand.m_pTexture = m_wCanvas.LoadTexture(m_sFillTexture);
			m_FillCommand.m_fUVScale = m_fFillUVScale;
		}
		m_FillCommand.m_iColor = m_FillColor.PackToInt();

		if (m_sOutlineTexture != "")
		{
			m_OutlineCommand.m_pTexture = m_wCanvas.LoadTexture(m_sOutlineTexture);
			m_OutlineCommand.m_UVScale = m_OutlineUVScale;
		}
		m_OutlineCommand.m_fWidth = m_fOutlineWidth;
		m_OutlineCommand.m_iColor = m_OutlineColor.PackToInt();
		// A closed zone joins the last point back to the first.
		m_OutlineCommand.m_bShouldEnclose = !m_bLineMode;

		SetEventMask(GetOwner(), EntityEvent.POSTFRAME);
	}

	void OnMapClose(MapConfiguration config)
	{
		ClearEventMask(GetOwner(), EntityEvent.POSTFRAME);
		if (m_wCanvas)
		{
			m_wCanvas.RemoveFromHierarchy();
			m_wCanvas = null;
		}
	}

	override void EOnPostFrame(IEntity owner, float timeSlice)
	{
		if (!m_wCanvas || m_aWorldPoints.Count() < 2)
			return;

		// Freeze may have ended or the faction changed while the map stayed open.
		if (!IsVisibleForLocalPlayer())
		{
			OnMapClose(null);
			return;
		}

		array<float> screen = ProjectPoints(m_aWorldPoints);

		m_aCommands.Clear();
		if (!m_bLineMode)
		{
			m_FillCommand.m_Vertices = screen;
			m_aCommands.Insert(m_FillCommand);
		}
		m_OutlineCommand.m_Vertices = screen;
		m_aCommands.Insert(m_OutlineCommand);
		m_wCanvas.SetDrawCommands(m_aCommands);
	}

	protected array<float> ProjectPoints(array<vector> worldPoints)
	{
		array<float> screen = {};
		foreach (vector wp : worldPoints)
		{
			float sx, sy;
			m_MapEntity.WorldToScreen(wp[0], wp[2], sx, sy, true);
			screen.Insert(sx);
			screen.Insert(sy);
		}
		return screen;
	}

	void ~LL_MapZoneComponent()
	{
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
			m_MapEntity.GetOnMapClose().Remove(OnMapClose);
		}
	}
}