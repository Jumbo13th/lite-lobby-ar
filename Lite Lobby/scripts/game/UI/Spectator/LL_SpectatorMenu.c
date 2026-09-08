// GAME-stage screen for dead players: sliding players and voice panels, alive-ratio bar,
// mission timer, character labels, a fullscreen map on M and a footer. The free-fly
// camera keeps flying under this menu (see LL_SpectatorCamera).

modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	SpectatorMenu
}

class LL_SpectatorMenu : MenuBase
{
	// Panel slide clamps, coupled to the 430 px panel width and a 16 px margin; they
	// override the layout's X offsets, so a width change must update them too.
	protected static const float SLIDE_SPEED = 1200.0;
	protected static const float ALIVE_HIDDEN_X = -425.0;
	protected static const float ALIVE_VISIBLE_X = 16.0;
	protected static const float VOICE_HIDDEN_X = -5.0;
	protected static const float VOICE_VISIBLE_X = -446.0;

	protected static const ResourceName LABEL_LAYOUT = "{7D84B5C90A2C6397}UI/Spectator/SpectatorLabel.layout";

	protected LL_GameModeCoop m_GameModeCoop;
	protected LL_LobbyManager m_LobbyManager;
	protected LL_SpectatorManager m_SpectatorManager;
	protected int m_iPlayerId;

	protected SCR_ChatPanel m_ChatPanel;
	protected Widget m_wAlivePlayersList;
	protected Widget m_wVoiceChatFrame;
	protected Widget m_wOverlayFooter;
	protected Widget m_wSidesRatioFrame;
	protected Widget m_wLabelsFrame;
	protected TextWidget m_wGameTimerText;

	// Personal killfeed panel: shown on request, hidden again on a timer.
	protected Widget m_wKillfeedFrame;
	protected RichTextWidget m_wKillfeedText;
	protected SCR_InputButtonComponent m_NavigationKillfeedComp;
	protected bool m_bKillfeedOpen;
	protected bool m_bKillfeedToggleQueued;

	protected static const int KILLFEED_SHOW_MS = 30000;
	protected SCR_ButtonBaseComponent m_hAlivePinButton;
	protected SCR_ButtonBaseComponent m_hVoicePinButton;
	protected SCR_InputButtonComponent m_NavigationChatComp;
	protected SCR_InputButtonComponent m_NavigationSwitchUIComp;
	protected SCR_InputButtonComponent m_NavigationResetComp;
	protected SCR_InputButtonComponent m_NavigationMapComp;
	protected LL_AlivePlayerList m_AlivePlayerList;

	// Keyed by slot RplId; the same widgets are reused as map icons while the map is open.
	protected ref map<int, LL_SpectatorLabel> m_mLabels = new map<int, LL_SpectatorLabel>();
	protected ref array<int> m_aLabelSweep = {};
	protected ref LL_SpectatorMap m_Map = new LL_SpectatorMap();
	protected bool m_bSwitchUIQueued;
	protected bool m_bMapToggleQueued;
	protected bool m_bLabelsDiagnosed;
	protected int m_iLastTimerSeconds = -1;

	// Static because the check lives in the modded SCR_ManualCamera.
	protected static bool s_bMapOpen;

	static bool IsMapOpen()
	{
		return s_bMapOpen;
	}

	override void OnMenuOpen()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Close();
			return;
		}

		m_GameModeCoop = LL_GameModeCoop.GetInstance();
		m_LobbyManager = LL_LobbyManager.GetInstance();
		m_SpectatorManager = LL_SpectatorManager.GetInstance();

		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
			m_iPlayerId = pc.GetPlayerId();

		Widget root = GetRootWidget();

		Widget wChatPanel = root.FindAnyWidget("ChatPanel");
		if (wChatPanel)
			m_ChatPanel = SCR_ChatPanel.Cast(wChatPanel.FindHandler(SCR_ChatPanel));

		LL_MenuChat.ShowOwnPanel(m_ChatPanel);

		m_wAlivePlayersList = root.FindAnyWidget("AlivePlayersList");
		m_wVoiceChatFrame = root.FindAnyWidget("VoiceChatFrame");
		m_wOverlayFooter = root.FindAnyWidget("OverlayFooter");
		m_wSidesRatioFrame = root.FindAnyWidget("SidesRatioFrame");
		m_wLabelsFrame = root.FindAnyWidget("LabelsFrame");
		m_wGameTimerText = TextWidget.Cast(root.FindAnyWidget("GameTimerText"));

		// Both panels name their pin "PinButton"; search each panel's subtree.
		if (m_wAlivePlayersList)
		{
			Widget pin = m_wAlivePlayersList.FindAnyWidget("PinButton");
			if (pin)
				m_hAlivePinButton = SCR_ButtonBaseComponent.Cast(pin.FindHandler(SCR_ButtonBaseComponent));

			m_AlivePlayerList = LL_AlivePlayerList.Cast(m_wAlivePlayersList.FindHandler(LL_AlivePlayerList));
			if (m_AlivePlayerList)
				m_AlivePlayerList.Init(this);
		}

		// False when the screen or world cannot host a map; the footer button then stays hidden.
		bool mapReady = m_Map.Init(root);
		s_bMapOpen = false;

		if (m_wVoiceChatFrame)
		{
			Widget pin = m_wVoiceChatFrame.FindAnyWidget("PinButton");
			if (pin)
				m_hVoicePinButton = SCR_ButtonBaseComponent.Cast(pin.FindHandler(SCR_ButtonBaseComponent));
		}

		// NavigationFactionVoice has no handler: VONDirect is consumed by the engine.
		Widget wNavChat = root.FindAnyWidget("NavigationChat");
		if (wNavChat)
			m_NavigationChatComp = SCR_InputButtonComponent.Cast(wNavChat.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationChatComp)
			m_NavigationChatComp.m_OnActivated.Insert(Action_ChatOpen);

		// Authored visible so it matches the other panels; hidden here before the first frame.
		EnsureKillfeedWidgets();
		HideKillfeed();

		Widget wNavKillfeed = root.FindAnyWidget("NavigationKillfeed");
		if (wNavKillfeed)
			m_NavigationKillfeedComp = SCR_InputButtonComponent.Cast(wNavKillfeed.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationKillfeedComp)
			m_NavigationKillfeedComp.m_OnActivated.Insert(Action_ToggleKillfeed);

		LL_LobbyPlayerComponent playerComp = LL_LobbyPlayerComponent.GetLocalInstance();
		if (playerComp)
			playerComp.GetOnKillfeedData().Insert(OnKillfeedData);

		Widget wNavSwitchUI = root.FindAnyWidget("NavigationSwitchSpectatorUI");
		if (wNavSwitchUI)
			m_NavigationSwitchUIComp = SCR_InputButtonComponent.Cast(wNavSwitchUI.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationSwitchUIComp)
			m_NavigationSwitchUIComp.m_OnActivated.Insert(Action_SwitchSpectatorUI);

		Widget wNavReset = root.FindAnyWidget("NavigationResetCamera");
		if (wNavReset)
			m_NavigationResetComp = SCR_InputButtonComponent.Cast(wNavReset.FindHandler(SCR_InputButtonComponent));
		if (m_NavigationResetComp)
			m_NavigationResetComp.m_OnActivated.Insert(Action_ResetCamera);

		Widget wNavMap = root.FindAnyWidget("NavigationMap");
		if (wNavMap)
		{
			wNavMap.SetVisible(mapReady);
			m_NavigationMapComp = SCR_InputButtonComponent.Cast(wNavMap.FindHandler(SCR_InputButtonComponent));
		}
		if (m_NavigationMapComp)
			m_NavigationMapComp.m_OnActivated.Insert(Action_ToggleMap);

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.AddActionListener("ChatToggle", EActionTrigger.DOWN, Action_ChatOpen);
		// With the UI hidden the footer button stops firing; M must still work. The double
		// fire is debounced in Action_ToggleMap.
		inputManager.AddActionListener("GadgetMap", EActionTrigger.DOWN, Action_ToggleMap);
		// Bound on UP like the stock radial it replaces.
		inputManager.AddActionListener("MapContextualMenu", EActionTrigger.UP, Action_MapJump);
		inputManager.AddActionListener("LL_SwitchSpectatorUI", EActionTrigger.DOWN, Action_SwitchSpectatorUI);
		// Same reason as the map key; reset is idempotent.
		inputManager.AddActionListener("LL_SpectatorReset", EActionTrigger.DOWN, Action_ResetCamera);
		inputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, Action_Exit);

		// Gameplay contexts are inactive while a menu is open; dead players reach the lobby here.
		inputManager.AddActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_LobbyKeyToggle);
		inputManager.AddActionListener("LL_SpectatorKillfeed", EActionTrigger.DOWN, Action_ToggleKillfeed);
	}

	override void OnMenuClose()
	{
		LL_MenuChat.NotifyMenuClosed();

		// The map entity outlives this screen.
		m_Map.Shutdown();
		s_bMapOpen = false;

		m_mLabels.Clear();

		if (m_NavigationChatComp)
			m_NavigationChatComp.m_OnActivated.Remove(Action_ChatOpen);
		if (m_NavigationSwitchUIComp)
			m_NavigationSwitchUIComp.m_OnActivated.Remove(Action_SwitchSpectatorUI);
		if (m_NavigationResetComp)
			m_NavigationResetComp.m_OnActivated.Remove(Action_ResetCamera);
		if (m_NavigationMapComp)
			m_NavigationMapComp.m_OnActivated.Remove(Action_ToggleMap);
		if (m_NavigationKillfeedComp)
			m_NavigationKillfeedComp.m_OnActivated.Remove(Action_ToggleKillfeed);

		GetGame().GetCallqueue().Remove(HideKillfeed);

		LL_LobbyPlayerComponent playerComp = LL_LobbyPlayerComponent.GetLocalInstance();
		if (playerComp)
			playerComp.GetOnKillfeedData().Remove(OnKillfeedData);

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.RemoveActionListener("ChatToggle", EActionTrigger.DOWN, Action_ChatOpen);
		inputManager.RemoveActionListener("LL_SpectatorKillfeed", EActionTrigger.DOWN, Action_ToggleKillfeed);
		inputManager.RemoveActionListener("GadgetMap", EActionTrigger.DOWN, Action_ToggleMap);
		inputManager.RemoveActionListener("MapContextualMenu", EActionTrigger.UP, Action_MapJump);
		inputManager.RemoveActionListener("LL_SwitchSpectatorUI", EActionTrigger.DOWN, Action_SwitchSpectatorUI);
		inputManager.RemoveActionListener("LL_SpectatorReset", EActionTrigger.DOWN, Action_ResetCamera);
		inputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, Action_Exit);
		inputManager.RemoveActionListener("LL_OpenLobby", EActionTrigger.DOWN, Action_LobbyKeyToggle);
	}

	override void OnMenuUpdate(float tDelta)
	{
		// Input comes from the preset's ActionContext; while the map is open the shared
		// map context is layered on top for pan/zoom/marker actions and the closing GadgetMap.
		if (m_Map.IsOpen())
			GetGame().GetInputManager().ActivateContext("LL_BriefingMapContext");

		// Unconditional: the stray tool frame is visible precisely when the map is closed.
		m_Map.EnforceToolsHidden();

		if (m_ChatPanel)
			m_ChatPanel.OnUpdateChat(tDelta);

		// A lobby or briefing view opened above hid this chat panel.
		if (m_ChatPanel && !m_ChatPanel.GetWidget().IsVisible()
			&& GetGame().GetMenuManager().GetTopMenu() == this)
			LL_MenuChat.ShowOwnPanel(m_ChatPanel);

		UpdatePanelSliding(tDelta);
		UpdateLabels();
		UpdateGameTimer();
	}

	// Vanilla m_fTimeElapsed only advances in PREGAME and GAME and is engine-synced, so
	// GetElapsedTime is the mission clock.
	protected void UpdateGameTimer()
	{
		if (!m_wGameTimerText || !m_GameModeCoop)
			return;

		int seconds = m_GameModeCoop.GetElapsedTime();
		if (seconds == m_iLastTimerSeconds)
			return;

		m_iLastTimerSeconds = seconds;
		m_wGameTimerText.SetText(SCR_FormatHelper.FormatTime(seconds));
	}

	// One label per slot, projected through the camera or through the map. The map costs
	// no extra widgets: the two views are never on screen together.

	protected void UpdateLabels()
	{
		if (!m_wLabelsFrame)
		{
			DiagnoseLabelsOnce("LabelsFrame widget not found — reimport UI/Spectator/SpectatorMenu.layout");
			return;
		}

		if (!m_LobbyManager)
			return;

		if (!m_wLabelsFrame.IsVisible())
			return;

		bool mapMode = m_Map.IsOpen();
		SCR_MapEntity mapEntity = m_Map.GetMapEntity();

		// The map entity attaches two frames after the key press.
		if (mapMode && (!mapEntity || !mapEntity.IsOpen()))
		{
			HideAllLabels();
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		int unresolvedSlots = 0;

		foreach (int labelSlotRplId, LL_SpectatorLabel existing : m_mLabels)
		{
			if (existing)
				existing.SetTouched(false);
		}

		foreach (LL_SlotData slot : m_LobbyManager.GetSlots())
		{
			LL_SpectatorLabel label;
			bool hasLabel = m_mLabels.Find(slot.m_iRplId, label);
			if (hasLabel && label)
				label.SetTouched(true);

			// On the map the players-panel filters apply so both views agree; the 3D view
			// shows what the camera could physically see.
			if (mapMode && !IsShownOnMap(slot))
			{
				if (hasLabel && label && label.GetRootWidget())
					label.GetRootWidget().SetVisible(false);

				continue;
			}

			if (!hasLabel)
			{
				// Resolves on the authority (offline fallback ids included) and on clients.
				IEntity entity = m_LobbyManager.ResolveSlotEntity(slot.m_iRplId);
				if (!entity)
				{
					unresolvedSlots++;
					continue;
				}

				Widget labelRoot = workspace.CreateWidgets(LABEL_LAYOUT, m_wLabelsFrame);
				if (!labelRoot)
				{
					DiagnoseLabelsOnce("SpectatorLabel.layout failed to create — is UI/Spectator/SpectatorLabel.layout imported and its GUID intact?");
					return;
				}

				label = LL_SpectatorLabel.Cast(labelRoot.FindHandler(LL_SpectatorLabel));
				if (!label)
				{
					labelRoot.RemoveFromHierarchy();
					DiagnoseLabelsOnce("SpectatorLabel.layout has no LL_SpectatorLabel handler — check the layout's components block");
					return;
				}

				label.SetTarget(entity, slot);
				label.SetTouched(true);
				m_mLabels.Insert(slot.m_iRplId, label);
			}

			// Entity deleted or streamed out: drop the widget, recreate when it returns.
			bool alive;
			if (mapMode)
				alive = label.UpdateOnMap(slot, mapEntity);
			else
				alive = label.UpdateLabel(slot);

			if (!alive)
				RemoveLabel(slot.m_iRplId);
		}

		SweepUnusedLabels();

		// Slot ids that match no replication item (Workbench offline fallback ids).
		if (m_mLabels.IsEmpty() && unresolvedSlots > 0)
			DiagnoseLabelsOnce(string.Format("no slot character resolved from %1 slot ids (offline fallback ids?)", unresolvedSlots));
	}

	// The dead are always drawn on the map, unlike the panel: where they fell is the
	// information, and they already read as dead.
	protected bool IsShownOnMap(notnull LL_SlotData slot)
	{
		if (!m_AlivePlayerList)
			return true;

		return m_AlivePlayerList.IsFactionSelected(slot.m_sFactionKey);
	}

	// Labels are keyed by the character RplId, which changes on respawn or revive; the
	// loop then never visits the old key and its widget would hang where last drawn.
	protected void SweepUnusedLabels()
	{
		m_aLabelSweep.Clear();

		foreach (int slotRplId, LL_SpectatorLabel label : m_mLabels)
		{
			if (!label || !label.IsTouched())
				m_aLabelSweep.Insert(slotRplId);
		}

		foreach (int slotRplId : m_aLabelSweep)
			RemoveLabel(slotRplId);
	}

	// The widgets must not keep drawing 3D positions over a map that is coming up.
	protected void HideAllLabels()
	{
		foreach (int slotRplId, LL_SpectatorLabel label : m_mLabels)
		{
			if (label && label.GetRootWidget())
				label.GetRootWidget().SetVisible(false);
		}
	}

	protected void DiagnoseLabelsOnce(string reason)
	{
		if (m_bLabelsDiagnosed)
			return;

		m_bLabelsDiagnosed = true;
		Print("[LL_Spectator] Labels disabled: " + reason, LogLevel.WARNING);
	}

	protected void RemoveLabel(int slotRplId)
	{
		LL_SpectatorLabel label;
		if (!m_mLabels.Find(slotRplId, label))
			return;

		if (label && label.GetRootWidget())
			label.GetRootWidget().RemoveFromHierarchy();

		m_mLabels.Remove(slotRplId);
	}

	// Lazy: on dedicated-server clients a menu can open before the player id is assigned.
	protected int GetLocalPlayerId()
	{
		if (m_iPlayerId <= 0)
		{
			PlayerController pc = GetGame().GetPlayerController();
			if (pc)
				m_iPlayerId = pc.GetPlayerId();
		}

		return m_iPlayerId;
	}

	protected void UpdatePanelSliding(float tDelta)
	{
		if (!m_wAlivePlayersList || !m_wVoiceChatFrame)
			return;

		Widget cursorWidget = WidgetManager.GetWidgetUnderCursor();
		while (cursorWidget)
		{
			if (cursorWidget == m_wAlivePlayersList)
				break;
			if (cursorWidget == m_wVoiceChatFrame)
				break;
			cursorWidget = cursorWidget.GetParent();
		}

		float aliveX = FrameSlot.GetPosX(m_wAlivePlayersList);
		if (cursorWidget == m_wAlivePlayersList || (m_hAlivePinButton && m_hAlivePinButton.IsToggled()))
		{
			aliveX += tDelta * SLIDE_SPEED;
			if (aliveX > ALIVE_VISIBLE_X)
				aliveX = ALIVE_VISIBLE_X;
		}
		else
		{
			aliveX -= tDelta * SLIDE_SPEED;
			if (aliveX < ALIVE_HIDDEN_X)
				aliveX = ALIVE_HIDDEN_X;
		}
		FrameSlot.SetPosX(m_wAlivePlayersList, aliveX);

		float voiceX = FrameSlot.GetPosX(m_wVoiceChatFrame);
		if (cursorWidget == m_wVoiceChatFrame || (m_hVoicePinButton && m_hVoicePinButton.IsToggled()))
		{
			voiceX -= tDelta * SLIDE_SPEED;
			if (voiceX < VOICE_VISIBLE_X)
				voiceX = VOICE_VISIBLE_X;
		}
		else
		{
			voiceX += tDelta * SLIDE_SPEED;
			if (voiceX > VOICE_HIDDEN_X)
				voiceX = VOICE_HIDDEN_X;
		}
		FrameSlot.SetPosX(m_wVoiceChatFrame, voiceX);
	}

	// Clicking the followed row releases back to free flight.
	void OnRowClicked(int slotRplId)
	{
		if (!m_SpectatorManager)
			return;

		if (m_SpectatorManager.GetSpectatedSlotRplId() == slotRplId)
			m_SpectatorManager.FreeCamera();
		else
			m_SpectatorManager.SpectateSlot(slotRplId);
	}

	protected void Action_Exit()
	{
		// The map is a layer of this screen, so Escape peels it off first.
		if (m_Map.IsOpen())
		{
			Action_ToggleMap();
			return;
		}

		// "Back" brings the pause menu up on top instead of closing.
		GetGame().GetCallqueue().CallLater(OpenPauseMenuWrap, 0);
	}

	protected void OpenPauseMenuWrap()
	{
		ArmaReforgerScripted.OpenPauseMenu();
	}

	protected void Action_ChatOpen()
	{
		// Opening chat inline from an input event fights the menu's own input handling.
		GetGame().GetCallqueue().CallLater(ChatOpenWrap, 0);
	}

	protected void ChatOpenWrap()
	{
		if (m_ChatPanel)
			SCR_ChatPanelManager.GetInstance().OpenChatPanel(m_ChatPanel);
	}

	// Debounced: the key reaches this handler twice in one frame (direct listener and
	// footer button), and the direct listener must stay for the hidden-footer case.
	protected void Action_SwitchSpectatorUI()
	{
		if (m_bSwitchUIQueued)
			return;

		m_bSwitchUIQueued = true;
		GetGame().GetCallqueue().CallLater(SwitchSpectatorUIImpl, 0, false);
	}

	protected void SwitchSpectatorUIImpl()
	{
		m_bSwitchUIQueued = false;

		bool visible = true;
		if (m_wAlivePlayersList)
			visible = m_wAlivePlayersList.IsVisible();

		if (m_wAlivePlayersList)
			m_wAlivePlayersList.SetVisible(!visible);
		if (m_wVoiceChatFrame)
			m_wVoiceChatFrame.SetVisible(!visible);
		if (m_wOverlayFooter)
			m_wOverlayFooter.SetVisible(!visible);
		if (m_wSidesRatioFrame)
			m_wSidesRatioFrame.SetVisible(!visible);
		if (m_wLabelsFrame)
			m_wLabelsFrame.SetVisible(!visible);
	}

	// Re-requested on every open so a revived player sees the current record. Debounced
	// like the UI toggle.
	protected void Action_ToggleKillfeed()
	{
		if (m_bKillfeedToggleQueued)
			return;

		m_bKillfeedToggleQueued = true;
		GetGame().GetCallqueue().CallLater(ToggleKillfeedImpl, 0, false);
	}

	protected void ToggleKillfeedImpl()
	{
		m_bKillfeedToggleQueued = false;

		if (m_bKillfeedOpen)
		{
			HideKillfeed();
			return;
		}

		if (!EnsureKillfeedWidgets())
			return;

		LL_LobbyPlayerComponent playerComp = LL_LobbyPlayerComponent.GetLocalInstance();
		if (!playerComp)
		{
			Print("[LL_Killfeed] no local LL_LobbyPlayerComponent — cannot ask the server", LogLevel.ERROR);
			return;
		}

		playerComp.RequestKillfeed();
	}

	// Resolved on use: a layout hot-reload rebuilds the widget tree without re-running
	// OnMenuOpen.
	protected bool EnsureKillfeedWidgets()
	{
		if (m_wKillfeedFrame && m_wKillfeedText)
			return true;

		Widget root = GetRootWidget();
		if (root)
		{
			m_wKillfeedFrame = root.FindAnyWidget("KillfeedFrame");
			m_wKillfeedText = RichTextWidget.Cast(root.FindAnyWidget("KillfeedText"));
			if (m_wKillfeedFrame && m_wKillfeedText)
				return true;
		}

		Print("[LL_Killfeed] KillfeedFrame / KillfeedText missing from SpectatorMenu.layout", LogLevel.ERROR);
		return false;
	}

	// Names only; every heading is translated here in the reader's language.
	protected void OnKillfeedData(string encoded)
	{
		// The reply lands a round trip later; the widget tree can be rebuilt in between.
		if (!EnsureKillfeedWidgets())
			return;

		m_wKillfeedText.SetText(FormatKillfeed(encoded));

		m_bKillfeedOpen = true;
		m_wKillfeedFrame.SetVisible(true);

		GetGame().GetCallqueue().Remove(HideKillfeed);
		GetGame().GetCallqueue().CallLater(HideKillfeed, KILLFEED_SHOW_MS, false);
	}

	protected void HideKillfeed()
	{
		m_bKillfeedOpen = false;
		GetGame().GetCallqueue().Remove(HideKillfeed);

		if (m_wKillfeedFrame)
			m_wKillfeedFrame.SetVisible(false);
	}

	// Rich text, so lines are joined with <br/>.
	protected string FormatKillfeed(string encoded)
	{
		if (encoded == "")
			return WidgetManager.Translate("#LL-Killfeed_Nothing");

		bool died = false;
		string killedBy = "";
		array<string> kills = {};
		array<string> teamkills = {};

		array<string> lines = {};
		encoded.Split("\n", lines, true);

		foreach (string line : lines)
		{
			if (line.Length() < 2)
				continue;

			string tag = line.Substring(0, 1);
			string value = line.Substring(2, line.Length() - 2);

			if (tag == "D")
				died = true;
			else if (tag == "B")
				killedBy = value;
			else if (tag == "K")
				value.Split("\t", kills, true);
			else if (tag == "T")
				value.Split("\t", teamkills, true);
		}

		string text = WidgetManager.Translate("#LL-Killfeed_TotalKills", kills.Count().ToString());

		if (!kills.IsEmpty())
			text += "<br/><br/>" + KillfeedSection("#LL-Killfeed_KilledEnemies", kills);

		if (!teamkills.IsEmpty())
			text += "<br/><br/>" + KillfeedSection("#LL-Killfeed_Teamkills", teamkills);

		if (died)
		{
			text += "<br/><br/>" + WidgetManager.Translate("#LL-Killfeed_DiedBy") + "<br/>  ";
			if (killedBy != "")
				text += killedBy;
			else
				text += WidgetManager.Translate("#LL-Killfeed_NoKiller");
		}

		return text;
	}

	// Not truncated: the panel scrolls.
	protected string KillfeedSection(string headerKey, notnull array<string> names)
	{
		string text = WidgetManager.Translate(headerKey, names.Count().ToString());

		foreach (string name : names)
			text += "<br/>  - " + name;

		return text;
	}

	// Debounced like the UI toggle.
	protected void Action_ToggleMap()
	{
		if (m_bMapToggleQueued || !m_Map.IsAvailable())
			return;

		m_bMapToggleQueued = true;
		GetGame().GetCallqueue().CallLater(ToggleMapImpl, 0, false);
	}

	protected void ToggleMapImpl()
	{
		m_bMapToggleQueued = false;

		m_Map.Toggle();
		s_bMapOpen = m_Map.IsOpen();

		// The labels were parked at map coordinates; hide them for the one frame before
		// the 3D pass repositions them.
		if (!s_bMapOpen)
			HideAllLabels();
	}

	// Closing the map is the feedback: it has no camera indicator.
	protected void Action_MapJump()
	{
		if (!m_Map.IsOpen() || !m_SpectatorManager)
			return;

		SCR_MapEntity mapEntity = m_Map.GetMapEntity();
		if (!mapEntity || !mapEntity.IsOpen())
			return;

		float worldX, worldZ;
		mapEntity.GetMapCursorWorldPosition(worldX, worldZ);

		m_SpectatorManager.MoveCameraTo(worldX, worldZ);
		Action_ToggleMap();
	}

	// Detached, levelled, default FOV, 1× speed.
	protected void Action_ResetCamera()
	{
		if (m_SpectatorManager)
			m_SpectatorManager.ResetCamera();
	}

	// A dead player may always look at the lobby; taking a slot is validated server-side.
	protected void Action_LobbyKeyToggle()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.CoopLobby))
			return;

		menuManager.OpenMenu(ChimeraMenuPreset.CoopLobby);
	}
}