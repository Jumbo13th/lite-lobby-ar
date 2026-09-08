// In-game fullscreen map screen (GAME): the briefing map trimmed to map plus mission
// description panel. The map key opens it instead of the engine map while the round is
// live (LL_M_SCR_MapMenuUI).

class LL_GameMapMenu : MenuBase
{
	protected ResourceName m_sMapConfig = "{DC07E80F6BB397EF}Configs/Map/LL_MapBriefing.conf";

	protected SCR_MapEntity m_MapEntity;
	protected InputManager m_InputManager;

	// The character that had the map raised; the map closes if it is lost or swapped.
	protected SCR_PlayerController m_PlayerController;
	protected IEntity m_MapCharacter;

	protected Widget m_wMissionDescriptionPanel;
	protected SCR_InputButtonComponent m_NavigationCloseMapComp;
	protected SCR_InputButtonComponent m_NavigationDescriptionComp;

	protected bool m_bSwitchDescriptionQueued;

	override void OnMenuInit()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}

	override void OnMenuOpen()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Close();
			return;
		}

		m_InputManager = GetGame().GetInputManager();
		Widget root = GetRootWidget();

		m_wMissionDescriptionPanel = root.FindAnyWidget("MissionDescriptionPanel");

		// The map key closes this screen; the description key fires both its button and
		// the listener, hence the debounce.
		Widget wNavCloseMap = root.FindAnyWidget("NavigationCloseMap");
		if (wNavCloseMap)
			m_NavigationCloseMapComp = SCR_InputButtonComponent.Cast(wNavCloseMap.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationCloseMapComp)
			m_NavigationCloseMapComp.m_OnActivated.Insert(Action_Close);

		Widget wNavDescription = root.FindAnyWidget("NavigationDescription");
		if (wNavDescription)
			m_NavigationDescriptionComp = SCR_InputButtonComponent.Cast(wNavDescription.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationDescriptionComp)
			m_NavigationDescriptionComp.m_OnActivated.Insert(Action_SwitchDescription);

		m_InputManager.AddActionListener("LL_SwitchDescription", EActionTrigger.DOWN, Action_SwitchDescription);

		// Esc closes the map too; no footer hint for it.
		m_InputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, Action_Close);

		// The gadget that raised the map is gone with the character.
		m_PlayerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (m_PlayerController)
		{
			m_MapCharacter = m_PlayerController.GetControlledEntity();
			m_PlayerController.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);
		}

		// The map fires these once OpenMap has attached and laid out the frame.
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapOpen().Insert(OnMapOpenedMarkers);
			m_MapEntity.GetOnMapClose().Insert(OnMapClosedMarkers);
		}

		// The map widget tree needs a couple of frames before SCR_MapEntity can attach.
		if (m_MapEntity)
			GetGame().GetCallqueue().CallLater(OpenMapStep1, 0);
		else
			Print("[LL_Lobby] GameMap: no SCR_MapEntity in world — map disabled", LogLevel.WARNING);
	}

	override void OnMenuClose()
	{
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapOpen().Remove(OnMapOpenedMarkers);
			m_MapEntity.GetOnMapClose().Remove(OnMapClosedMarkers);
		}
		LL_SpawnMarkers squadMarkers = LL_SpawnMarkers.GetInstance();
		if (squadMarkers)
			squadMarkers.CloseOnMap();

		LL_TrackerManager trackers = LL_TrackerManager.GetInstance();
		if (trackers)
			trackers.CloseOnMap();

		if (m_MapEntity)
			m_MapEntity.CloseMap();

		if (m_NavigationCloseMapComp)
			m_NavigationCloseMapComp.m_OnActivated.Remove(Action_Close);
		if (m_NavigationDescriptionComp)
			m_NavigationDescriptionComp.m_OnActivated.Remove(Action_SwitchDescription);

		if (m_InputManager)
		{
			m_InputManager.RemoveActionListener("LL_SwitchDescription", EActionTrigger.DOWN, Action_SwitchDescription);
			m_InputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, Action_Close);
		}

		if (m_PlayerController)
			m_PlayerController.m_OnControlledEntityChanged.Remove(OnControlledEntityChanged);

		// Closing this menu does not lower the raised map gadget; the character would
		// stay frozen holding it up.
		if (m_MapCharacter)
		{
			SCR_GadgetManagerComponent gadgetMgr = SCR_GadgetManagerComponent.GetGadgetManager(m_MapCharacter);
			if (gadgetMgr)
				gadgetMgr.RemoveHeldGadget();
		}
	}

	override void OnMenuUpdate(float tDelta)
	{
		LL_SpawnMarkers squadMarkers = LL_SpawnMarkers.GetInstance();
		if (squadMarkers)
			squadMarkers.UpdateOnMap(m_MapEntity);

		LL_TrackerManager trackers = LL_TrackerManager.GetInstance();
		if (trackers)
			trackers.UpdateOnMap(m_MapEntity);

		// Polled: the corpse stays the controlled entity, so no entity-changed event fires
		// on death.
		if (!m_MapCharacter)
			return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(
			m_MapCharacter.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
			Close();
	}

	protected void OpenMapStep1()
	{
		// SetupMapConfig reads the layout's MapFrame, laid out two frames after menu open.
		GetGame().GetCallqueue().CallLater(OpenMapStep2, 0);
	}

	protected void OpenMapStep2()
	{
		if (!m_MapEntity)
			return;

		MapConfiguration mapConfig = m_MapEntity.SetupMapConfig(EMapEntityMode.FULLSCREEN, m_sMapConfig, GetRootWidget());
		if (!mapConfig)
		{
			Print("[LL_Lobby] GameMap: map config failed to load", LogLevel.WARNING);
			return;
		}

		m_MapEntity.OpenMap(mapConfig);
	}

	protected void Action_Close()
	{
		Close();
	}

	// Death is polled in OnMenuUpdate; this covers team switch and unpossess.
	protected void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		Close();
	}

	// Debounced: the key can fire two listeners in one frame.
	protected void Action_SwitchDescription()
	{
		if (m_bSwitchDescriptionQueued)
			return;

		m_bSwitchDescriptionQueued = true;
		GetGame().GetCallqueue().CallLater(SwitchDescriptionImpl, 0, false);
	}

	protected void SwitchDescriptionImpl()
	{
		m_bSwitchDescriptionQueued = false;

		if (m_wMissionDescriptionPanel)
			m_wMissionDescriptionPanel.SetVisible(!m_wMissionDescriptionPanel.IsVisible());
	}

	protected void OnMapOpenedMarkers(MapConfiguration config)
	{
		LL_SpawnMarkers markers = LL_SpawnMarkers.GetInstance();
		if (markers)
			markers.OpenOnMap(m_MapEntity);

		LL_TrackerManager trackers = LL_TrackerManager.GetInstance();
		if (trackers)
			trackers.OpenOnMap(m_MapEntity);
	}

	protected void OnMapClosedMarkers(MapConfiguration config)
	{
		LL_SpawnMarkers markers = LL_SpawnMarkers.GetInstance();
		if (markers)
			markers.CloseOnMap();

		LL_TrackerManager trackers = LL_TrackerManager.GetInstance();
		if (trackers)
			trackers.CloseOnMap();
	}
}