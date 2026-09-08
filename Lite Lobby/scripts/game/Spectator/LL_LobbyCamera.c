// Client-local static camera for the pre-game stages. Nobody is possessed before GAME
// and the engine registers no camera, so the view and the audio listener sit at the
// engine's default, which on some terrains is under the sea. Input is disabled: a
// flyable camera during slotting is free reconnaissance. Uses the spectator camera
// prefab (no RplComponent); deleted when GAME starts.
class LL_LobbyCamera
{
	// Pulled back so the surroundings are in frame rather than the top of the head.
	protected static const float UNIT_HEIGHT = 30.0;
	protected static const float UNIT_DISTANCE = 40.0;

	// Fallback over the middle of the world; high enough to clear any terrain.
	protected static const float WORLD_HEIGHT = 300.0;
	protected static const float WORLD_LOOK_AHEAD = 600.0;

	// Slots arrive by replication shortly after the join.
	protected static const int AIM_RETRIES = 5;
	protected static const int AIM_RETRY_MS = 2000;

	// Keys that make sense with no character while no menu is open. Defined in
	// Configs/System/chimeraInputCommon.conf.
	protected static const string INPUT_CONTEXT = "LL_LobbyCameraContext";

	protected LL_SpectatorCamera m_Camera;
	protected int m_iAimRetries;
	protected bool m_bAimedAtUnit;

	// GAME and DEBRIEFING belong to the character or the spectator camera.
	void UpdateForState(SCR_EGameModeState state)
	{
		if (state == SCR_EGameModeState.SLOTSELECTION || state == SCR_EGameModeState.BRIEFING)
		{
			m_iAimRetries = 0;
			Show();
		}
		else
		{
			Hide();
		}
	}

	void Show()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		// A retry can land after the round started; the state is re-read.
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (!gameMode)
			return;

		SCR_EGameModeState state = gameMode.GetState();
		if (state != SCR_EGameModeState.SLOTSELECTION && state != SCR_EGameModeState.BRIEFING)
			return;

		if (SCR_PlayerController.GetLocalControlledEntity())
			return;

		if (m_Camera)
		{
			Aim();
			return;
		}

		// The spectator prefab has the same requirements and is already assigned on the
		// game mode entity.
		ResourceName prefab;
		LL_SpectatorManager spectatorManager = LL_SpectatorManager.GetInstance();
		if (spectatorManager)
			prefab = spectatorManager.GetCameraPrefab();

		if (prefab == "")
		{
			Print("[LL_Lobby] No camera prefab on LL_SpectatorManager — lobby keeps the engine's default view", LogLevel.WARNING);
			return;
		}

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print(string.Format("[LL_Lobby] Lobby camera prefab failed to load: %1", prefab), LogLevel.WARNING);
			return;
		}

		vector transform[4];
		BuildTransform(transform);

		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform = transform;

		IEntity cameraEntity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!cameraEntity)
		{
			Print("[LL_Lobby] Lobby camera spawn failed", LogLevel.WARNING);
			return;
		}

		m_Camera = LL_SpectatorCamera.Cast(cameraEntity);
		if (!m_Camera)
		{
			Print("[LL_Lobby] Lobby camera prefab root class is not LL_SpectatorCamera — check the prefab", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(cameraEntity);
			return;
		}

		// SCR_ManualCamera's init snaps to the previously active camera's transform.
		m_Camera.SetWorldTransform(transform);
		m_Camera.SetInputEnabled(false);
		GetGame().GetCameraManager().SetCamera(m_Camera);

		// The Game Master editor restores the controlled entity's camera on close, which
		// for an entity-less player is no camera at all.
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager)
			editorManager.GetOnClosed().Insert(OnEditorClosed);

		GetGame().GetCallqueue().CallLater(KeepLobbyKeysAlive, 0, true);

		Print(string.Format("[LL_Lobby] Lobby camera placed at %1", transform[3].ToString()), LogLevel.NORMAL);

		if (!m_bAimedAtUnit)
			RetryAim();
	}

	void Hide()
	{
		GetGame().GetCallqueue().Remove(Show);
		GetGame().GetCallqueue().Remove(KeepLobbyKeysAlive);
		GetGame().GetCallqueue().Remove(RestoreAfterEditor);

		if (!m_Camera)
			return;

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager)
			editorManager.GetOnClosed().Remove(OnEditorClosed);

		// Hand the view over before deleting: the destructor's fallback picks the last
		// registered camera.
		m_Camera.TrySwitchToControlledEntityCamera();

		SCR_EntityHelper.DeleteEntityAndChildren(m_Camera);
		m_Camera = null;
	}

	// With the lobby menu closed (admin U-toggle) the player has no character and no
	// menu, so no input context is active and every key is dead. Vanilla's manual camera
	// only activates its context while input is enabled. Only while no menu is up.
	protected void KeepLobbyKeysAlive()
	{
		if (!m_Camera)
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager && menuManager.GetTopMenu())
			return;

		GetGame().GetInputManager().ActivateContext(INPUT_CONTEXT);
	}

	// Re-aim once slots have replicated; the camera only moves.
	protected void RetryAim()
	{
		if (m_iAimRetries >= AIM_RETRIES)
			return;

		m_iAimRetries++;
		GetGame().GetCallqueue().CallLater(Show, AIM_RETRY_MS, false);
	}

	protected void Aim()
	{
		if (m_bAimedAtUnit)
			return;

		vector transform[4];
		BuildTransform(transform);
		m_Camera.SetWorldTransform(transform);

		if (!m_bAimedAtUnit)
			RetryAim();
	}

	// Above a playable character: dry land by definition. The middle of the world is
	// often open sea.
	protected void BuildTransform(out vector transform[4])
	{
		IEntity unit = FindAnySlotEntity();
		if (unit)
		{
			vector unitPos = unit.GetOrigin();
			vector from = unitPos + Vector(0, UNIT_HEIGHT, 0) - Vector(0, 0, UNIT_DISTANCE);
			LL_SpectatorCamera.LookAt(from, unitPos, transform);
			m_bAimedAtUnit = true;
			return;
		}

		m_bAimedAtUnit = false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Math3D.MatrixIdentity4(transform);
			return;
		}

		vector mins, maxs;
		world.GetBoundBox(mins, maxs);

		vector centre = (mins + maxs) * 0.5;
		float groundY = Math.Max(world.GetSurfaceY(centre[0], centre[2]), 0);

		vector from = Vector(centre[0], groundY + WORLD_HEIGHT, centre[2]);
		vector to = Vector(centre[0], groundY, centre[2] + WORLD_LOOK_AHEAD);
		LL_SpectatorCamera.LookAt(from, to, transform);
	}

	// Pre-placed playables are part of the loaded world on every machine.
	protected IEntity FindAnySlotEntity()
	{
		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		if (!lobbyManager)
			return null;

		foreach (LL_SlotData slot : lobbyManager.GetSlots())
		{
			if (slot.IsDestroyed())
				continue;

			IEntity entity = lobbyManager.ResolveSlotEntity(slot.m_iRplId);
			if (entity)
				return entity;
		}

		return null;
	}

	protected void OnEditorClosed()
	{
		// The editor finishes its own camera restore first.
		GetGame().GetCallqueue().CallLater(RestoreAfterEditor, 0, false);
	}

	protected void RestoreAfterEditor()
	{
		if (!m_Camera)
			return;

		GetGame().GetCameraManager().SetCamera(m_Camera);
	}
}