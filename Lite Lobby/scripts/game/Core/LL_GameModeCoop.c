// Lobby game mode: the SLOTSELECTION → BRIEFING → GAME → DEBRIEFING state machine and
// the designer-facing configuration. Lobby data lives in LL_LobbyManager.

modded enum SCR_EGameModeState
{
	SLOTSELECTION,
	BRIEFING,
	DEBRIEFING,
}

class LL_GameModeCoopClass : SCR_BaseGameModeClass
{
}

class LL_GameModeCoop : SCR_BaseGameMode
{
	[Attribute("0", UIWidgets.CheckBox, "DEBUG ONLY: match reconnecting players by name when they have no GUID (e.g. a dedicated debug server with no backend). Names aren't unique, so leave this OFF in production — GUID reconnection always works regardless.", category: "Lite Lobby")]
	protected bool m_bDebugAllowNameReconnect;

	[Attribute("120000", UIWidgets.EditBox, "Reconnect reservation time (ms) during SLOT SELECTION (while players are still picking roles). -1 = infinite. From briefing onwards the value below applies instead.", category: "Lite Lobby")]
	protected int m_iReconnectTime;

	[Attribute("-1", UIWidgets.EditBox, "Reconnect reservation time (ms) from when BRIEFING starts (briefing, game, debriefing). -1 (or 0) = infinite — hold the slot until the player reconnects. A positive value caps it.", category: "Lite Lobby")]
	protected int m_iReconnectTimeAfterBriefing;

	[Attribute("1", UIWidgets.CheckBox, "Only admins can advance game state (start briefing, start game).", category: "Lite Lobby")]
	protected bool m_bAdminMode;

	[Attribute("120000", UIWidgets.EditBox, "Freeze time (ms) after GAME starts. Players can't leave spawn zone.", category: "Lite Lobby")]
	protected int m_iFreezeTime;

	[Attribute("1", UIWidgets.CheckBox, "Hard freeze at GAME start: hold everyone's controls for a moment so the world can stream in around them before anyone can move. Time stands still during any hard freeze; the freeze time counts from its end.", category: "Lite Lobby")]
	protected bool m_bHardFreezeEnabled;

	[Attribute("60000", UIWidgets.EditBox, "Hard freeze duration (ms) at GAME start. The freeze time above starts counting when it ends.", category: "Lite Lobby")]
	protected int m_iHardFreezeTime;

	[Attribute("1", UIWidgets.CheckBox, "During freeze time players can't fire their weapons and take no damage (a protected setup period). They can still be killed by leaving the freeze zone.", category: "Lite Lobby")]
	protected bool m_bForbidShootingDuringFreeze;

	[Attribute("0", UIWidgets.CheckBox, "Session saves: the server snapshots the GAME phase on its own autosave schedule (the persistence block of the server configuration) so a crashed round can continue where it stopped. A plain restart is always a fresh start; a resume is the server's -loadSessionSave launch parameter. Leave the mission's Save Types at their default and set its Systems Config to the lobby's (LL_LobbySystems.conf).", category: "Lite Lobby")]
	protected bool m_bSessionSaves;

	[Attribute("0", UIWidgets.CheckBox, "Remove AI units not occupied by players when GAME starts.", category: "Lite Lobby")]
	protected bool m_bRemoveRedundantUnits;

	[Attribute("0", UIWidgets.CheckBox, "Let AI groups report the enemies they identify as timestamped military markers on their own faction's map (a game feature since 1.8). Off by default: in a lobby mission those markers reveal player squads to the other side. A faction whose own settings forbid AI reports never reports, even when this is on.", category: "Lite Lobby")]
	protected bool m_bAllowAiSpotReports;

	[Attribute("1", UIWidgets.CheckBox, "Disable text chat for alive players during GAME. Admins always see chat.", category: "Lite Lobby")]
	protected bool m_bDisableChat;

	[Attribute("1", UIWidgets.CheckBox, "Hide the VON HUD popup for incoming PROXIMITY (direct) speech — you no longer see who is talking nearby.", category: "Lite Lobby")]
	protected bool m_bHideProximityVonUI;

	[Attribute("1", UIWidgets.CheckBox, "Hide WHO is speaking on RADIO in the VON HUD popup (name/role/icons) — only the frequency stays visible.", category: "Lite Lobby")]
	protected bool m_bHideRadioSpeakerUI;

	[Attribute("0", UIWidgets.CheckBox, "Enforce minimum video settings on every client (shadows, distant shadows, grass, contact shadows). Turning those down lets a player see units that others cannot — no shadows in the open, no grass hiding prone bodies. Raises the setting when a client joins and prevents it from being lowered again in the video options. The minimums are below.", category: "Lite Lobby")]
	protected bool m_bEnforceMinGraphics;

	[Attribute("2", UIWidgets.EditBox, "Minimum shadow quality — video options steps: 1 = Low, 2 = Medium, 3 = High.", category: "Lite Lobby")]
	protected int m_iMinShadowQuality;

	[Attribute("2", UIWidgets.EditBox, "Minimum distant shadows — video options steps: 0 = None, 1 = Low, 2 = Medium, 3 = High, 4 = Ultra.", category: "Lite Lobby")]
	protected int m_iMinDistantShadows;

	[Attribute("2", UIWidgets.EditBox, "Minimum grass detail (LOD) — video options steps: 0 = Lowest, 1 = Low, 2 = Medium, 3 = High.", category: "Lite Lobby")]
	protected int m_iMinGrassLod;

	[Attribute("1", UIWidgets.EditBox, "Minimum contact shadows (SSDO) — video options steps: 0 = Off, 1 = On.", category: "Lite Lobby")]
	protected int m_iMinContactShadows;

	// Ticked once a second during freeze.
	[RplProp()]
	protected float m_fFreezeTimeRemaining;

	[RplProp()]
	protected float m_fGameStartTimestamp;

	// Hard freeze locks character controls, suspends the mission logic, the mission clock
	// and the day/night clock, and stops or pins every vehicle. Script cannot stop AI or
	// physics (no world time-scale setter).
	[RplProp()]
	protected bool m_bHardFreeze;

	// Seconds of hold left. Only meaningful while m_bHardFreeze.
	[RplProp()]
	protected float m_fHardFreezeRemaining;

	// Session saves (server). Saving opens only after the GAME entry has dispatched its
	// whole body batch: a replacement finishing inside the loop must not open it early.
	protected bool m_bStartupBatchDispatched;
	protected bool m_bRosterCheckedForSaves;
	protected bool m_bResumeRefused;
	protected int m_iSnapshotStartTick;

	// Planned stop (server): one stop-time snapshot, whichever path writes it.
	protected bool m_bStopSaveRequested;
	protected bool m_bStopSaveDone;

	// Resume (server). The snapshot's lobby record is applied in one step at ACTIVE.
	protected static const int RESUME_DEADLINE_MS = 10000;
	protected bool m_bGameStarted;
	protected bool m_bResumeBegun;
	protected bool m_bResumePending;
	protected bool m_bResumeFinalising;
	protected float m_fResumeDeadline;
	protected string m_sResumeFailure;
	protected ref LL_SessionRecord m_ResumeRecord;

	// The resume's three replicated additions: each set once and bumped once, read by the
	// hold text and the spectator countdown.
	[RplProp()]
	protected float m_fResumedSnapshotTime;
	[RplProp()]
	protected int m_iMissionEndDuration;
	[RplProp()]
	protected float m_fMissionEndStartedAt = -1;
	protected bool m_bMissionEndDurationSet;
	protected bool m_bMissionEndStartSet;

	// What a snapshot during a resume hold writes instead of the hold's own values, and
	// what the release restores.
	protected float m_fPreHoldHardFreezeRemaining;
	protected bool m_bPreHoldDayAdvance;

	// The clock reading a hard freeze holds; while free it follows the clock, so a proxy that
	// receives the flag before the value already anchors on its own estimate.
	protected float m_fHeldElapsed;
	protected float m_fLastSeenElapsed;

	protected bool m_bFreezeTimerScheduled;

	// Server: the aircraft held in the air and the damage effects paused during a hard freeze.
	protected ref LL_VehicleHold m_VehicleHold = new LL_VehicleHold();
	protected ref LL_DamagePause m_DamagePause = new LL_DamagePause();

	protected ref ScriptInvokerInt m_OnGameStateChanged = new ScriptInvokerInt();

	ScriptInvokerInt GetOnGameStateChanged()
	{
		return m_OnGameStateChanged;
	}

	// Debug only: names are not unique.
	bool IsNameReconnectAllowed()
	{
		return m_bDebugAllowNameReconnect;
	}

	int GetReconnectTime()
	{
		// From BRIEFING on the longer window applies; a value <= 0 schedules no expiry.
		SCR_EGameModeState state = GetState();
		if (state == SCR_EGameModeState.BRIEFING || state == SCR_EGameModeState.GAME || state == SCR_EGameModeState.DEBRIEFING)
			return m_iReconnectTimeAfterBriefing;

		return m_iReconnectTime;
	}

	bool IsAdminMode()			{ return m_bAdminMode; }
	int GetFreezeTimeDuration()	{ return m_iFreezeTime; }
	float GetFreezeTimeRemaining() { return m_fFreezeTimeRemaining; }
	bool ForbidShootingDuringFreeze() { return m_bForbidShootingDuringFreeze; }
	bool IsSessionSavesEnabled()	{ return m_bSessionSaves; }

	// The countdown reads > 0 only during the post-start freeze.
	bool IsFreezeSafetyActive()
	{
		return m_bForbidShootingDuringFreeze && m_fFreezeTimeRemaining > 0;
	}
	float GetHardFreezeRemaining()	{ return m_fHardFreezeRemaining; }
	bool IsHardFreezeEnabled()		{ return m_bHardFreezeEnabled; }
	int GetHardFreezeDuration()		{ return m_iHardFreezeTime; }

	bool RemoveRedundantUnits()	{ return m_bRemoveRedundantUnits; }
	bool AllowAiSpotReports()	{ return m_bAllowAiSpotReports; }
	bool IsChatDisabled()		{ return m_bDisableChat; }
	bool HideProximityVonUI()	{ return m_bHideProximityVonUI; }
	bool HideRadioSpeakerUI()	{ return m_bHideRadioSpeakerUI; }

	bool EnforceMinGraphics()	{ return m_bEnforceMinGraphics; }
	int GetMinShadowQuality()	{ return m_iMinShadowQuality; }
	int GetMinDistantShadows()	{ return m_iMinDistantShadows; }
	int GetMinGrassLod()		{ return m_iMinGrassLod; }
	int GetMinContactShadows()	{ return m_iMinContactShadows; }

	float GetGameStartTimestamp() { return m_fGameStartTimestamp; }

	SCR_EGameModeState GetLobbyState()
	{
		return GetState();
	}

	static LL_GameModeCoop GetInstance()
	{
		return LL_GameModeCoop.Cast(GetGame().GetGameMode());
	}

	//! Static so a one-line guard does not need the game mode in hand.
	static bool IsHardFreezeActive()
	{
		LL_GameModeCoop gameMode = GetInstance();
		return gameMode && gameMode.m_bHardFreeze;
	}

	override void OnGameStart()
	{
		super.OnGameStart();

		Print("[LL_Lobby] GameMode OnGameStart", LogLevel.NORMAL);

		// A stats menu open at the previous world's teardown never saw OnMenuClose.
		LL_InputLock.ResetStatic();

		// Input only exists on machines with a local player.
		if (RplSession.Mode() != RplMode.Dedicated)
		{
			GetGame().GetInputManager().AddActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_OpenLobby);

			// Applied on join so the player is at the floor while slotting; the video-options
			// hook keeps it there.
			LL_GraphicsPolicy.ApplyMinimums();
		}

		if (Replication.IsServer())
		{
			InitSessionSaves_S();

			bool resume = WillResume();
			m_bGameStarted = true;
			m_bResumeBegun = resume;
			if (resume)
				BeginResume_S();
			else
				SetLobbyState(SCR_EGameModeState.SLOTSELECTION);
		}
	}

	// Manual lobby open (LL_OpenLobby, default hold U). Only opens.
	protected void Action_OpenLobby()
	{
		// Admin-only: once a round runs, slots are locked in and the server rejects takes.
		if (!SCR_Global.IsAdmin())
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		// A lobby menu on top owns the toggle itself; the menu preset contexts also carry
		// LL_OpenLobby, so acting here would reopen what the menu just closed.
		MenuBase topMenu = menuManager.GetTopMenu();
		if (topMenu && (topMenu.IsInherited(LL_CoopLobby)
			|| topMenu.IsInherited(LL_BriefingMapMenu)
			|| topMenu.IsInherited(LL_SpectatorMenu)))
			return;

		// An open Game Master editor owns the player's voice, so the lobby cannot sit on top
		// of it with working menu voice. Close it; Y reopens it.
		SCR_EditorManagerCore editorCore = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (editorCore)
		{
			SCR_EditorManagerEntity editorManager = editorCore.GetEditorManager();
			if (editorManager && editorManager.IsOpened())
				editorManager.Close();
		}

		// OpenMenu on a preset already in the stack below another menu shows nothing;
		// closing first puts a fresh instance on top.
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.CoopLobby);
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);

		// Closing a BriefingMapMenu calls SCR_MapEntity.CloseMap; reopening in the same
		// frame races the new instance's map setup.
		GetGame().GetCallqueue().CallLater(OpenStateMenuDeferred, 0, false);
	}

	protected void OpenStateMenuDeferred()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (GetState() == SCR_EGameModeState.BRIEFING)
			menuManager.OpenMenu(ChimeraMenuPreset.BriefingMapMenu);
		else
			menuManager.OpenMenu(ChimeraMenuPreset.CoopLobby);
	}

	// The stats menus close the map screens before opening. Restored from the game mode,
	// not the closing menu: the call queue holds weak refs, so a CallLater scheduled on a
	// menu that just closed dies with it.
	void RestoreStateMenuIfBare()
	{
		GetGame().GetCallqueue().Remove(RestoreStateMenuStep);
		GetGame().GetCallqueue().CallLater(RestoreStateMenuStep, 0, false);
	}

	protected void RestoreStateMenuStep()
	{
		if (GetState() == SCR_EGameModeState.GAME)
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.CoopLobby)
			|| menuManager.FindMenuByPreset(ChimeraMenuPreset.BriefingMapMenu)
			|| menuManager.FindMenuByPreset(ChimeraMenuPreset.SpectatorMenu))
			return;

		// Keep checking while another menu is on top: a player with no screen in a
		// mandatory stage has no recovery (the U key is admin-gated outside GAME).
		if (menuManager.IsAnyMenuOpen())
		{
			GetGame().GetCallqueue().Remove(RestoreStateMenuStep);
			GetGame().GetCallqueue().CallLater(RestoreStateMenuStep, 500, false);
			return;
		}

		OpenStateMenuDeferred();
	}

	// Vanilla rewrites the local disable flags from EOnFrame every frame and re-enables
	// them when the last menu closes, so this override is the only writer that wins.
	// Reasserting per frame also freezes a client that joins or reconnects mid-hold.
	override protected void SetLocalControls(bool enabled)
	{
		bool held = m_bHardFreeze;

		if (LL_InputLock.IsLocked() || held)
			enabled = false;

		super.SetLocalControls(enabled);
		LL_InputLock.EnforceView(held);
	}

	override void OnGameStateChanged()
	{
		super.OnGameStateChanged();

		SCR_EGameModeState newState = GetState();

		Print(string.Format("[LL_Lobby] State changed to: %1", typename.EnumToString(SCR_EGameModeState, newState)), LogLevel.NORMAL);

		m_OnGameStateChanged.Invoke(newState);
	}

	// Server-only.
	protected void SetLobbyState(SCR_EGameModeState newState)
	{
		if (!Replication.IsServer())
			return;

		SCR_EGameModeState oldState = GetState();
		if (oldState == newState)
			return;

		Print(string.Format("[LL_Lobby] State transition: %1 → %2",
			typename.EnumToString(SCR_EGameModeState, oldState),
			typename.EnumToString(SCR_EGameModeState, newState)), LogLevel.NORMAL);

		// SetGameModeState is a modded addition; m_eGameState is private in vanilla.
		SetGameModeState(newState);

		OnEnterState_S(newState);
	}

	void AdvanceLobbyState()
	{
		if (!Replication.IsServer())
			return;

		SCR_EGameModeState current = GetState();

		switch (current)
		{
			case SCR_EGameModeState.SLOTSELECTION:
				SetLobbyState(SCR_EGameModeState.BRIEFING);
				break;

			case SCR_EGameModeState.BRIEFING:
				SetLobbyState(SCR_EGameModeState.GAME);
				break;

			case SCR_EGameModeState.GAME:
				SetLobbyState(SCR_EGameModeState.DEBRIEFING);
				break;
		}
	}

	protected void OnEnterState_S(SCR_EGameModeState state)
	{
		// Leaving GAME with a hold running would lock everyone in their body with no menu
		// to escape to.
		if (state != SCR_EGameModeState.GAME)
		{
			EndHardFreeze_S();
			CancelFreezeCountdown_S();
			CloseSavePhase_S();
		}

		switch (state)
		{
			case SCR_EGameModeState.GAME:
				OnEnterGame_S();
				break;

			case SCR_EGameModeState.DEBRIEFING:
				DiscardSnapshots_S();
				break;
		}
	}

	protected void OnEnterGame_S()
	{
		m_fGameStartTimestamp = System.GetTickCount();
		m_fFreezeTimeRemaining = m_iFreezeTime / 1000.0;

		Replication.BumpMe();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			// Before possession so each body's squad radio can be tuned as players enter.
			mgr.AssignSquadFrequencies_S();

			// Every slotted, present player enters a fresh runtime body (see PossessSlot_S).
			// A disconnected-but-reserved holder has no controller to receive one; the
			// reconnect path spawns it on return. Ids are collected first because each
			// respawn mutates the slot list.
			Print("[LL_Lobby] GAME start: spawning bodies for slotted players...", LogLevel.NORMAL);
			PlayerManager pm = GetGame().GetPlayerManager();
			array<int> toSpawn = {};
			foreach (LL_SlotData slot : mgr.GetSlots())
			{
				if (slot.m_iPlayerId < 0 || slot.IsDestroyed())
					continue;
				if (!pm || !pm.GetPlayerController(slot.m_iPlayerId))
					continue;
				toSpawn.Insert(slot.m_iRplId);
			}

			foreach (int slotRplId : toSpawn)
				mgr.RespawnSlotCharacter_S(slotRplId, true);

			Print(string.Format("[LL_Lobby] GAME start: spawned %1 bodies", toSpawn.Count()), LogLevel.NORMAL);

			// A player without a slot would otherwise sit in gameplay with no controlled entity.
			array<int> playerIds = {};
			GetGame().GetPlayerManager().GetPlayers(playerIds);
			foreach (int playerId : playerIds)
			{
				if (!mgr.FindSlotByPlayerId(playerId))
					SendPlayerToSpectator_S(playerId);
			}
		}

		// Players who just entered a body stop being menu speakers; the parked flag replicates.
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
			vonMgr.UpdateAllParked_S();

		// Deletion fires LL_PlayableComponent.OnDelete → UnregisterSlot_S on every machine.
		if (m_bRemoveRedundantUnits)
			RemoveRedundantUnits_S();

		ScheduleFreezeCountdown_S();

		// Runs inside the freeze window and holds its countdown; the freeze zones still
		// confine movement once it lifts.
		if (m_bHardFreezeEnabled && m_iHardFreezeTime > 0)
			StartHardFreeze_S(m_iHardFreezeTime / 1000);

		m_bStartupBatchDispatched = true;
		AllowSavesIfReady_S();

		Print(string.Format("[LL_Lobby] Game started. Freeze time: %1s", m_fFreezeTimeRemaining), LogLevel.NORMAL);
	}

	protected void RemoveRedundantUnits_S()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		// Deleting triggers UnregisterSlot_S, which mutates the list being iterated.
		array<int> redundant = {};
		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (slot.m_iPlayerId < 0)
				redundant.Insert(slot.m_iRplId);
		}

		foreach (int rplId : redundant)
		{
			IEntity entity = mgr.GetSlotEntity_S(rplId);
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}

	// Every path that starts or stops the freeze countdown goes through this pair, so no
	// path can leave two ticks running or none.
	protected void ScheduleFreezeCountdown_S()
	{
		if (m_bFreezeTimerScheduled || m_fFreezeTimeRemaining <= 0)
			return;

		m_bFreezeTimerScheduled = true;
		GetGame().GetCallqueue().CallLater(UpdateFreezeTimer_S, 1000, true);
	}

	protected void CancelFreezeCountdown_S()
	{
		if (!m_bFreezeTimerScheduled)
			return;

		m_bFreezeTimerScheduled = false;
		GetGame().GetCallqueue().Remove(UpdateFreezeTimer_S);
	}

	protected void UpdateFreezeTimer_S()
	{
		// Cancelled by every hard freeze; a tick that slips in still does not drain the freeze.
		if (m_bHardFreeze)
			return;

		m_fFreezeTimeRemaining -= 1.0;

		if (m_fFreezeTimeRemaining <= 0)
		{
			m_fFreezeTimeRemaining = 0;
			CancelFreezeCountdown_S();
			OnFreezeTimeEnded_S();
		}

		Replication.BumpMe();
	}

	// LL freeze zones gate themselves on the replicated countdown; only stock editor
	// restriction zones need tearing down.
	protected void OnFreezeTimeEnded_S()
	{
		Print("[LL_Lobby] Freeze time ended", LogLevel.NORMAL);
		RemoveVanillaRestrictionZones_S();
	}

	// Stock editor restriction zones (SCR_EditorRestrictionZoneEntity) stop confining once
	// freeze ends. Unregister BEFORE deleting: the manager kills anyone whose in-zone flag
	// flips, and a zone deleted while still in its set reads null on the next 50 ms tick,
	// so every confined player registers as having left and is killed.
	protected void RemoveVanillaRestrictionZones_S()
	{
		if (!Replication.IsServer())
			return;

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return;

		SCR_PlayersRestrictionZoneManagerComponent zoneManager = SCR_PlayersRestrictionZoneManagerComponent.Cast(
			gameMode.FindComponent(SCR_PlayersRestrictionZoneManagerComponent));
		if (!zoneManager)
			return;

		// Unregistering mutates the live set.
		array<SCR_EditorRestrictionZoneEntity> zones = zoneManager.LL_GetZones();

		foreach (SCR_EditorRestrictionZoneEntity zone : zones)
		{
			if (zone)
				zoneManager.RemoveRestrictionZone(zone);
		}

		foreach (SCR_EditorRestrictionZoneEntity zone : zones)
		{
			if (zone)
				SCR_EntityHelper.DeleteEntityAndChildren(zone);
		}

		if (!zones.IsEmpty())
			Print(string.Format("[LL_Lobby] Removed %1 vanilla restriction zone(s) after freeze", zones.Count()), LogLevel.NORMAL);
	}

	// Hard freeze: both scalars replicate, so a late joiner lands in the correct state.

	//! Calling again replaces a running hold.
	void StartHardFreeze_S(int seconds)
	{
		if (!Replication.IsServer())
			return;

		if (seconds <= 0)
		{
			EndHardFreeze_S();
			return;
		}

		GetGame().GetCallqueue().Remove(UpdateHardFreezeTimer_S);

		// Re-timing a running hold must not re-read a state this already overwrote.
		if (!m_bHardFreeze)
		{
			StopDayAdvance_S();
			m_fHeldElapsed = GetElapsedTime();
			m_VehicleHold.PinAll_S();
			m_DamagePause.PauseAll_S();
		}
		else if (IsResumeHoldActive())
		{
			// The admin's timed hold supersedes the saved remainder, but the daylight intent the
			// snapshot recorded still has to come back when this one ends.
			m_bDayAdvanceWasOn = m_bPreHoldDayAdvance;
			m_fPreHoldHardFreezeRemaining = 0;
		}

		m_bHardFreeze = true;
		m_fHardFreezeRemaining = seconds;
		Replication.BumpMe();

		// Every hard freeze holds the freeze countdown too; the release resumes it.
		CancelFreezeCountdown_S();

		GetGame().GetCallqueue().CallLater(UpdateHardFreezeTimer_S, 1000, true);

		Print(string.Format("[LL_Lobby] Hard freeze started (%1s)", seconds), LogLevel.NORMAL);
	}

	//! Release the hold. Safe to call when nothing is held.
	void EndHardFreeze_S()
	{
		if (!Replication.IsServer())
			return;
		if (!m_bHardFreeze)
			return;

		bool resumeHold = IsResumeHoldActive();

		GetGame().GetCallqueue().Remove(UpdateHardFreezeTimer_S);
		m_bHardFreeze = false;
		m_fHardFreezeRemaining = 0;
		Replication.BumpMe();

		m_VehicleHold.Release_S();
		m_DamagePause.ResumeAll_S();

		if (resumeHold)
		{
			// The release puts back what the snapshot recorded, not what the hold wrote.
			if (m_bPreHoldDayAdvance)
			{
				TimeAndWeatherManagerEntity timeManager = GetTimeManager();
				if (timeManager)
					timeManager.SetIsDayAutoAdvanced(true);
			}

			Print("[LL_Lobby] Resume: hold released", LogLevel.NORMAL);

			LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
			if (mgr)
				mgr.BroadcastAdminMessage_S("#LL-Resume_Released");

			// Leaving GAME releases everything; the saved remainder must not restart a hold there.
			int remaining = Math.Ceil(m_fPreHoldHardFreezeRemaining);
			m_fPreHoldHardFreezeRemaining = 0;
			if (remaining > 0 && GetState() == SCR_EGameModeState.GAME)
			{
				StartHardFreeze_S(remaining);
				return;
			}
		}
		else
		{
			// Runs exactly once per hold: timer drain, admin /hardfreeze 0, and leaving GAME.
			RestoreDayAdvance_S();
			Print("[LL_Lobby] Hard freeze ended", LogLevel.NORMAL);
		}

		// The last hard freeze to end resumes the freeze countdown, exactly once.
		if (GetState() == SCR_EGameModeState.GAME)
			ScheduleFreezeCountdown_S();
	}

	// Time stands still during any hard freeze, on every machine, by writing the held reading
	// back over the clock: the base adds time only under its own conditions (players present,
	// GAME running), so subtracting the slice would run the clock backwards whenever it did not.
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// A proxy's copy changes outside super only when the server corrects it; the correction
		// must become the anchor before the base advances it.
		if (!IsMaster() && m_fTimeElapsed != m_fLastSeenElapsed)
			m_fHeldElapsed = m_fTimeElapsed;

		super.EOnFrame(owner, timeSlice);

		if (m_bHardFreeze)
			m_fTimeElapsed = m_fHeldElapsed;
		else
			m_fHeldElapsed = m_fTimeElapsed;
		m_fLastSeenElapsed = m_fTimeElapsed;

		if (m_bHardFreeze && IsMaster())
			m_VehicleHold.Tick_S();
	}

	//! A hold with no countdown: the resume hold, released by the admin.
	bool IsResumeHoldActive()
	{
		return m_bHardFreeze && m_fHardFreezeRemaining < 0;
	}

	// SetIsDayAutoAdvanced is authority-only and the engine replicates it itself.
	protected bool m_bDayAdvanceWasOn;

	// Saved, not assumed: a mission may deliberately run a fixed time of day.
	protected void StopDayAdvance_S()
	{
		TimeAndWeatherManagerEntity timeManager = GetTimeManager();
		if (!timeManager)
			return;

		m_bDayAdvanceWasOn = timeManager.GetIsDayAutoAdvanced();
		if (m_bDayAdvanceWasOn)
			timeManager.SetIsDayAutoAdvanced(false);
	}

	protected void RestoreDayAdvance_S()
	{
		if (!m_bDayAdvanceWasOn)
			return;

		m_bDayAdvanceWasOn = false;

		TimeAndWeatherManagerEntity timeManager = GetTimeManager();
		if (timeManager)
			timeManager.SetIsDayAutoAdvanced(true);
	}

	protected TimeAndWeatherManagerEntity GetTimeManager()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;

		return world.GetTimeAndWeatherManager();
	}

	//! In GAME acts immediately (start, re-time, or release with 0); outside GAME it only
	//! arms the next GAME start. Independent of the freeze countdown.
	void AdjustHardFreeze_S(int seconds)
	{
		if (!Replication.IsServer())
			return;

		m_iHardFreezeTime = Math.Max(0, seconds) * 1000;

		if (GetState() == SCR_EGameModeState.GAME)
			StartHardFreeze_S(seconds);

		Print(string.Format("[LL_Lobby] Hard freeze duration set to %1s", seconds), LogLevel.NORMAL);
	}

	protected void UpdateHardFreezeTimer_S()
	{
		m_fHardFreezeRemaining -= 1.0;

		if (m_fHardFreezeRemaining <= 0)
		{
			EndHardFreeze_S();
			return;
		}

		Replication.BumpMe();
	}

	// Signed delta: positive extends or revives a lapsed freeze, negative shortens; a
	// result <= 0 ends it. Clients react through the replicated scalar.
	void AdjustFreezeTime_S(int seconds)
	{
		if (!Replication.IsServer())
			return;
		if (GetState() != SCR_EGameModeState.GAME || seconds == 0)
			return;

		// Checked before mutating so EndFreezeTime_S still sees a running freeze.
		if (m_fFreezeTimeRemaining + seconds <= 0)
		{
			EndFreezeTime_S();
			return;
		}

		m_fFreezeTimeRemaining += seconds;

		// A lapsed freeze revived here needs its tick back; during a hard freeze the release
		// schedules it instead.
		if (!m_bHardFreeze)
			ScheduleFreezeCountdown_S();

		Replication.BumpMe();
		Print(string.Format("[LL_Lobby] Freeze time adjusted by %1s (now %2s)", seconds, m_fFreezeTimeRemaining), LogLevel.NORMAL);
	}

	// Zeroing the replicated scalar stops every client's zone and HUD timer.
	void EndFreezeTime_S()
	{
		if (!Replication.IsServer())
			return;
		if (m_fFreezeTimeRemaining <= 0)
			return;

		m_fFreezeTimeRemaining = 0;
		CancelFreezeCountdown_S();
		OnFreezeTimeEnded_S();
		Replication.BumpMe();
	}

	// With the switch off no save of any type happens, whatever the header says. With it
	// on, every missing precondition is logged so a misconfiguration never reads as a
	// fresh start.
	protected void InitSessionSaves_S()
	{
		SaveGameManager saveManager = GetGame().GetSaveGameManager();
		if (!saveManager)
			return;

		if (!m_bSessionSaves)
		{
			saveManager.SetEnabledSaveTypes(0);
			return;
		}

		// Nothing before GAME is worth a snapshot; the phase gate opens it.
		saveManager.SetSavingAllowed(false);

		if (saveManager.GetEnabledSaveTypes() == 0)
			Print("[LL_Lobby] Session saves: save types are unticked on the mission header", LogLevel.WARNING);

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			Print("[LL_Lobby] Session saves: mission header names no lobby systems config", LogLevel.WARNING);
		else if (!persistence.FindCollection("LL_Lobby"))
			Print("[LL_Lobby] Session saves: systems config is not the lobby's", LogLevel.WARNING);

		SCR_PersistenceSystem scripted = SCR_PersistenceSystem.GetScriptedInstance();
		if (scripted)
		{
			scripted.GetOnBeforeSave().Insert(OnBeforeSnapshot_S);
			scripted.GetOnAfterSave().Insert(OnAfterSnapshot_S);
		}

		SaveGame activeSave = saveManager.GetActiveSave();
		if (!activeSave)
		{
			Print("[LL_Lobby] Resume: no snapshot loaded", LogLevel.NORMAL);
			return;
		}

		int year, month, day, hour, minute, second;
		activeSave.GetSavePointCreatedLocalDateTime(year, month, day, hour, minute, second);
		Print(string.Format("[LL_Lobby] Resume: snapshot %1 %2-%3-%4 %5:%6:%7 active",
			activeSave.GetId(), year, month.ToString(2), day.ToString(2),
			hour.ToString(2), minute.ToString(2), second.ToString(2)), LogLevel.NORMAL);
	}

	protected void OnBeforeSnapshot_S(ESaveGameType saveType)
	{
		m_iSnapshotStartTick = System.GetTickCount();
	}

	protected void OnAfterSnapshot_S(ESaveGameType saveType, bool success)
	{
		string result = "ok";
		if (!success)
			result = "failed";

		Print(string.Format("[LL_Lobby] Snapshot %1 %2 in %3 ms",
			typename.EnumToString(ESaveGameType, saveType), result,
			System.GetTickCount() - m_iSnapshotStartTick), LogLevel.NORMAL);

		// The type alone marks an exit save, whichever side requested it and whether it ran
		// before or after the game-end event; the engine's exit transition would otherwise
		// write a second one.
		if (saveType == ESaveGameType.SHUTDOWN && success)
		{
			m_bStopSaveDone = true;
			GetGame().GetSaveGameManager().SetSavingAllowed(false);
		}
	}

	// A failed native load is a broken world: refused before any lobby record is read.
	void OnNativeLoadResult_S(bool success)
	{
		if (success)
			return;

		if (!m_bSessionSaves)
		{
			Print("[LL_Lobby] Resume: the game reported a failed load (session saves are off)", LogLevel.WARNING);
			return;
		}

		RefuseResume_S("load failed");
	}

	// Idempotent. Nothing is purged, so an older snapshot stays available; saving is
	// disabled first so the close's own shutdown save cannot write the half-loaded world.
	void RefuseResume_S(string cause)
	{
		if (m_bResumeRefused)
			return;
		m_bResumeRefused = true;

		SaveGameManager saveManager = GetGame().GetSaveGameManager();
		if (saveManager)
		{
			saveManager.SetSavingAllowed(false);
			saveManager.SetEnabledSaveTypes(0);
		}

		GetGame().GetCallqueue().Remove(ResumeCheck_S);

		Print(string.Format("[LL_Lobby] Resume refused: %1", cause), LogLevel.ERROR);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.BroadcastAdminMessage_S("#LL-Resume_Refused");

		GetGame().RequestClose();
	}

	//! Safe from any release in any state; the roster check runs once per game phase.
	void AllowSavesIfReady_S()
	{
		if (!m_bSessionSaves || !Replication.IsServer())
			return;
		if (GetState() != SCR_EGameModeState.GAME || !m_bStartupBatchDispatched)
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr && mgr.GetPendingReplacements() > 0)
			return;

		if (!m_bRosterCheckedForSaves)
		{
			m_bRosterCheckedForSaves = true;
			CheckRosterForSaves_S();
		}

		GetGame().GetSaveGameManager().SetSavingAllowed(true);
	}

	void DisallowSaves_S()
	{
		if (!m_bSessionSaves || !Replication.IsServer())
			return;

		GetGame().GetSaveGameManager().SetSavingAllowed(false);
	}

	//! Admin /snapshot. The request queues behind the same gate as the autosave.
	void RequestSnapshot_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (!m_bSessionSaves)
		{
			Print(string.Format("[LL_Lobby] Snapshot requested by admin %1 ignored: session saves are off", playerId), LogLevel.WARNING);
			return;
		}

		Print(string.Format("[LL_Lobby] Snapshot requested by admin %1", playerId), LogLevel.NORMAL);
		GetGame().GetSaveGameManager().RequestSavePoint(ESaveGameType.SCRIPTED);
	}

	protected void CloseSavePhase_S()
	{
		m_bStartupBatchDispatched = false;
		m_bRosterCheckedForSaves = false;
		DisallowSaves_S();
	}

	// The mission is over, so nothing stays resumable. The game's own end-of-game handler is
	// protected and skipped outside a dedicated build, so its body runs here, after any save
	// in flight, and for the current playthrough only. A refusal never comes through here.
	protected void DiscardSnapshots_S()
	{
		if (!m_bSessionSaves || !Replication.IsServer())
			return;

		SaveGameManager saveManager = GetGame().GetSaveGameManager();
		if (!saveManager)
			return;
		saveManager.SetSavingAllowed(false);

		LL_SaveWaiter waiter = new LL_SaveWaiter();
		waiter.GetOnReady().Insert(OnDiscardReady_S);
		waiter.Start();
	}

	protected void OnDiscardReady_S(Managed context)
	{
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
			persistence.ClearStorage(PersistenceSessionStorage);

		SaveGameManager saveManager = GetGame().GetSaveGameManager();
		if (saveManager)
			saveManager.Purge(SaveGameManager.GetCurrentMissionResource(), saveManager.GetCurrentPlaythroughNumber());

		Print("[LL_Lobby] Session saves: snapshots discarded at debriefing", LogLevel.NORMAL);
	}

	// The game's own exit flow minus the menu: wait for an idle manager, request the shutdown
	// save once, and skip it when the engine's own exit save already completed. Nothing in
	// script can delay the process exit, so the request is blocking.
	override void OnGameEnd()
	{
		super.OnGameEnd();

		if (!Replication.IsServer() || !m_bSessionSaves || m_bStopSaveRequested)
			return;
		if (GetState() != SCR_EGameModeState.GAME)
			return;

		m_bStopSaveRequested = true;

		LL_SaveWaiter waiter = new LL_SaveWaiter();
		waiter.GetOnReady().Insert(OnStopSaveReady_S);
		waiter.Start();
	}

	protected void OnStopSaveReady_S(Managed context)
	{
		if (m_bStopSaveDone)
			return;

		Print("[LL_Lobby] Stop: requesting the shutdown snapshot", LogLevel.NORMAL);
		GetGame().GetSaveGameManager().RequestSavePoint(ESaveGameType.SHUTDOWN, string.Empty, ESaveGameRequestFlags.BLOCKING);
	}

	// What a resume cannot recover from, logged once by name: squads are re-attached by
	// entity name, bodies found by persistence id, one mission-end timer assumed.
	protected void CheckRosterForSaves_S()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!mgr || !persistence)
			return;

		// Entity name → callsign it was first seen under.
		map<string, string> namesSeen = new map<string, string>();
		set<int> groupsChecked = new set<int>();
		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (groupsChecked.Find(slot.m_iGroupId) == -1)
			{
				groupsChecked.Insert(slot.m_iGroupId);

				IEntity groupEntity = null;
				RplComponent groupRpl = RplComponent.Cast(Replication.FindItem(slot.m_iGroupId));
				if (groupRpl)
					groupEntity = groupRpl.GetEntity();

				if (groupEntity)
				{
					string name = groupEntity.GetName();
					if (name == "")
						Print(string.Format("[LL_Lobby] Session saves: squad '%1' has no entity name; its members cannot be re-attached on a resume", slot.m_sGroupName), LogLevel.WARNING);
					else if (namesSeen.Contains(name))
						Print(string.Format("[LL_Lobby] Session saves: squads '%1' and '%2' share the entity name '%3'", namesSeen.Get(name), slot.m_sGroupName, name), LogLevel.WARNING);
					else
						namesSeen.Set(name, slot.m_sGroupName);
				}
			}

			IEntity body = mgr.GetSlotEntity_S(slot.m_iRplId);
			if (body && persistence.GetId(body).IsNull())
				Print(string.Format("[LL_Lobby] Session saves: slot '%1' body is not tracked by the persistence system; a snapshot with it refuses to resume", slot.m_sName), LogLevel.WARNING);
		}

		// Exact class: timed announcements inherit the timer and may be many.
		int timers = 0;
		foreach (LL_TriggerComponent trigger : LL_TriggerComponent.GetAll())
		{
			if (!trigger || trigger.Type() != LL_TriggerMissionEndTimer)
				continue;

			LL_TriggerMissionEndTimer timer = LL_TriggerMissionEndTimer.Cast(trigger);

			timers++;
			if (timers > 1)
				Print(string.Format("[LL_Lobby] Session saves: second mission-end timer '%1'; one per mission is supported", timer.GetStatKey()), LogLevel.WARNING);
			if (timer.GetDuration() <= 0)
				Print(string.Format("[LL_Lobby] Session saves: mission-end timer '%1' has no positive duration", timer.GetStatKey()), LogLevel.WARNING);
		}
	}

	static bool IsResumePending()
	{
		LL_GameModeCoop gameMode = GetInstance();
		return gameMode && gameMode.m_bResumePending;
	}

	// Bodies and records arrive while the world loads, before the game start; after the
	// start the decision is kept, because a snapshot taken later in this session is also
	// the manager's active save.
	protected bool WillResume()
	{
		if (m_bGameStarted)
			return m_bResumeBegun;

		return m_bSessionSaves && GetGame().GetSaveGameManager().GetActiveSave() != null;
	}

	//! True from world load until the record is applied or refused; bodies spawned later
	//! register the ordinary way.
	bool IsResumeInProgress()
	{
		if (m_bResumeRefused)
			return false;
		if (m_bResumeFinalising && !m_bResumePending)
			return false;

		return WillResume();
	}

	float GetResumeDeadline()		{ return m_fResumeDeadline; }
	int GetMissionEndDuration()		{ return m_iMissionEndDuration; }
	float GetMissionEndStartedAt()	{ return m_fMissionEndStartedAt; }
	float GetResumedSnapshotTime()	{ return m_fResumedSnapshotTime; }

	// Set once: a second timer or a restarted countdown must not move the spectator clock.
	void SetMissionEndDuration_S(int seconds)
	{
		if (!Replication.IsServer() || m_bMissionEndDurationSet)
			return;

		m_bMissionEndDurationSet = true;
		m_iMissionEndDuration = seconds;
		Replication.BumpMe();
	}

	void SetMissionEndStartedAt_S(float clockReading)
	{
		if (!Replication.IsServer() || m_bMissionEndStartSet)
			return;

		m_bMissionEndStartSet = true;
		m_fMissionEndStartedAt = clockReading;
		Replication.BumpMe();
	}

	// The live remainder of a timed hard freeze, or the remainder a resume hold protects.
	float GetSavedHardFreezeRemaining()
	{
		if (!m_bHardFreeze)
			return 0;
		if (IsResumeHoldActive())
			return m_fPreHoldHardFreezeRemaining;

		return m_fHardFreezeRemaining;
	}

	// The daylight intent, never the disabled value a hold writes.
	bool GetPreHoldDayAdvance()
	{
		if (IsResumeHoldActive())
			return m_bPreHoldDayAdvance;
		if (m_bHardFreeze)
			return m_bDayAdvanceWasOn;

		TimeAndWeatherManagerEntity timeManager = GetTimeManager();
		return timeManager && timeManager.GetIsDayAutoAdvanced();
	}

	void SetResumeRecord_S(LL_SessionRecord record)
	{
		m_ResumeRecord = record;
	}

	//! The first cause wins; the finaliser turns it into the refusal.
	void ReportResumeFailure_S(string cause)
	{
		if (m_sResumeFailure != "")
			return;

		m_sResumeFailure = cause;
		Print(string.Format("[LL_Lobby] Resume: failure recorded: %1", cause), LogLevel.WARNING);
	}

	// A negative remainder is the hold with no countdown: the HUD shows the resume text instead
	// of a timer, and only the admin releases it (research R5).
	protected void StartResumeHold_S()
	{
		m_bHardFreeze = true;
		m_fHardFreezeRemaining = -1;
		Replication.BumpMe();
	}

	// The hold is engaged before anything loads so the damage gate and the input lock cover
	// every restored entity. No day-advance capture: the weather record is not applied yet.
	protected void BeginResume_S()
	{
		m_bResumePending = true;
		StartResumeHold_S();

		SCR_PersistenceSystem scripted = SCR_PersistenceSystem.GetScriptedInstance();
		if (scripted)
			scripted.GetOnStateChanged().Insert(OnPersistenceStateChanged_S);

		Print("[LL_Lobby] Resume: hold engaged, waiting for the world to load", LogLevel.NORMAL);

		// The order of ACTIVE against the game start is not established.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			return;

		EPersistenceSystemState state = persistence.GetState();
		if (state == EPersistenceSystemState.ACTIVE)
			FinaliseResume_S();
		else if (state > EPersistenceSystemState.ACTIVE)
			RefuseResume_S("persistence system failed before the game started");
	}

	protected void OnPersistenceStateChanged_S(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		if (newState == EPersistenceSystemState.ACTIVE)
			FinaliseResume_S();
		else if (newState == EPersistenceSystemState.FAILURE)
			RefuseResume_S("persistence system failure");
	}

	//! Once at ACTIVE: arms the deadline and waits for every saved body to register.
	void FinaliseResume_S()
	{
		if (m_bResumeFinalising || !m_bResumePending)
			return;
		m_bResumeFinalising = true;

		m_fResumeDeadline = System.GetTickCount() + RESUME_DEADLINE_MS;

		if (!m_ResumeRecord)
		{
			RefuseResume_S("no lobby record in the snapshot");
			return;
		}

		GetGame().GetCallqueue().CallLater(ResumeCheck_S, 250, true);
		ResumeCheck_S();
	}

	protected void ResumeCheck_S()
	{
		if (m_bResumeRefused)
		{
			GetGame().GetCallqueue().Remove(ResumeCheck_S);
			return;
		}

		if (m_sResumeFailure != "")
		{
			RefuseResume_S(m_sResumeFailure);
			return;
		}

		string pending = "";
		int pendingCount = 0;
		foreach (LL_SlotRecord slot : m_ResumeRecord.slots)
		{
			if (IsResumeBodyRegistered_S(slot))
				continue;

			pendingCount++;
			if (pending != "")
				pending += ", ";
			pending += slot.name;
		}

		if (pendingCount > 0)
		{
			if (System.GetTickCount() < m_fResumeDeadline)
				return;

			RefuseResume_S(string.Format("%1 body(ies) did not register in time: %2", pendingCount, pending));
			return;
		}

		GetGame().GetCallqueue().Remove(ResumeCheck_S);
		CompleteResume_S();
	}

	// Done = the manager holds a slot for the body in the saved squad.
	protected bool IsResumeBodyRegistered_S(LL_SlotRecord record)
	{
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			return false;

		IEntity body = IEntity.Cast(persistence.FindById(record.body));
		if (!body)
			return false;

		LL_PlayableComponent playable = LL_PlayableComponent.Cast(body.FindComponent(LL_PlayableComponent));
		return playable && playable.IsResumeRegistered(record.squad);
	}

	protected void CompleteResume_S()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!mgr || !persistence)
		{
			RefuseResume_S("lobby manager missing");
			return;
		}

		LL_SessionRecord record = m_ResumeRecord;

		// Frequencies before any possession: a body's radio is tuned as its holder enters.
		map<string, int> frequencies = new map<string, int>();
		foreach (LL_SquadRecord squad : record.squads)
			frequencies.Set(squad.name, squad.frequency);
		mgr.RestoreSquadFrequencies_S(frequencies);

		int seated = 0;
		foreach (LL_SlotRecord slot : record.slots)
		{
			IEntity body = IEntity.Cast(persistence.FindById(slot.body));
			if (!body)
				continue;

			LL_PlayableComponent playable = LL_PlayableComponent.Cast(body.FindComponent(LL_PlayableComponent));
			if (!playable)
				continue;

			int slotRplId = playable.GetRplId();
			mgr.ApplyResumedSlotFlags_S(slotRplId, slot.kia, slot.locked);
			if (slot.holderKey != "")
			{
				mgr.SeatResumedHolder_S(slot.holderKey, slot.holderName, slotRplId);
				seated++;
			}
		}

		// Runs the components' GAME hooks; the recorder and the playable registration read
		// the pending flag and stay out.
		SetGameModeState(SCR_EGameModeState.GAME);

		// After the state change: the base game mode resets the clock across it.
		m_fTimeElapsed = record.elapsedSeconds;
		m_fHeldElapsed = record.elapsedSeconds;
		m_fResumedSnapshotTime = record.elapsedSeconds;
		m_fGameStartTimestamp = System.GetTickCount() - record.elapsedSeconds * 1000;
		m_fFreezeTimeRemaining = record.freezeRemaining;
		m_fPreHoldHardFreezeRemaining = record.hardFreezeRemaining;
		m_bPreHoldDayAdvance = record.dayAdvance;

		if (record.missionEndDuration > 0)
			SetMissionEndDuration_S(record.missionEndDuration);
		if (record.missionEndStartedAt >= 0)
			SetMissionEndStartedAt_S(record.missionEndStartedAt);
		Replication.BumpMe();

		// Without a capture: the record's intent, not the restored live setting, is what
		// the release puts back.
		TimeAndWeatherManagerEntity timeManager = GetTimeManager();
		if (timeManager)
			timeManager.SetIsDayAutoAdvanced(false);

		// The lobby's freeze zones follow the countdown; the stock zones need the removal.
		if (m_fFreezeTimeRemaining <= 0)
			RemoveVanillaRestrictionZones_S();

		// The hold was engaged before the world existed; the restored bodies are here now.
		m_VehicleHold.PinAll_S();
		m_DamagePause.PauseAll_S();

		// Whoever connected during the load found no reservation to claim.
		mgr.ClaimResumedSlotsForConnected_S();

		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			stats.ResumeRecording_S(record.stats);

		m_bStartupBatchDispatched = true;
		m_bResumePending = false;
		AllowSavesIfReady_S();

		Print(string.Format("[LL_Lobby] Resume: %1 slot(s) restored, %2 holder(s) seated, clock at %3 s, hold active until released",
			record.slots.Count(), seated, record.elapsedSeconds), LogLevel.NORMAL);

		mgr.BroadcastAdminMessage_S("#LL-Resume_HoldBody", "#LL-Resume_HoldTitle");
	}

	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);
		Print(string.Format("[LL_Lobby] Player connected: %1", playerId), LogLevel.NORMAL);

		// Deferred past the reconnect reservation (ReconnectPlayer at +500 ms); a slot-less
		// player in GAME goes to spectator.
		if (GetState() == SCR_EGameModeState.GAME)
			GetGame().GetCallqueue().CallLater(CheckJipSpectator_S, 2000, false, playerId);
	}

	protected void CheckJipSpectator_S(int playerId)
	{
		if (GetState() != SCR_EGameModeState.GAME)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm || !pm.IsPlayerConnected(playerId))
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr && mgr.FindSlotByPlayerId(playerId))
			return;

		SendPlayerToSpectator_S(playerId);
	}

	// OnPlayerDisconnected is not overridden: vanilla handles the vehicle gentle-stop and,
	// with the modded SCR_ReconnectComponent, keeps the slot body alive.

	// During GAME the dead player spectates; the slot stays assigned (KIA), the corpse stays.
	override protected void OnPlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled(playerId, playerEntity, killerEntity, killer);

		if (GetState() != SCR_EGameModeState.GAME)
			return;

		SendPlayerToSpectator_S(playerId);
	}

	// VoN moves to the muted spectator channel, then that client builds camera and menu locally.
	void SendPlayerToSpectator_S(int playerId)
	{
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
			vonMgr.SetPlayerSpectator_S(playerId);

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetByPlayerId(playerId);
		if (lobbyPlayer)
			lobbyPlayer.NotifySpectatorEnter_S();
	}

	void AdvanceState_S(int playerId)
	{
		if (m_bAdminMode && !SCR_Global.IsAdmin(playerId))
			return;

		Print(string.Format("[LL_Lobby] Admin %1 advancing state", playerId), LogLevel.NORMAL);
		AdvanceLobbyState();
	}
}

void LL_SaveWaiterCallback(Managed context);
typedef func LL_SaveWaiterCallback;
typedef ScriptInvokerBase<LL_SaveWaiterCallback> LL_SaveWaiterInvoker;

// One-shot wait for an idle save manager with no body replacement in flight. The
// after-save event is not an idle signal: it fires for every save type, the shutdown one
// included. The callback goes through an invoker: a script method cannot take a func.
class LL_SaveWaiter
{
	protected static ref array<ref LL_SaveWaiter> s_aWaiters = {};

	protected ref LL_SaveWaiterInvoker m_OnReady = new LL_SaveWaiterInvoker();
	protected ref Managed m_Context;
	protected bool m_bDone;

	void LL_SaveWaiter(Managed context = null)
	{
		m_Context = context;
	}

	LL_SaveWaiterInvoker GetOnReady()
	{
		return m_OnReady;
	}

	//! Registers the waiter and evaluates it at once; may run the callback before returning.
	void Start()
	{
		s_aWaiters.Insert(this);
		EventProvider.ConnectEvent(GetGame().GetSaveGameManager().OnBusyStateChanged, OnManagerBusyChanged);
		Evaluate();
	}

	static bool IsIdle()
	{
		SaveGameManager saveManager = GetGame().GetSaveGameManager();
		if (saveManager && saveManager.IsBusy())
			return false;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		return !mgr || mgr.GetPendingReplacements() == 0;
	}

	// In order: an earlier callback may start a replacement that keeps the rest waiting.
	// The copy is strong because a waiter leaves the list as it runs.
	static void ReevaluateAll()
	{
		array<ref LL_SaveWaiter> pending = {};
		foreach (LL_SaveWaiter waiter : s_aWaiters)
			pending.Insert(waiter);

		foreach (LL_SaveWaiter waiter : pending)
			waiter.Evaluate();
	}

	[ReceiverAttribute()]
	protected void OnManagerBusyChanged(bool busy)
	{
		if (!busy)
			Evaluate();
	}

	protected void Evaluate()
	{
		if (m_bDone || !IsIdle())
			return;
		m_bDone = true;

		// The list holds the only reference; leaving it must not free the running object.
		ref LL_SaveWaiter keepAlive = this;
		EventProvider.DisconnectEvent(GetGame().GetSaveGameManager().OnBusyStateChanged, OnManagerBusyChanged);
		s_aWaiters.RemoveItem(this);

		m_OnReady.Invoke(m_Context);
	}
}