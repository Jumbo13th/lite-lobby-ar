// Client-side spectator mode: a local free-fly camera and the spectator menu, entered
// when the server says this player should spectate. No replication of its own.

class LL_SpectatorManagerClass : SCR_BaseGameModeComponentClass
{
}

class LL_SpectatorManager : SCR_BaseGameModeComponent
{
	// The camera prefab is operator-built in Workbench, which owns resource GUIDs. Child
	// of ManualCameraBase.et with root class LL_SpectatorCamera and no RplComponent.
	[Attribute("", UIWidgets.ResourceNamePicker, "Spectator camera prefab (LL_SpectatorCamera, no RplComponent).", params: "et")]
	protected ResourceName m_sSpectatorCameraPrefab;

	// Below this a map jump lands inside terrain clutter or a building.
	protected static const float MIN_JUMP_HEIGHT = 30.0;

	protected static LL_SpectatorManager s_Instance;

	static LL_SpectatorManager GetInstance()
	{
		return s_Instance;
	}

	//! Also used by the lobby stages' camera; one attribute.
	ResourceName GetCameraPrefab()
	{
		return m_sSpectatorCameraPrefab;
	}

	protected bool m_bIsSpectating;

	protected int m_iSpectatedSlotRplId = -1;

	protected LL_SpectatorCamera m_Camera;

	protected int m_iEnsureCameraRetries;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);

		if (s_Instance == this)
			s_Instance = null;
	}

	// A real stage change must tear spectating down; the stage menus take over.
	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		super.OnGameStateChanged(state);

		if (m_bIsSpectating && state != SCR_EGameModeState.GAME)
			ExitSpectator();
	}

	void EnterSpectator()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		if (m_bIsSpectating)
			return;

		m_bIsSpectating = true;
		m_iSpectatedSlotRplId = -1;

		Print("[LL_Spectator] Entering spectator mode", LogLevel.NORMAL);

		SpawnCamera();

		// U-key lobby/briefing views must not stay stacked under the spectator screen.
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
		{
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.CoopLobby);
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);
			menuManager.OpenMenu(ChimeraMenuPreset.SpectatorMenu);
		}

		// A new living character (team switch, admin reassign) drops the camera.
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);

		// Closing the Game Master editor restores the controlled entity's camera, which
		// for a dead player is no camera at all.
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager)
			editorManager.GetOnClosed().Insert(OnEditorClosed);

		// The assignment broadcast covers timings where the controlled-entity callback misses.
		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		if (lobbyManager)
			lobbyManager.GetOnPlayerAssigned().Insert(OnPlayerAssigned);

		// Camera ownership is enforced in one place; the immediate run covers a hidden
		// auto-opened editor.
		GetGame().GetCallqueue().CallLater(EnforceSpectatorCamera, 500, true);
		GetGame().GetCallqueue().CallLater(EnforceSpectatorCamera, 0, false);
	}

	// The single camera-ownership rule while spectating, decided by what the player looks
	// at: spectator on top = our camera; Game Master on top = the editor's; another
	// fullscreen menu = do nothing; no menu at all = a stage preview closed over us, so
	// the spectator menu is restored (also the only way U stays alive with no context).
	protected void EnforceSpectatorCamera()
	{
		if (!m_bIsSpectating || !m_Camera)
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase topMenu = menuManager.GetTopMenu();
		if (!topMenu && !menuManager.IsAnyDialogOpen())
		{
			if (!menuManager.FindMenuByPreset(ChimeraMenuPreset.SpectatorMenu))
			{
				Print("[LL_Spectator] Watchdog: restoring spectator menu", LogLevel.WARNING);
				menuManager.OpenMenu(ChimeraMenuPreset.SpectatorMenu);
			}
			return;
		}

		if (!topMenu || !topMenu.IsInherited(LL_SpectatorMenu))
			return;

		// An opened editor fights for the camera every frame, even with no menu at all.
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager && editorManager.IsOpened())
		{
			// The player is opening Game Master right now.
			if (editorManager.IsInTransition())
				return;

			Print("[LL_Spectator] Watchdog: closing editor left opened behind the spectator view", LogLevel.WARNING);
			editorManager.Close(false);
			return;
		}

		// A fullscreen map under the spectator is a leftover of a closed briefing menu,
		// unless the spectator screen opened it itself; closing that one would leave a
		// frozen blank canvas.
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen() && !LL_SpectatorMenu.IsMapOpen())
		{
			Print("[LL_Spectator] Watchdog: closing stale fullscreen map", LogLevel.WARNING);
			mapEntity.CloseMap();
		}

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return;

		CameraBase current = cameraManager.CurrentCamera();
		if (current == m_Camera)
			return;

		string thiefName = "<none>";
		if (current)
			thiefName = current.ClassName();
		Print(string.Format("[LL_Spectator] Watchdog: re-taking view from %1", thiefName), LogLevel.WARNING);

		cameraManager.SetCamera(m_Camera);
	}

	void ExitSpectator()
	{
		if (!m_bIsSpectating)
			return;

		m_bIsSpectating = false;
		m_iSpectatedSlotRplId = -1;

		GetGame().GetCallqueue().Remove(EnforceSpectatorCamera);

		Print("[LL_Spectator] Exiting spectator mode", LogLevel.NORMAL);

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.m_OnControlledEntityChanged.Remove(OnControlledEntityChanged);

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager)
			editorManager.GetOnClosed().Remove(OnEditorClosed);

		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		if (lobbyManager)
			lobbyManager.GetOnPlayerAssigned().Remove(OnPlayerAssigned);

		if (m_Camera)
		{
			// Hand the view over before deleting: the destructor's fallback picks the most
			// recently registered camera, which after a team switch can be the corpse's.
			m_Camera.TrySwitchToControlledEntityCamera();

			SCR_EntityHelper.DeleteEntityAndChildren(m_Camera);
			m_Camera = null;
		}

		// Exiting into a fresh possession: the new character's camera may not exist yet,
		// and character camera handlers do not take the view from a scripted camera.
		m_iEnsureCameraRetries = 6;
		GetGame().GetCallqueue().CallLater(EnsureControlledEntityCamera, 500, false);

		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.SpectatorMenu);
	}

	protected void SpawnCamera()
	{
		if (m_Camera)
			return;

		if (m_sSpectatorCameraPrefab == "")
		{
			Print("[LL_Spectator] No spectator camera prefab assigned on LL_SpectatorManager — free camera disabled", LogLevel.WARNING);
			return;
		}

		Resource res = Resource.Load(m_sSpectatorCameraPrefab);
		if (!res || !res.IsValid())
		{
			Print(string.Format("[LL_Spectator] Spectator camera prefab failed to load: %1", m_sSpectatorCameraPrefab), LogLevel.WARNING);
			return;
		}

		// Start at the corpse at eye height, else above a living slot character, else the
		// current camera.
		vector transform[4];
		IEntity corpse = SCR_PlayerController.GetLocalControlledEntity();
		IEntity aliveCharacter;
		if (!corpse)
			aliveCharacter = FindAnyAliveSlotEntity();

		if (corpse)
		{
			corpse.GetWorldTransform(transform);
			vector angles = Math3D.MatrixToAngles(transform);
			angles[1] = 0;
			angles[2] = 0;
			Math3D.AnglesToMatrix(angles, transform);
			transform[3] = corpse.GetOrigin() + Vector(0, 1.7, 0);
		}
		else if (aliveCharacter)
		{
			// Eye-height spawn would put the camera inside the character's head.
			vector charTransform[4];
			aliveCharacter.GetWorldTransform(charTransform);
			vector charPos = charTransform[3];
			vector cameraPos = charPos + Vector(0, 5, 0) - (charTransform[2] * 4);
			LL_SpectatorCamera.LookAt(cameraPos, charPos + Vector(0, 1.5, 0), transform);
		}
		else
		{
			CameraBase current = GetGame().GetCameraManager().CurrentCamera();
			if (current)
				current.GetTransform(transform);
			else
				Math3D.MatrixIdentity4(transform);
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform = transform;

		// No RplComponent on the prefab: local only.
		IEntity cameraEntity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!cameraEntity)
		{
			Print("[LL_Spectator] Spectator camera spawn failed", LogLevel.WARNING);
			return;
		}

		// SCR_ManualCamera's init may snap to the previous camera's transform.
		cameraEntity.SetWorldTransform(transform);

		m_Camera = LL_SpectatorCamera.Cast(cameraEntity);
		if (!m_Camera)
		{
			Print("[LL_Spectator] Camera prefab root class is not LL_SpectatorCamera — check the prefab", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(cameraEntity);
			return;
		}

		m_Camera.SetInputEnabled(true);
		GetGame().GetCameraManager().SetCamera(m_Camera);
	}

	void SpectateSlot(int slotRplId)
	{
		if (!m_bIsSpectating || !m_Camera)
			return;

		// -1 is the sentinel; valid ids can be negative as ints.
		if (slotRplId == -1)
		{
			FreeCamera();
			return;
		}

		// Slot keys are the character's RplComponent id, resolvable on any client.
		IEntity entity = GetSlotEntityLocal(slotRplId);
		if (!entity)
		{
			Print(string.Format("[LL_Spectator] Slot %1 has no streamed-in entity — staying in free camera", slotRplId), LogLevel.NORMAL);
			return;
		}

		m_iSpectatedSlotRplId = slotRplId;
		m_Camera.FollowEntity(entity);
	}

	void ResetCamera()
	{
		if (!m_bIsSpectating || !m_Camera)
			return;

		m_iSpectatedSlotRplId = -1;
		m_Camera.ResetCamera();
	}

	// Clearing the followed slot matters as much as moving: the attach component would
	// drag the camera back next frame.
	void MoveCameraTo(float worldX, float worldZ)
	{
		if (!m_bIsSpectating || !m_Camera)
			return;

		m_iSpectatedSlotRplId = -1;
		m_Camera.MoveTo(worldX, worldZ, MIN_JUMP_HEIGHT);
	}

	void FreeCamera()
	{
		if (!m_bIsSpectating)
			return;

		m_iSpectatedSlotRplId = -1;

		if (m_Camera)
			m_Camera.StopFollowing();
	}

	// Camera start point for spectators without a corpse.
	protected IEntity FindAnyAliveSlotEntity()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return null;

		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.IsDestroyed())
				continue;

			IEntity entity = mgr.ResolveSlotEntity(slot.m_iRplId);
			if (entity)
				return entity;
		}

		return null;
	}

	protected IEntity GetSlotEntityLocal(int slotRplId)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return null;

		return mgr.ResolveSlotEntity(slotRplId);
	}

	bool IsSpectating()
	{
		return m_bIsSpectating;
	}

	int GetSpectatedSlotRplId()
	{
		return m_iSpectatedSlotRplId;
	}

	// Leave spectating regardless of which callback arrives first.
	protected void OnPlayerAssigned(int playerId, int slotRplId)
	{
		if (!m_bIsSpectating)
			return;

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc || pc.GetPlayerId() != playerId)
			return;

		ExitSpectator();
	}

	// Post-exit safety net after possession had time to settle.
	protected void EnsureControlledEntityCamera()
	{
		if (m_bIsSpectating)
			return;

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;

		CameraBase characterCamera = pc.GetPlayerCamera();
		if (!characterCamera)
		{
			// Exits into menu-only states have no character camera at all.
			m_iEnsureCameraRetries--;
			if (m_iEnsureCameraRetries > 0)
				GetGame().GetCallqueue().CallLater(EnsureControlledEntityCamera, 500, false);
			return;
		}

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager && cameraManager.CurrentCamera() != characterCamera)
			cameraManager.SetCamera(characterCamera);
	}

	// The editor's close handler restores the controlled-entity camera; re-take one frame later.
	protected void OnEditorClosed()
	{
		GetGame().GetCallqueue().CallLater(RestoreCameraAfterEditor, 0, false);
	}

	protected void RestoreCameraAfterEditor()
	{
		if (!m_bIsSpectating || !m_Camera)
			return;

		GetGame().GetCameraManager().SetCamera(m_Camera);
	}

	// A living controlled entity means the player is back in the game.
	protected void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		if (!m_bIsSpectating || !to)
			return;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(
			to.FindComponent(SCR_DamageManagerComponent));
		if (damageManager && damageManager.GetState() == EDamageState.DESTROYED)
			return;

		ExitSpectator();
	}
}