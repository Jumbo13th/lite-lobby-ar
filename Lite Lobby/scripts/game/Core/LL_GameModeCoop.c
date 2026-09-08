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

	[Attribute("1", UIWidgets.CheckBox, "Hard freeze at GAME start: hold everyone's controls for a moment so the world can stream in around them before anyone can move. Runs inside the freeze window.", category: "Lite Lobby")]
	protected bool m_bHardFreezeEnabled;

	[Attribute("60000", UIWidgets.EditBox, "Hard freeze duration (ms) at GAME start. Should stay well inside the freeze time above.", category: "Lite Lobby")]
	protected int m_iHardFreezeTime;

	[Attribute("1", UIWidgets.CheckBox, "During freeze time players can't fire their weapons and take no damage (a protected setup period). They can still be killed by leaving the freeze zone.", category: "Lite Lobby")]
	protected bool m_bForbidShootingDuringFreeze;

	[Attribute("0", UIWidgets.CheckBox, "Remove AI units not occupied by players when GAME starts.", category: "Lite Lobby")]
	protected bool m_bRemoveRedundantUnits;

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

	// Hard freeze locks character controls, suspends the mission logic and the day/night
	// clock. Script cannot stop AI, vehicles or physics (no vehicle-control disable, no
	// world time-scale setter), so a hold engaged mid-round misses anyone already driving.
	[RplProp()]
	protected bool m_bHardFreeze;

	// Seconds of hold left. Only meaningful while m_bHardFreeze.
	[RplProp()]
	protected float m_fHardFreezeRemaining;

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

	// The countdown reads > 0 only during the post-start freeze.
	bool IsFreezeSafetyActive()
	{
		return m_bForbidShootingDuringFreeze && m_fFreezeTimeRemaining > 0;
	}
	float GetHardFreezeRemaining()	{ return m_fHardFreezeRemaining; }
	bool IsHardFreezeEnabled()		{ return m_bHardFreezeEnabled; }
	int GetHardFreezeDuration()		{ return m_iHardFreezeTime; }

	bool RemoveRedundantUnits()	{ return m_bRemoveRedundantUnits; }
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
			EndHardFreeze_S();

		switch (state)
		{
			case SCR_EGameModeState.GAME:
				OnEnterGame_S();
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

		if (m_iFreezeTime > 0)
		{
			GetGame().GetCallqueue().CallLater(UpdateFreezeTimer_S, 1000, true);
		}

		// Runs inside the freeze window; the freeze zones still confine movement once it lifts.
		if (m_bHardFreezeEnabled && m_iHardFreezeTime > 0)
			StartHardFreeze_S(m_iHardFreezeTime / 1000);

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

	protected void UpdateFreezeTimer_S()
	{
		m_fFreezeTimeRemaining -= 1.0;

		if (m_fFreezeTimeRemaining <= 0)
		{
			m_fFreezeTimeRemaining = 0;
			GetGame().GetCallqueue().Remove(UpdateFreezeTimer_S);
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
			StopDayAdvance_S();

		m_bHardFreeze = true;
		m_fHardFreezeRemaining = seconds;
		Replication.BumpMe();

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

		GetGame().GetCallqueue().Remove(UpdateHardFreezeTimer_S);
		m_bHardFreeze = false;
		m_fHardFreezeRemaining = 0;
		Replication.BumpMe();

		// Runs exactly once per hold: timer drain, admin /hardfreeze 0, and leaving GAME.
		RestoreDayAdvance_S();

		Print("[LL_Lobby] Hard freeze ended", LogLevel.NORMAL);
	}

	// The day/night clock is the one part of the world script can stop during a hold.
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

		// CallLater does not de-duplicate.
		GetGame().GetCallqueue().Remove(UpdateFreezeTimer_S);
		GetGame().GetCallqueue().CallLater(UpdateFreezeTimer_S, 1000, true);

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
		GetGame().GetCallqueue().Remove(UpdateFreezeTimer_S);
		OnFreezeTimeEnded_S();
		Replication.BumpMe();
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