// Fullscreen map layer of the spectator screen (M). Draws into the MapFrame widget,
// which sits first in the layout so labels and panels stay usable over it. The character
// icons are the menu's per-slot label widgets switched to map projection. Costs nothing
// on the network: positions come from entities the client already has. Player icons of
// all factions are shown (the alive list already lists every slot); other factions'
// placed markers are dropped upstream; spawn markers and trackers are not wired here.

class LL_SpectatorMap
{
	// The same config the briefing and in-game maps use.
	protected static const ResourceName MAP_CONFIG = "{DC07E80F6BB397EF}Configs/Map/LL_MapBriefing.conf";

	protected SCR_MapEntity m_MapEntity;
	protected Widget m_wMapFrame;
	protected Widget m_wMenuRoot;

	// Not part of the vanilla Map.layout include; SCR_MapToolMenuUI resolves it by name
	// from the menu root and dereferences it without a null check. A sibling of the map
	// frame so its visibility is ours to drive.
	protected Widget m_wToolMenu;

	// Held hidden by EnforceToolsHidden.
	protected Widget m_wRulerFrame;
	protected Widget m_wCompassFrame;
	protected Widget m_wWatchFrame;

	// Tracked here rather than read from SCR_MapEntity.IsOpen: the map takes two frames
	// to attach, and the camera lock and input context switch on the key press.
	protected bool m_bOpen;
	protected bool m_bDiagnosed;

	//! False when the screen has no MapFrame or the world has no map entity.
	bool Init(notnull Widget menuRoot)
	{
		m_wMenuRoot = menuRoot;
		m_wMapFrame = menuRoot.FindAnyWidget("MapFrame");
		m_wToolMenu = menuRoot.FindAnyWidget("ToolMenu");
		m_MapEntity = SCR_MapEntity.GetMapInstance();

		if (!m_wMapFrame)
			return Diagnose("MapFrame widget not found — reimport UI/Spectator/SpectatorMenu.layout");

		if (!m_wToolMenu)
			return Diagnose("ToolMenu widget not found — reimport UI/Spectator/SpectatorMenu.layout (SCR_MapToolMenuUI would crash without it)");

		if (!m_MapEntity)
			return Diagnose("no SCR_MapEntity in world — spectator map disabled");

		m_wRulerFrame = m_wMapFrame.FindAnyWidget("RulerFrame");
		m_wCompassFrame = m_wMapFrame.FindAnyWidget("CompassFrame");
		m_wWatchFrame = m_wMapFrame.FindAnyWidget("WatchFrame");

		SetMapFrameVisible(false);
		return true;
	}

	bool IsOpen()
	{
		return m_bOpen;
	}

	bool IsAvailable()
	{
		return m_wMapFrame && m_MapEntity;
	}

	void Toggle()
	{
		if (m_bOpen)
			Close();
		else
			Open();
	}

	void Open()
	{
		if (m_bOpen || !IsAvailable())
			return;

		m_bOpen = true;
		SetMapFrameVisible(true);

		// SetupMapConfig reads the layout's MapFrame, which is laid out two frames after
		// the frame is shown.
		GetGame().GetCallqueue().CallLater(OpenStep1, 0);
	}

	void Close()
	{
		if (!m_bOpen)
			return;

		m_bOpen = false;

		// The open chain may still be in flight.
		GetGame().GetCallqueue().Remove(OpenStep1);
		GetGame().GetCallqueue().Remove(OpenStep2);

		if (m_MapEntity)
			m_MapEntity.CloseMap();

		SetMapFrameVisible(false);
	}

	//! Holds the map tools hidden, every frame: Map.layout leaves two frames visible,
	//! components restore themselves in OnMapOpen after this subscriber, SCR_MapRTWBaseUI
	//! reveals its frame a frame later, and PopulateToolMenu re-shows the tool bar on every
	//! rebuild. When writers disagree, the one that runs every frame wins.
	void EnforceToolsHidden()
	{
		HideIfVisible(m_wRulerFrame);
		HideIfVisible(m_wCompassFrame);
		HideIfVisible(m_wWatchFrame);
		HideIfVisible(m_wToolMenu);
	}

	protected void HideIfVisible(Widget w)
	{
		if (w && w.IsVisible())
			w.SetVisible(false);
	}

	protected void SetMapFrameVisible(bool visible)
	{
		if (m_wMapFrame)
			m_wMapFrame.SetVisible(visible);
	}

	//! The map entity outlives this screen.
	void Shutdown()
	{
		Close();
	}

	SCR_MapEntity GetMapEntity()
	{
		return m_MapEntity;
	}

	protected void OpenStep1()
	{
		GetGame().GetCallqueue().CallLater(OpenStep2, 0);
	}

	protected void OpenStep2()
	{
		if (!m_bOpen || !m_MapEntity)
			return;

		MapConfiguration mapConfig = m_MapEntity.SetupMapConfig(EMapEntityMode.FULLSCREEN, MAP_CONFIG, m_wMenuRoot);
		if (!mapConfig)
		{
			Diagnose("map config failed to load — spectator map disabled");
			Close();
			return;
		}

		m_MapEntity.OpenMap(mapConfig);
	}

	protected bool Diagnose(string reason)
	{
		if (!m_bDiagnosed)
		{
			m_bDiagnosed = true;
			Print("[LL_Spectator] Map: " + reason, LogLevel.WARNING);
		}

		return false;
	}
}