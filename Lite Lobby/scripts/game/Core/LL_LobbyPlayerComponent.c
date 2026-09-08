// The only client → server channel: every RpcAsk_* lands here and is re-validated on
// the authority before it reaches a manager. Holds no lobby data. Also hosts the local
// chat commands and the per-client HUD widgets.

class LL_LobbyPlayerComponentClass : ScriptComponentClass
{
}

class LL_LobbyPlayerComponent : ScriptComponent
{
	// Local client only; shown by its handler from the replicated countdown.
	protected Widget m_wFreezeTimerHud;
	protected ResourceName m_sFreezeTimerLayout = "{1D12C3D4E5F6A7B8}UI/HUD/FreezeTimeCounter.layout";

	// Same lifetime as the freeze HUD.
	protected Widget m_wHardFreezeHud;
	protected ResourceName m_sHardFreezeLayout = "{1D14E5F6A7B8C9DA}UI/HUD/PauseOverlay.layout";

	protected bool m_bChatCommandsRegistered;

	// Nobody is possessed before GAME and the engine registers no camera, so the view
	// and the audio listener have to be provided locally.
	protected ref LL_LobbyCamera m_LobbyCamera;

	static LL_LobbyPlayerComponent GetByPlayerId(int playerId)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return null;

		PlayerController pc = pm.GetPlayerController(playerId);
		if (!pc)
			return null;

		return LL_LobbyPlayerComponent.Cast(pc.FindComponent(LL_LobbyPlayerComponent));
	}

	static LL_LobbyPlayerComponent GetLocalInstance()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;

		return LL_LobbyPlayerComponent.Cast(pc.FindComponent(LL_LobbyPlayerComponent));
	}

	int GetPlayerId()
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return -1;

		return pc.GetPlayerId();
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		// GetGame().GetPlayerController() may still be null here; the local-player guard
		// sits in the callbacks.
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode)
		{
			gameMode.GetOnGameStateChanged().Insert(OnGameStateChanged_Client);
			Print("[LL_Lobby] LobbyPlayerComponent: subscribed to state changes", LogLevel.NORMAL);
		}
		else
		{
			Print("[LL_Lobby] LobbyPlayerComponent: WARNING — LL_GameModeCoop not found at init time", LogLevel.WARNING);
		}

		// The state may already be SLOTSELECTION (JIP); OpenMenu needs a frame.
		GetGame().GetCallqueue().CallLater(CheckAndOpenLobby, 100, false);

		// A listen host has one of these components per connected player; the local-player
		// guard sits in the callbacks.
		if (!System.IsConsoleApp())
		{
			InputManager inputManager = GetGame().GetInputManager();
			if (inputManager)
			{
				inputManager.AddActionListener("LL_EffectsVolumeDown", EActionTrigger.DOWN, OnEffectsVolumeDown);
				inputManager.AddActionListener("LL_EffectsVolumeUp", EActionTrigger.DOWN, OnEffectsVolumeUp);
			}
		}
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode)
			gameMode.GetOnGameStateChanged().Remove(OnGameStateChanged_Client);

		LL_LobbyManager lobbyMgr = LL_LobbyManager.GetInstance();
		if (lobbyMgr)
			lobbyMgr.GetOnStatsPublished().Remove(OnStatsPublishedChanged_Local);

		if (m_wFreezeTimerHud)
		{
			m_wFreezeTimerHud.RemoveFromHierarchy();
			m_wFreezeTimerHud = null;
		}

		if (m_wHardFreezeHud)
		{
			m_wHardFreezeHud.RemoveFromHierarchy();
			m_wHardFreezeHud = null;
		}

		UnregisterChatCommands();

		if (m_LobbyCamera)
		{
			m_LobbyCamera.Hide();
			m_LobbyCamera = null;
		}

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener("LL_EffectsVolumeDown", EActionTrigger.DOWN, OnEffectsVolumeDown);
			inputManager.RemoveActionListener("LL_EffectsVolumeUp", EActionTrigger.DOWN, OnEffectsVolumeUp);
		}
	}

	protected void OnEffectsVolumeDown(float value, EActionTrigger reason)
	{
		if (IsLocalPlayer())
			LL_EffectsVolume.Step(-1);
	}

	protected void OnEffectsVolumeUp(float value, EActionTrigger reason)
	{
		if (IsLocalPlayer())
			LL_EffectsVolume.Step(1);
	}

	// Evaluated lazily: the local controller is null at OnPostInit.
	protected bool IsLocalPlayer()
	{
		// No log here: on a dedicated server the local controller is always null and this
		// runs per player per state change.
		PlayerController local = GetGame().GetPlayerController();
		if (!local)
			return false;

		return local == GetOwner();
	}

	protected void OnGameStateChanged_Client(int state)
	{
		// Subscribed by every controller instance; only the local one opens UI.
		if (!IsLocalPlayer())
			return;

		SCR_EGameModeState gameState = state;
		OpenMenuForState(gameState);
	}

	// Polls: the local controller lags component init, and the authority sets the initial
	// state before any controller subscribed, so the first state event is missed.
	protected void CheckAndOpenLobby()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		PlayerController localPC = GetGame().GetPlayerController();

		if (!localPC)
		{
			GetGame().GetCallqueue().CallLater(CheckAndOpenLobby, 200, false);
			return;
		}

		// A different player's controller (the server holds one per player).
		if (localPC != GetOwner())
			return;

		EnsureFreezeTimerHud();
		EnsureHardFreezeHud();
		RegisterChatCommands();
		SubscribeStatsEvents();

		// Gaining or losing a character flips who owns the microphone.
		SCR_PlayerController scrPC = SCR_PlayerController.Cast(localPC);
		if (scrPC)
			scrPC.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged_Local);

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (!gameMode)
		{
			GetGame().GetCallqueue().CallLater(CheckAndOpenLobby, 200, false);
			return;
		}

		OpenMenuForState(gameMode.GetState());
	}

	protected void OnControlledEntityChanged_Local(IEntity from, IEntity to)
	{
		LL_MenuVoN.Refresh();
	}

	// A real state change re-syncs everyone to the stage's menu, overriding a local preview.
	protected void OpenMenuForState(SCR_EGameModeState state)
	{
		// Not in SwitchToMenu: an admin previewing the slotting stage mid-game must not have
		// their view moved off their character.
		if (!m_LobbyCamera)
			m_LobbyCamera = new LL_LobbyCamera();

		m_LobbyCamera.UpdateForState(state);

		SwitchToMenu(state);
	}

	// Stage-view switching for both real state changes and the header-tab previews.
	void SwitchToMenu(SCR_EGameModeState state)
	{
		// Idempotent; every stage view change may need the menu talking device.
		LL_MenuVoN.Refresh();

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		menuManager.CloseMenuByPreset(ChimeraMenuPreset.CoopLobby);
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.SpectatorMenu);

		if (state == SCR_EGameModeState.SLOTSELECTION)
		{
			menuManager.OpenMenu(ChimeraMenuPreset.CoopLobby);
			Print("[LL_Lobby] Switched to lobby menu", LogLevel.NORMAL);
		}
		else if (state == SCR_EGameModeState.BRIEFING)
		{
			menuManager.OpenMenu(ChimeraMenuPreset.BriefingMapMenu);
			Print("[LL_Lobby] Switched to briefing menu", LogLevel.NORMAL);
		}
		else if (state == SCR_EGameModeState.GAME)
		{
			// For a spectating player the GAME view is the spectator screen, not bare gameplay.
			LL_SpectatorManager spectatorMgr = LL_SpectatorManager.GetInstance();
			if (spectatorMgr && spectatorMgr.IsSpectating())
			{
				menuManager.OpenMenu(ChimeraMenuPreset.SpectatorMenu);
				Print("[LL_Lobby] Switched back to spectator menu", LogLevel.NORMAL);
			}
		}

		// Re-raise the force-held stats screen over the new stage menu, only if it was open.
		LL_LobbyManager lobbyMgr = LL_LobbyManager.GetInstance();
		if (lobbyMgr && lobbyMgr.IsStatsPublished()
			&& menuManager.FindMenuByPreset(ChimeraMenuPreset.StatsScreenMenu))
		{
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.StatsScreenMenu);
			LL_StatsScreen.Open();
		}
	}

	protected void EnsureFreezeTimerHud()
	{
		if (m_wFreezeTimerHud)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wFreezeTimerHud = workspace.CreateWidgets(m_sFreezeTimerLayout, null);
	}

	protected void EnsureHardFreezeHud()
	{
		if (m_wHardFreezeHud)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wHardFreezeHud = workspace.CreateWidgets(m_sHardFreezeLayout, null);
	}

	void AskTakeSlot(int slotRplId)
	{
		Rpc(RpcAsk_TakeSlot, slotRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeSlot(int slotRplId)
	{
		int playerId = GetPlayerId();

		// Slots are locked in once the round is live (death is final); reconnect and admin
		// assignment call TakeSlot_S directly. Admins keep self-service for moderation.
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && gameMode.GetLobbyState() == SCR_EGameModeState.GAME && !SCR_Global.IsAdmin(playerId))
		{
			Print(string.Format("[LL_Lobby] RpcAsk_TakeSlot rejected: player %1 cannot take slot %2 during GAME (no mid-game slot switching)", playerId, slotRplId), LogLevel.NORMAL);
			return;
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		bool success = mgr.TakeSlot_S(playerId, slotRplId);
		if (!success)
		{
			Print(string.Format("[LL_Lobby] RpcAsk_TakeSlot denied: player=%1 slot=%2", playerId, slotRplId), LogLevel.WARNING);
		}
	}

	void AskLeaveSlot()
	{
		Rpc(RpcAsk_LeaveSlot);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LeaveSlot()
	{
		int playerId = GetPlayerId();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		// Mid-game release is admin-only; see LL_LobbyManager.ReleaseSlotInGame_S.
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && gameMode.GetLobbyState() == SCR_EGameModeState.GAME)
		{
			if (!SCR_Global.IsAdmin(playerId))
			{
				Print(string.Format("[LL_Lobby] RpcAsk_LeaveSlot rejected: player %1 cannot release a slot during GAME", playerId), LogLevel.NORMAL);
				return;
			}

			mgr.ReleaseSlotInGame_S(playerId);
			return;
		}

		mgr.LeaveSlot_S(playerId);
	}

	// The connect-time name broadcasts can race a joining client's stream-in of the
	// manager, so the client pulls the roster when its lobby opens. Owner-targeted replies.

	void AskSyncRoster()
	{
		// Snapshot the local roster so the reply can prune ghosts.
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.BeginRosterSync();

		Rpc(RpcAsk_SyncRoster);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SyncRoster()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		array<int> playerIds = {};
		mgr.GetKnownPlayerIds(playerIds);

		foreach (int pid : playerIds)
		{
			Rpc(RpcDo_OwnerSyncPlayer, pid, mgr.GetPlayerName(pid), mgr.IsPlayerReady(pid), mgr.IsPlayerDisconnected(pid));
		}

		// Reliable and ordered, so it arrives after every entry above.
		Rpc(RpcDo_OwnerSyncComplete);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerSyncPlayer(int playerId, string name, bool ready, bool disconnected)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.SyncPlayerFromServer(playerId, name, ready, disconnected);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerSyncComplete()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.EndRosterSync();
	}

	// Publish force-opens the stats screen on every client; hide closes it. Local instance only.

	protected bool m_bStatsEventsSubscribed;

	protected void SubscribeStatsEvents()
	{
		if (m_bStatsEventsSubscribed)
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.GetOnStatsPublished().Insert(OnStatsPublishedChanged_Local);
		m_bStatsEventsSubscribed = true;

		// The manager's publish invoke from RplLoad can land before this subscription exists.
		if (mgr.IsStatsPublished())
			OnStatsPublishedChanged_Local(true);
	}

	protected void OnStatsPublishedChanged_Local(bool published)
	{
		if (published)
			LL_StatsScreen.Open();
		else
			LL_StatsScreen.CloseStatic();
	}

	// The panel pulls preview data for the asking admin only; publish and hide broadcast
	// through LL_LobbyManager.

	protected ref ScriptInvoker m_OnStatsPanelData = new ScriptInvoker();	// (string viewJson, string unitsJson, bool published, bool recording, string commanders, string suggestedCommanders)
	protected ref ScriptInvoker m_OnKillfeedData = new ScriptInvoker();		// (string encoded) — see LL_StatsManager.BuildPlayerKillfeedData
	protected string m_sStatsPanelViewIncoming;
	protected string m_sStatsPanelUnitsIncoming;
	protected bool m_bStatsPanelPublished;
	protected bool m_bStatsPanelRecording;
	protected string m_sStatsPanelCommanders;
	protected string m_sStatsPanelSuggested;

	ScriptInvoker GetOnStatsPanelData()
	{
		return m_OnStatsPanelData;
	}

	void AskStatsPanelData()
	{
		Rpc(RpcAsk_StatsPanelData);
	}

	void AskStatsPublish(string winnerFactionKey, string commandersEncoded)
	{
		Rpc(RpcAsk_StatsPublish, winnerFactionKey, commandersEncoded);
	}

	void AskStatsHide()
	{
		Rpc(RpcAsk_StatsHide);
	}

	void AskStatsWebsiteRefresh()
	{
		Rpc(RpcAsk_StatsWebsiteRefresh);
	}

	// Season self-heal for clients that streamed in during the broadcast; no admin gate.
	void AskSeasonResend()
	{
		Rpc(RpcAsk_SeasonResend);
	}

	// Owner-targeted, never a re-broadcast: a looping client can only flood itself.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SeasonResend()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		string url = mgr.GetSiteUrl();
		string json = mgr.GetSeasonJson();
		if (url == "" && json == "")
			return;

		Rpc(RpcDo_OwnerSeasonBegin, url);

		array<string> chunks = {};
		LL_StatsManager.SplitChunks(json, chunks);
		foreach (string chunk : chunks)
			Rpc(RpcDo_OwnerSeasonChunk, chunk);

		Rpc(RpcDo_OwnerSeasonEnd);
	}

	protected string m_sOwnerSeasonIncoming;
	protected string m_sOwnerSeasonSiteUrl;

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerSeasonBegin(string siteUrl)
	{
		m_sOwnerSeasonSiteUrl = siteUrl;
		m_sOwnerSeasonIncoming = "";
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerSeasonChunk(string chunk)
	{
		m_sOwnerSeasonIncoming += chunk;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerSeasonEnd()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.ClientApplySeason(m_sOwnerSeasonSiteUrl, m_sOwnerSeasonIncoming);

		m_sOwnerSeasonIncoming = "";
		m_sOwnerSeasonSiteUrl = "";
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StatsPanelData()
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		SendStatsPanelData_S();
	}

	// Server → this owner. Also re-sent when a requested website refresh lands.
	void SendStatsPanelData_S()
	{
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!stats || !mgr)
		{
			Print("[LL_Lobby] Stats: panel data requested but LL_StatsManager is not on the game-mode entity — add the component next to LL_LobbyManager", LogLevel.WARNING);
			return;
		}

		// Plain Rpc covers both topologies: with no session the engine invokes the method
		// directly on this machine, so a local apply would double every chunk there.
		Rpc(RpcDo_OwnerStatsPanelBegin, mgr.IsStatsPublished(), stats.IsRecording(), stats.GetCommandersEncoded(), stats.BuildSuggestedCommandersEncoded());

		array<string> chunks = {};
		LL_StatsManager.SplitChunks(stats.BuildViewJson(), chunks);
		foreach (string viewChunk : chunks)
			Rpc(RpcDo_OwnerStatsPanelViewChunk, viewChunk);

		chunks.Clear();
		LL_StatsManager.SplitChunks(stats.BuildUnitsOptionsJson(), chunks);
		foreach (string unitsChunk : chunks)
			Rpc(RpcDo_OwnerStatsPanelUnitsChunk, unitsChunk);

		Rpc(RpcDo_OwnerStatsPanelEnd);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerStatsPanelBegin(bool published, bool recording, string commanders, string suggestedCommanders)
	{
		m_sStatsPanelViewIncoming = "";
		m_sStatsPanelUnitsIncoming = "";
		m_bStatsPanelPublished = published;
		m_bStatsPanelRecording = recording;
		// Admin picker state rides the meta RPC, not the broadcast view.
		m_sStatsPanelCommanders = commanders;
		m_sStatsPanelSuggested = suggestedCommanders;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerStatsPanelViewChunk(string chunk)
	{
		m_sStatsPanelViewIncoming += chunk;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerStatsPanelUnitsChunk(string chunk)
	{
		m_sStatsPanelUnitsIncoming += chunk;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerStatsPanelEnd()
	{
		m_OnStatsPanelData.Invoke(m_sStatsPanelViewIncoming, m_sStatsPanelUnitsIncoming, m_bStatsPanelPublished, m_bStatsPanelRecording, m_sStatsPanelCommanders, m_sStatsPanelSuggested);
		m_sStatsPanelViewIncoming = "";
		m_sStatsPanelUnitsIncoming = "";
		m_sStatsPanelCommanders = "";
		m_sStatsPanelSuggested = "";
	}

	// Pulled, never pushed: nothing crosses the wire while the panel is closed.

	void RequestKillfeed()
	{
		Rpc(RpcAsk_Killfeed);
	}

	ScriptInvoker GetOnKillfeedData()
	{
		return m_OnKillfeedData;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Killfeed()
	{
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (!stats)
		{
			Print("[LL_Killfeed] !! no LL_StatsManager on the game-mode entity — add the component next to LL_LobbyManager", LogLevel.ERROR);
			return;
		}

		// Own record only: the requester's id comes from the controller, never the message.
		Rpc(RpcDo_OwnerKillfeed, stats.BuildPlayerKillfeedData(GetPlayerId()));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerKillfeed(string encoded)
	{
		m_OnKillfeedData.Invoke(encoded);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StatsPublish(string winnerFactionKey, string commandersEncoded)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			stats.PublishFromPanel_S(winnerFactionKey, commandersEncoded);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StatsHide()
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.HideStatsView_S();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StatsWebsiteRefresh()
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (!stats)
			return;

		stats.StartSeasonFetch();
		stats.StartUnitsFetch(GetPlayerId());
	}

	// Vanilla drops other-faction markers at receipt and never re-sends them; the server
	// re-sends the new faction's markers to a switching player.

	// The caller decides entitlement (LL_M_SCR_MapMarkerManagerComponent.OnAskAddStaticMarker).
	void SendStaticMarker_S(SCR_MapMarkerBase marker)
	{
		if (!Replication.IsServer() || !marker)
			return;

		Rpc(RpcDo_OwnerAddMarker, marker);
	}

	void SyncFactionMarkers_S(notnull Faction faction)
	{
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!markerMgr || !factionManager)
			return;

		array<SCR_MapMarkerBase> markers = {};
		markerMgr.LL_GetStaticMarkersOfFaction(factionManager.GetFactionIndex(faction), markers);

		// Whole-object RPC, as vanilla ships SCR_MapMarkerBase itself.
		foreach (SCR_MapMarkerBase marker : markers)
		{
			Rpc(RpcDo_OwnerAddMarker, marker);
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerAddMarker(SCR_MapMarkerBase markerData)
	{
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
			return;

		// Same-faction markers received before slotting are still cached.
		int markerID = markerData.GetMarkerID();
		if (markerMgr.GetStaticMarkerByID(markerID) || markerMgr.GetDisabledMarkerByID(markerID))
			return;

		// Trusted add: the server already decided entitlement, and the client's own
		// affiliation may not have replicated yet.
		markerMgr.LL_AddTrustedMarker(markerData);
	}

	// Ready requires a slot; un-ready is always allowed.

	void AskSetReady(bool ready)
	{
		Rpc(RpcAsk_SetReady, ready);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetReady(bool ready)
	{
		int playerId = GetPlayerId();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		if (ready && !mgr.FindSlotByPlayerId(playerId))
			return;

		mgr.SetPlayerReady_S(playerId, ready);
	}

	void AskAdvanceState()
	{
		int playerId = GetPlayerId();
		Rpc(RpcAsk_AdvanceState, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AdvanceState(int playerId)
	{
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (!gameMode)
			return;

		gameMode.AdvanceState_S(GetPlayerId());
	}

	void AskSetSlotLocked(int slotRplId, bool locked)
	{
		Rpc(RpcAsk_SetSlotLocked, slotRplId, locked);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetSlotLocked(int slotRplId, bool locked)
	{
		// The sender's real role is only known on the server.
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.SetSlotLocked_S(slotRplId, locked);
	}

	void AskSetGroupLocked(int groupId, bool locked)
	{
		Rpc(RpcAsk_SetGroupLocked, groupId, locked);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetGroupLocked(int groupId, bool locked)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.SetGroupLocked_S(groupId, locked);
	}

	// Frees the target's slot; they stay on the server.

	void AskKickPlayer(int targetPlayerId)
	{
		Rpc(RpcAsk_KickPlayer, targetPlayerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_KickPlayer(int targetPlayerId)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.LeaveSlot_S(targetPlayerId);
	}

	// The held slot is freed by the normal disconnect path.

	void AskKickFromServer(int targetPlayerId)
	{
		Rpc(RpcAsk_KickFromServer, targetPlayerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_KickFromServer(int targetPlayerId)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		if (targetPlayerId == GetPlayerId())
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
			pm.KickPlayer(targetPlayerId, PlayerManagerKickReason.KICK);
	}

	// Reuses TakeSlot_S: it validates availability, auto-leaves the old slot and
	// repossesses in GAME. No pin.

	void AskAssignPlayerToSlot(int targetPlayerId, int slotRplId)
	{
		Rpc(RpcAsk_AssignPlayerToSlot, targetPlayerId, slotRplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AssignPlayerToSlot(int targetPlayerId, int slotRplId)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.TakeSlot_S(targetPlayerId, slotRplId);
	}

	// Server-initiated: spectator triggers are detected on the authority; camera and
	// menu are client-local, so the server only notifies the owning client.

	void NotifySpectatorEnter_S()
	{
		// Rpc never executes on the sending machine; the listen host calls directly.
		if (GetGame().GetPlayerController() == GetOwner())
			ScheduleEnterSpectator();
		else
			Rpc(RpcDo_OwnerEnterSpectator);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerEnterSpectator()
	{
		ScheduleEnterSpectator();
	}

	// One second gives a death-cam beat on the corpse and lets streaming settle.
	protected void ScheduleEnterSpectator()
	{
		GetGame().GetCallqueue().CallLater(EnterSpectatorNow, 1000, false);
	}

	protected void EnterSpectatorNow()
	{
		LL_SpectatorManager spectatorMgr = LL_SpectatorManager.GetInstance();
		if (spectatorMgr)
			spectatorMgr.EnterSpectator();

		// Death keeps the corpse as the controlled entity: no entity-changed event fires.
		LL_MenuVoN.Refresh();
	}

	// A transceiver's frequency is owner-authoritative; a server-side SetFrequency on a
	// client-owned body never reaches the client, so the owner tunes its own radio.

	void TuneSquadRadio_S(int frequency)
	{
		if (frequency <= 0)
			return;

		// Rpc never runs on the sender.
		if (GetGame().GetPlayerController() == GetOwner())
			ApplySquadRadioFrequency(frequency, 0);
		else
			Rpc(RpcDo_OwnerTuneSquadRadio, frequency);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_OwnerTuneSquadRadio(int frequency)
	{
		ApplySquadRadioFrequency(frequency, 0);
	}

	// Owner-side. Retries locally: the body and its radio gadget stream in a few seconds
	// after possession on a dedicated server.
	protected void ApplySquadRadioFrequency(int frequency, int attempt)
	{
		if (frequency <= 0)
			return;

		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity controlled = pc.GetControlledEntity();
		if (controlled)
		{
			SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(controlled);
			if (gadgetManager)
			{
				IEntity radioEntity = gadgetManager.GetGadgetByType(EGadgetType.RADIO);
				if (radioEntity)
				{
					BaseRadioComponent radio = BaseRadioComponent.Cast(radioEntity.FindComponent(BaseRadioComponent));
					if (radio && radio.TransceiversCount() >= 1)
					{
						BaseTransceiver transceiver = radio.GetTransceiver(0);
						if (transceiver)
						{
							if (transceiver.GetFrequency() != frequency)
								transceiver.SetFrequency(frequency);
							return;
						}
					}
				}
			}
		}

		if (attempt < 20)
			GetGame().GetCallqueue().CallLater(ApplySquadRadioFrequency, 250, false, frequency, attempt + 1);
	}

	// Faction-scoped channels are sealed to that faction; see CanPlayerJoinChannel.

	void AskJoinVoNChannel(string channelKey)
	{
		Rpc(RpcAsk_JoinVoNChannel, channelKey);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_JoinVoNChannel(string channelKey)
	{
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
			vonMgr.JoinChannel_S(GetPlayerId(), channelKey);
	}

	// A restriction zone decides locally; only the authority may apply damage, and it
	// only ever kills the caller's own controlled entity.

	void RequestKillOutsideZone()
	{
		// Must run on the authority; Rpc never loops back to the sender.
		if (Replication.IsServer())
			ExecKillOutsideZone();
		else
			Rpc(RpcAsk_KillOutsideZone);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_KillOutsideZone()
	{
		ExecKillOutsideZone();
	}

	protected void ExecKillOutsideZone()
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity controlled = pc.GetControlledEntity();
		if (!controlled)
			return;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(
			controlled.FindComponent(SCR_DamageManagerComponent));
		if (!damage || damage.GetState() == EDamageState.DESTROYED)
			return;

		Print(string.Format("[LL_Lobby] Player %1 (%2) killed by zone-restriction timeout at %3",
			pc.GetPlayerId(),
			GetGame().GetPlayerManager().GetPlayerName(pc.GetPlayerId()),
			controlled.GetOrigin().ToString()), LogLevel.NORMAL);

		damage.Kill(Instigator.CreateInstigator(controlled));
	}

	// Admin chat commands. The client-side admin check is fast feedback only; every Ask
	// is re-validated on the server.

	void AskBroadcastAdminMessage(string text)
	{
		Rpc(RpcAsk_BroadcastAdminMessage, text);
	}

	protected static const int MAX_ADMIN_MESSAGE_CHARS = 200;

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BroadcastAdminMessage(string text)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		if (text.Length() > MAX_ADMIN_MESSAGE_CHARS)
			text = text.Substring(0, MAX_ADMIN_MESSAGE_CHARS);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.BroadcastAdminMessage_S(text, "#LL-AdminMessage_AdminAnnouncement");
	}

	// /hardfreeze <seconds>: in GAME engages or re-times the hold, 0 releases; outside
	// GAME only sets what GAME start uses.
	void AskAdjustHardFreeze(int seconds)
	{
		Rpc(RpcAsk_AdjustHardFreeze, seconds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AdjustHardFreeze(int seconds)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode)
			gameMode.AdjustHardFreeze_S(seconds);
	}

	void AskAdjustFreezeTime(int seconds)
	{
		Rpc(RpcAsk_AdjustFreezeTime, seconds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AdjustFreezeTime(int seconds)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode)
			gameMode.AdjustFreezeTime_S(seconds);
	}

	void AskEndFreezeTime()
	{
		Rpc(RpcAsk_EndFreezeTime);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EndFreezeTime()
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode)
			gameMode.EndFreezeTime_S();
	}

	// The reply comes from the server so the admin sees the authoritative outcome.
	void AskSetVerification(bool enable)
	{
		Rpc(RpcAsk_SetVerification, enable);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetVerification(bool enable)
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return;

		int result = LL_PlayerVerificationComponent.CMD_RESULT_NOT_ACTIVE;
		LL_PlayerVerificationComponent verification = LL_PlayerVerificationComponent.GetInstance();
		if (verification)
			result = verification.SetVerificationSuspended_S(!enable);

		Rpc(RpcDo_VerifyCommandResult, result);
	}

	// Only compact slotRplId → label pairs travel, chunked. Reliable RPCs from one sender
	// stay ordered; two concurrent imports last-write-win as a whole.
	void AskApplyWebsiteSlotting(notnull array<string> chunks)
	{
		Rpc(RpcAsk_BeginWebsiteSlotting);

		foreach (string chunk : chunks)
			Rpc(RpcAsk_WebsiteSlottingChunk, chunk);

		Rpc(RpcAsk_EndWebsiteSlotting);
	}

	protected bool CanApplyWebsiteSlotting_S()
	{
		if (!SCR_Global.IsAdmin(GetPlayerId()))
			return false;

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		return gameMode && gameMode.GetLobbyState() == SCR_EGameModeState.SLOTSELECTION;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BeginWebsiteSlotting()
	{
		if (!CanApplyWebsiteSlotting_S())
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.BeginWebsiteOccupants_S();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_WebsiteSlottingChunk(string chunk)
	{
		if (!CanApplyWebsiteSlotting_S())
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.AddWebsiteOccupantsChunk_S(chunk);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EndWebsiteSlotting()
	{
		if (!CanApplyWebsiteSlotting_S())
		{
			Rpc(RpcDo_SlotImportResult, -1);
			return;
		}

		int count = 0;
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			count = mgr.EndWebsiteOccupants_S();

		Rpc(RpcDo_SlotImportResult, count);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SlotImportResult(int count)
	{
		if (count < 0)
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-Command_SlottingOnly"));
			return;
		}

		if (count == 0)
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-Command_SlotImportCleared"));
			return;
		}

		ShowCommandHelp(WidgetManager.Translate("#LL-Command_SlotImportApplied", count.ToString()));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_VerifyCommandResult(int result)
	{
		string key;
		switch (result)
		{
			case LL_PlayerVerificationComponent.CMD_RESULT_SUSPENDED:
			{
				key = "#LL-Command_VerifySuspended";
				break;
			}
			case LL_PlayerVerificationComponent.CMD_RESULT_RESUMED:
			{
				key = "#LL-Command_VerifyResumed";
				break;
			}
			default:
			{
				key = "#LL-Command_VerifyNotActive";
				break;
			}
		}

		ShowCommandHelp(WidgetManager.Translate(key));
	}

	// GetCommandInvoker is the engine's hook for custom chat commands: the panel strips
	// "/<name>" and fires the invoker with the trailing text.

	protected void RegisterChatCommands()
	{
		if (m_bChatCommandsRegistered)
			return;

		SCR_ChatPanelManager chatMgr = SCR_ChatPanelManager.GetInstance();
		if (!chatMgr)
			return;

		chatMgr.GetCommandInvoker("help").Insert(OnChatCmd_Help);
		chatMgr.GetCommandInvoker("message").Insert(OnChatCmd_Message);
		chatMgr.GetCommandInvoker("freeze").Insert(OnChatCmd_Freeze);
		chatMgr.GetCommandInvoker("endfreeze").Insert(OnChatCmd_EndFreeze);
		chatMgr.GetCommandInvoker("hardfreeze").Insert(OnChatCmd_HardFreeze);
		chatMgr.GetCommandInvoker("verify").Insert(OnChatCmd_Verify);
		chatMgr.GetCommandInvoker("slotexport").Insert(OnChatCmd_SlotExport);
		chatMgr.GetCommandInvoker("slotimport").Insert(OnChatCmd_SlotImport);
		chatMgr.GetCommandInvoker("stats").Insert(OnChatCmd_Stats);

		m_bChatCommandsRegistered = true;
	}

	protected void UnregisterChatCommands()
	{
		if (!m_bChatCommandsRegistered)
			return;

		SCR_ChatPanelManager chatMgr = SCR_ChatPanelManager.GetInstance();
		if (chatMgr)
		{
			chatMgr.GetCommandInvoker("help").Remove(OnChatCmd_Help);
			chatMgr.GetCommandInvoker("message").Remove(OnChatCmd_Message);
			chatMgr.GetCommandInvoker("freeze").Remove(OnChatCmd_Freeze);
			chatMgr.GetCommandInvoker("endfreeze").Remove(OnChatCmd_EndFreeze);
			chatMgr.GetCommandInvoker("hardfreeze").Remove(OnChatCmd_HardFreeze);
			chatMgr.GetCommandInvoker("verify").Remove(OnChatCmd_Verify);
			chatMgr.GetCommandInvoker("slotexport").Remove(OnChatCmd_SlotExport);
			chatMgr.GetCommandInvoker("slotimport").Remove(OnChatCmd_SlotImport);
			chatMgr.GetCommandInvoker("stats").Remove(OnChatCmd_Stats);
		}

		m_bChatCommandsRegistered = false;
	}

	// /help. Lines are translated up front (chat shows plain text) and accumulated one by
	// one: a single chained expression trips the compiler's formula-complexity limit.
	// Two messages because the chat truncates a long system line.
	protected void OnChatCmd_Help(SCR_ChatPanel panel, string data)
	{
		string help = WidgetManager.Translate("#LL-Command_HelpHeader");
		help += "\n" + WidgetManager.Translate("#LL-Command_HelpMessage");
		help += "\n" + WidgetManager.Translate("#LL-Command_HelpFreeze");
		help += "\n" + WidgetManager.Translate("#LL-Command_HelpEndFreeze");
		help += "\n" + WidgetManager.Translate("#LL-Command_HelpHardFreeze");
		ShowCommandHelp(help);

		string help2 = WidgetManager.Translate("#LL-Command_HelpHeader2");
		help2 += "\n" + WidgetManager.Translate("#LL-Command_HelpVerify");
		help2 += "\n" + WidgetManager.Translate("#LL-Command_HelpSlotExport");
		help2 += "\n" + WidgetManager.Translate("#LL-Command_HelpSlotImport");
		help2 += "\n" + WidgetManager.Translate("#LL-Command_HelpStats");
		help2 += "\n" + WidgetManager.Translate("#LL-Command_HelpHelp");
		ShowCommandHelp(help2);
	}

	// /stats: the only entry to the admin control panel. The header's Statistics button
	// is the viewer for everyone.
	protected void OnChatCmd_Stats(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin())
			return;

		LL_StatsAdminPanel.Open();
	}

	// /message <text> — flash an admin notice on everyone's HUD for ~10s.
	protected void OnChatCmd_Message(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin())
			return;

		if (data == "")
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-Command_UsageMessage"));
			return;
		}

		AskBroadcastAdminMessage(data);
	}

	// /freeze <seconds> — adjust the freeze window. Positive extends, negative
	// subtracts; a negative value larger than what's left ends the freeze now.
	protected void OnChatCmd_Freeze(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin())
			return;

		// ToInt on an empty string raises a VM exception.
		int seconds = 0;
		if (data != "")
			seconds = data.ToInt();

		if (seconds == 0)
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-Command_UsageFreeze"));
			return;
		}

		AskAdjustFreezeTime(seconds);
	}

	// /hardfreeze <seconds> — engage the hold now (in GAME), 0 releases it.
	protected void OnChatCmd_HardFreeze(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin())
			return;

		// ToInt on an empty string raises a VM exception.
		string arg = data.Trim();
		if (arg == "")
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-Command_UsageHardFreeze"));
			return;
		}

		// ToInt turns any text into 0, which would release the hold on a typo; accept 0 only
		// when typed. Negatives are a mistake: this is an absolute duration.
		int seconds = arg.ToInt();
		if (seconds < 0 || (seconds == 0 && arg != "0"))
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-Command_UsageHardFreeze"));
			return;
		}

		AskAdjustHardFreeze(seconds);
	}

	// /endfreeze — end the freeze window immediately.
	protected void OnChatCmd_EndFreeze(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin())
			return;

		AskEndFreezeTime();
	}

	// /verify on|off: suspend or resume the website registration gate during an outage.
	protected void OnChatCmd_Verify(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin())
			return;

		string arg = data.Trim();
		arg.ToLower();

		if (arg == "off")
		{
			AskSetVerification(false);
			return;
		}
		if (arg == "on")
		{
			AskSetVerification(true);
			return;
		}

		ShowCommandHelp(WidgetManager.Translate("#LL-Command_UsageVerify"));
	}

	// /slotexport: local only, slots and vehicles are already replicated.
	protected void OnChatCmd_SlotExport(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin() || !RequireSlotSelection())
			return;

		string json = LL_WebsiteSlotting.BuildExportJson();
		if (json == "")
		{
			ShowCommandHelp(WidgetManager.Translate("#LL-SlottingExport_Empty"));
			return;
		}

		LL_SlottingExportDialog.Open(json);
	}

	// /slotimport — paste the website's filled slotting, validate, apply.
	protected void OnChatCmd_SlotImport(SCR_ChatPanel panel, string data)
	{
		if (!RequireAdmin() || !RequireSlotSelection())
			return;

		LL_SlottingImportDialog.Open(this);
	}

	protected bool RequireSlotSelection()
	{
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && gameMode.GetLobbyState() == SCR_EGameModeState.SLOTSELECTION)
			return true;

		ShowCommandHelp(WidgetManager.Translate("#LL-Command_SlottingOnly"));
		return false;
	}

	protected bool RequireAdmin()
	{
		if (SCR_Global.IsAdmin())
			return true;

		ShowCommandHelp(WidgetManager.Translate("#LL-Command_AdminOnly"));
		return false;
	}

	protected void ShowCommandHelp(string msg)
	{
		SCR_ChatPanelManager chatMgr = SCR_ChatPanelManager.GetInstance();
		if (chatMgr)
			chatMgr.ShowHelpMessage(msg);
	}
}