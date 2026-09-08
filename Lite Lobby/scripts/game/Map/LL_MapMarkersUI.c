// The lobby's marker UI (LL_MapBriefing.conf). Rendering and sync are inherited; the
// placement flow changes: double left-click on empty map opens LL_InsertMarkerDialog at
// the cursor, a click on an own marker edits it, the stock radial category is gone, and
// holding the info key shows every marker's age. Placement rides the stock static-marker
// pipeline, so faction gates, JIP and the marker limit apply with no new replication.

//! Not a subclass of SCR_MarkerIconEntry: that would drag its unused Workbench fields
//! into every mission maker's attribute list.
[BaseContainerProps(), LL_MarkerIconEntryTitle()]
class LL_MarkerIconEntry
{
	[Attribute("{CCED36C0D903191D}UI/Textures/GroupManagement/FlagIcons/LiteIcons.imageset", UIWidgets.ResourcePickerThumbnail, desc: "Imageset holding the marker glyph.", params: "imageset")]
	protected ResourceName m_sIconImageset;

	[Attribute("", desc: "Quad (sprite) name inside the imageset.")]
	protected string m_sIconImagesetQuad;

	[Attribute("", desc: "Human-readable name shown in the insert-marker dialog's Type dropdown (e.g. \"Fire team\"). If empty, the raw imageset quad name is shown instead.")]
	protected string m_sDisplayName;

	[Attribute("50", desc: "Size of this icon on the map, in pixels (constant screen size, does not zoom). 0 = the stock layout's default size.")]
	protected float m_fIconSize;

	[Attribute("1", UIWidgets.Slider, desc: "DISTANCE FROM ICON TO ITS TEXT LABEL, as the fraction of this icon's square that the glyph actually fills (0.1 - 1). LOWER = label sits CLOSER to the icon. Leave at 1 for a shape that fills its whole square, such as a diamond. Lower it for a small or narrow glyph drawn inside a large transparent square, or its label sits stranded far to the right: a plain dot is around 0.25, an exclamation mark around 0.22, a flag around 0.5. Purely visual, and independent of the icon's Size.", params: "0.1 1 0.01")]
	protected float m_fLabelInk;

	void GetIconResource(out ResourceName imageset, out string imageQuad)
	{
		imageset = m_sIconImageset;
		imageQuad = m_sIconImagesetQuad;
	}

	float GetIconSize()
	{
		return m_fIconSize;
	}

	float GetLabelInk()
	{
		return m_fLabelInk;
	}

	string GetDisplayName()
	{
		if (m_sDisplayName != string.Empty)
			return m_sDisplayName;

		return m_sIconImagesetQuad;
	}
}

class LL_MarkerIconEntryTitle : BaseContainerCustomTitle
{
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		string name, quad;
		source.Get("m_sDisplayName", name);
		source.Get("m_sIconImagesetQuad", quad);

		if (name != string.Empty)
			title = name;
		else
			title = quad;

		return true;
	}
}

//! Resolves a faction's live colour at runtime so markers match the map's faction symbols.
[BaseContainerProps()]
class LL_MarkerColorEntry : SCR_MarkerColorEntry
{
	[Attribute("", desc: "OPTIONAL - purely visual. Enter a faction key (e.g. US, USSR, FIA) to make this palette entry use that faction's color, so markers match the faction symbols on the map. The Color above is then only a fallback for missions without that faction. Leave EMPTY to always use the fixed Color. This does NOT restrict who can pick this color or who sees the marker.")]
	protected string m_sTakeColorFromFaction;

	protected ref Color m_ResolvedColor;

	override Color GetColor()
	{
		if (!m_ResolvedColor && m_sTakeColorFromFaction != string.Empty)
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				Faction faction = factionManager.GetFactionByKey(m_sTakeColorFromFaction);
				if (faction)
					m_ResolvedColor = faction.GetFactionColor();
			}
		}

		if (m_ResolvedColor)
			return Color.FromInt(m_ResolvedColor.PackToInt());

		return super.GetColor();
	}
}

class LL_MapMarkersUI : SCR_MapMarkersUI
{
	[Attribute("{1D21A7B8C9D0E1F2}UI/Map/InsertMarkerDialog.layout", UIWidgets.ResourceNamePicker, desc: "Insert-marker dialog layout", params: "layout")]
	protected ResourceName m_sInsertDialogLayout;

	protected const float DOUBLE_CLICK_MS = 400;
	protected const int DOUBLE_CLICK_MAX_DIST_PX = 12;

	protected Widget m_wLLDialogRoot;
	protected LL_InsertMarkerDialog m_LLDialog;
	protected bool m_bLLDialogDelayed;

	protected bool m_bLLAgesShown;

	protected float m_fLastClickTime = -float.MAX;
	protected int m_iLastClickX;
	protected int m_iLastClickY;

	// Last confirmed type/colour preselect the next place dialog; statics survive map
	// close/reopen. Text is never remembered.
	protected static int s_iLastIconEntry;
	protected static int s_iLastColorEntry;

	// Captured when the dialog opens, so the marker lands where the player double-clicked.
	protected float m_fTargetWorldX;
	protected float m_fTargetWorldY;

	//! No radial "Markers" category; the hover-delete entry is added at the radial root.
	override protected void OnRadialMenuInit()
	{
	}

	//! The quick-marker key performs the category this class no longer creates.
	override protected void OnInputQuickMarkerMenu(float value, EActionTrigger reason)
	{
	}

	//! Client-side mirror of the server gate: a faction and a living slot character.
	//! The client must refuse too because the stock drag writes the new position locally
	//! before asking the server; with the RPC dropped a spectator could redraw the plan
	//! for themselves.
	protected bool LL_CanEditMarkers()
	{
		// The spectator map is read-only outright; the death → spectator handover does
		// not land in one frame.
		if (LL_SpectatorMenu.IsMapOpen())
			return false;

		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return false;

		int playerId = playerController.GetPlayerId();

		if (!SCR_FactionManager.SGetPlayerFaction(playerId))
			return false;

		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		if (lobbyManager)
		{
			LL_SlotData slot = lobbyManager.FindSlotByPlayerId(playerId);
			if (slot && slot.IsDestroyed())
				return false;
		}

		return true;
	}

	//! Never setting the dragged flag is what pins the marker: SCR_MapMarkerBase.Update
	//! re-asserts its own screen position every frame otherwise.
	override protected void OnDragWidget(Widget widget)
	{
		if (!LL_CanEditMarkers())
			return;

		super.OnDragWidget(widget);
	}

	//! The dragged flag is cleared regardless: a player who dies mid-drag already had it
	//! set, and a marker left flagged never updates its widget again.
	override protected void OnDragEnd(Widget widget, bool wasDragged)
	{
		if (!LL_CanEditMarkers())
		{
			SCR_MapMarkerBase marker;
			if (m_MarkerMgr)
				marker = m_MarkerMgr.GetMarkerByWidget(widget);

			if (marker)
				marker.SetDragged(false);

			return;
		}

		super.OnDragEnd(widget, wasDragged);
	}

	//! Removal is intel too; the server refuses it from a spectator.
	override protected void OnInputMarkerDelete(float value, EActionTrigger reason)
	{
		if (!LL_CanEditMarkers())
			return;

		super.OnInputMarkerDelete(value, reason);
	}

	//! Own placed marker → edit dialog; empty map → double-click → place dialog.
	override protected void OnInputMapSelect(float value, EActionTrigger reason)
	{
		if (!LL_CanEditMarkers())
			return;

		if ((m_CursorModule.GetCursorState() & SCR_MapCursorModule.STATE_POPUP_RESTRICTED) != 0)
			return;

		// While the dialog is up its MapMarkerEditContext suppresses the menu's map
		// context, so MapSelect never fires; outside clicks use the raw MouseLeft listener.
		if (m_wLLDialogRoot)
			return;

		array<Widget> widgets = SCR_MapCursorModule.GetMapWidgetsUnderCursor();

		SCR_MapMarkerWidgetComponent markerComp;
		foreach (Widget widget : widgets)
		{
			markerComp = SCR_MapMarkerWidgetComponent.Cast(widget.FindHandler(SCR_MapMarkerWidgetComponent));
			if (!markerComp)
				continue;

			SCR_MapMarkerBase marker = m_MarkerMgr.GetMarkerByWidget(widget);
			if (!marker)
				continue;

			if (IsOwnedMarker(marker) && marker.GetType() == SCR_EMapMarkerType.PLACED_CUSTOM)
			{
				m_fLastClickTime = -float.MAX;
				OpenEditDialog(marker);
				return;
			}
		}

		if (RegisterClickAndCheckDouble())
			OpenPlaceDialog();
	}

	//! True when this click completed a double click, which is then consumed.
	protected bool RegisterClickAndCheckDouble()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);

		bool isDoubleClick = (now - m_fLastClickTime) <= DOUBLE_CLICK_MS
			&& Math.AbsInt(mouseX - m_iLastClickX) <= DOUBLE_CLICK_MAX_DIST_PX
			&& Math.AbsInt(mouseY - m_iLastClickY) <= DOUBLE_CLICK_MAX_DIST_PX;

		m_fLastClickTime = now;
		m_iLastClickX = mouseX;
		m_iLastClickY = mouseY;

		if (!isDoubleClick)
			return false;

		m_fLastClickTime = -float.MAX;
		return true;
	}

	protected void OpenPlaceDialog()
	{
		m_MapEntity.GetMapCursorWorldPosition(m_fTargetWorldX, m_fTargetWorldY);
		CreateLLDialog(s_iLastIconEntry, s_iLastColorEntry, string.Empty);
	}

	protected void OpenEditDialog(notnull SCR_MapMarkerBase marker)
	{
		int wPos[2];
		marker.GetWorldPos(wPos);
		m_fTargetWorldX = wPos[0];
		m_fTargetWorldY = wPos[1];

		CreateLLDialog(marker.GetIconEntry(), marker.GetColorEntry(), marker.GetCustomText());
		if (!m_wLLDialogRoot)
			return;

		// The original is hidden while edited; confirm replaces it, cancel restores it.
		// SetVisible alone does not hold: the manager's per-frame pass re-shows any hidden
		// marker inside the visible frame unless it is blocked.
		m_EditedMarker = marker;
		marker.SetBlocked(true);
		marker.SetVisible(false);
	}

	//! Per frame, not once: the manager rewrites the blocked flag on every marker each
	//! time any player adds one, and clearing it re-shows the widget.
	protected void LL_EnforceEditedMarkerHidden()
	{
		if (!m_EditedMarker || m_EditedMarker.GetBlocked())
			return;

		m_EditedMarker.SetBlocked(true);
		m_EditedMarker.SetVisible(false);
	}

	//! Blocking moved the marker into the manager's disabled list, which is only re-tested
	//! on pan, so put it back explicitly.
	protected void LL_RestoreEditedMarker()
	{
		if (!m_EditedMarker)
			return;

		m_EditedMarker.SetBlocked(false);

		if (m_MarkerMgr)
			m_MarkerMgr.SetStaticMarkerDisabled(m_EditedMarker, false);

		m_EditedMarker.SetVisible(true);
	}

	protected void CreateLLDialog(int iconEntry, int colorEntry, string text)
	{
		CloseLLDialog();

		if (!m_PlacedMarkerConfig)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wLLDialogRoot = workspace.CreateWidgets(m_sInsertDialogLayout, m_RootWidget);
		if (!m_wLLDialogRoot)
			return;

		m_LLDialog = LL_InsertMarkerDialog.Cast(m_wLLDialogRoot.FindHandler(LL_InsertMarkerDialog));
		if (!m_LLDialog)
		{
			m_wLLDialogRoot.RemoveFromHierarchy();
			m_wLLDialogRoot = null;
			return;
		}

		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);
		FrameSlot.SetPos(m_wLLDialogRoot, workspace.DPIUnscale(mouseX), workspace.DPIUnscale(mouseY));

		m_LLDialog.Init(m_PlacedMarkerConfig, iconEntry, colorEntry, text);
		m_LLDialog.GetOnConfirm().Insert(OnLLDialogConfirm);
		m_LLDialog.GetOnCancel().Insert(OnLLDialogCancel);

		// Console users without the UGC privilege may not share content.
		SocialComponent sc = GetSocialComponent();
		if (sc && !sc.IsPrivilegedTo(EUserInteraction.UserGeneratedContent))
			m_LLDialog.SetConfirmEnabled(false);

		GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_SELECT, EActionTrigger.DOWN, OnLLInputConfirm);
		GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnLLInputBack);

		// Raw mouse, not MapSelect: map actions are suppressed while the dialog's input
		// context is active.
		GetGame().GetInputManager().AddActionListener("MouseLeft", EActionTrigger.DOWN, OnLLDialogMouseDown);

		m_CursorModule.HandleDialog(true);
	}

	//! Does not touch m_EditedMarker; confirm/cancel decide its fate first.
	protected void CloseLLDialog()
	{
		if (!m_wLLDialogRoot)
			return;

		GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_SELECT, EActionTrigger.DOWN, OnLLInputConfirm);
		GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnLLInputBack);
		GetGame().GetInputManager().RemoveActionListener("MouseLeft", EActionTrigger.DOWN, OnLLDialogMouseDown);

		m_wLLDialogRoot.RemoveFromHierarchy();
		m_wLLDialogRoot = null;
		m_LLDialog = null;
		m_bLLDialogDelayed = false;

		m_CursorModule.HandleDialog(false);
	}

	protected void OnLLDialogConfirm(int iconEntry, int colorEntry, string text)
	{
		s_iLastIconEntry = iconEntry;
		s_iLastColorEntry = colorEntry;

		// Editing = replace; a marker's fields are immutable once synced.
		float rotation = 0;
		if (m_EditedMarker)
		{
			rotation = m_EditedMarker.GetRotation();

			// Un-hide before the removal request: if the server drops it, the original must
			// stay visible here too.
			LL_RestoreEditedMarker();
			RemoveOwnedMarker(m_EditedMarker);
			m_EditedMarker = null;
		}

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		marker.SetIconEntry(iconEntry);
		marker.SetColorEntry(colorEntry);
		marker.SetRotation(rotation);
		marker.SetCustomText(text);
		marker.SetWorldPos(m_fTargetWorldX, m_fTargetWorldY);

		ChimeraWorld world = GetGame().GetWorld();
		if (world)
			marker.SetTimestamp(world.GetServerTimestamp());

		// Not a display flag here: the manager's RplSave writes the timestamp only for
		// markers that have it set, so without it late joiners see ageless markers.
		marker.SetTimestampVisibility(true);

		// Faction-flagged like every stock public marker; the visibility model keys off it.
		FactionManager factionManager = GetGame().GetFactionManager();
		if (factionManager)
		{
			Faction ownerFaction = SCR_FactionManager.SGetPlayerFaction(GetGame().GetPlayerController().GetPlayerId());
			if (ownerFaction)
				marker.AddMarkerFactionFlags(factionManager.GetFactionIndex(ownerFaction));
		}

		m_MarkerMgr.InsertStaticMarker(marker, false);
		GetOnCustomMarkerPlaced().Invoke(m_fTargetWorldX, m_fTargetWorldY, false);

		CloseLLDialog();
	}

	protected void OnLLDialogCancel()
	{
		if (m_EditedMarker)
		{
			LL_RestoreEditedMarker();
			m_EditedMarker = null;
		}

		CloseLLDialog();
	}

	protected void OnLLInputConfirm(float value, EActionTrigger reason)
	{
		if (!m_LLDialog || !m_bLLDialogDelayed)
			return;

		if (m_LLDialog.IsComboOpened() || m_LLDialog.IsComboFocused() || m_LLDialog.IsEditingText())
			return;

		m_LLDialog.Confirm();
	}

	//! Double-click outside the dialog cancels it. While a dropdown is expanded the first
	//! outside click belongs to closing the list.
	protected void OnLLDialogMouseDown(float value, EActionTrigger reason)
	{
		if (!m_LLDialog)
			return;

		if (m_LLDialog.IsComboOpened() || m_LLDialog.IsCursorInside())
			return;

		if (RegisterClickAndCheckDouble())
			OnLLDialogCancel();
	}

	protected void OnLLInputBack(float value, EActionTrigger reason)
	{
		if (!m_LLDialog)
			return;

		// Escape closes the dropdown or exits typing first; only a bare Escape cancels.
		if (m_LLDialog.IsComboOpened() || m_LLDialog.IsEditingText() || m_LLDialog.WasEscapeJustConsumed())
			return;

		m_LLDialog.Cancel();
	}

	//! Hold the info key to see every marker's age (last change), computed once per press
	//! from the replicated timestamps. Polled rather than DOWN/UP driven: a lost release
	//! (the insert dialog takes the map context mid-hold) would strand the ages.
	protected void LL_UpdateMarkerAges()
	{
		if (!m_MarkerMgr)
			return;

		bool wanted = GetGame().GetInputManager().GetActionValue("LL_MapMarkerInfo") > 0;
		if (wanted == m_bLLAgesShown)
			return;

		m_bLLAgesShown = wanted;

		// Markers outside the visible frame sit in a separate list; a marker panned off
		// screen while the key is held would otherwise keep a frozen age.
		LL_ApplyMarkerAges(m_MarkerMgr.GetStaticMarkers(), wanted);
		LL_ApplyMarkerAges(m_MarkerMgr.GetDisabledMarkers(), wanted);
	}

	protected void LL_ApplyMarkerAges(array<SCR_MapMarkerBase> markers, bool state)
	{
		if (!markers)
			return;

		foreach (SCR_MapMarkerBase marker : markers)
		{
			if (!marker)
				continue;

			SCR_MapMarkerWidgetComponent widgetComp = marker.GetMarkerComponent();
			if (!widgetComp)
				continue;

			if (state)
				widgetComp.LL_SetTimeSuffix(LL_FormatMarkerAge(marker.GetTimestamp()));
			else
				widgetComp.LL_SetTimeSuffix(string.Empty);
		}
	}

	//! "12 minutes ago", translated. Under a minute gets its own wording; the stock helper
	//! says "0 minutes ago".
	protected string LL_FormatMarkerAge(WorldTimestamp timestamp)
	{
		if (!timestamp)
			return string.Empty;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return string.Empty;

		float seconds = world.GetServerTimestamp().DiffSeconds(timestamp);
		if (seconds < 0)
			return string.Empty;

		if (seconds < 60)
			return WidgetManager.Translate("#LL-Markers_TimeJustNow");

		return SCR_FormatHelper.GetTimeSinceEventImprecise(seconds);
	}

	override void OnMapClose(MapConfiguration config)
	{
		// The next open builds new widgets with no age on them.
		m_bLLAgesShown = false;

		if (m_wLLDialogRoot)
			OnLLDialogCancel();

		LL_DropMarkerWidgets();

		super.OnMapClose(config);
	}

	//! SCR_MapMarkerBase.OnCreateMarker creates the widget on every open and OnMapClosed
	//! never removes it; vanilla survives because closing a map screen destroys the whole
	//! menu tree. The spectator map is a layer of a menu that stays alive, so every open
	//! stacked another set of markers. Not fixed in a modded SCR_MapMarkerBase: a modded
	//! class on it loses the static codec methods and all three marker RPCs stop
	//! replicating. m_wRoot stays set, as after vanilla's own OnDelete.
	protected void LL_DropMarkerWidgets()
	{
		if (!m_MarkerMgr)
			return;

		// Disabled markers hold widgets too.
		LL_DropWidgetsOf(m_MarkerMgr.GetStaticMarkers());
		LL_DropWidgetsOf(m_MarkerMgr.GetDisabledMarkers());
	}

	protected void LL_DropWidgetsOf(array<SCR_MapMarkerBase> markers)
	{
		if (!markers)
			return;

		foreach (SCR_MapMarkerBase marker : markers)
		{
			if (!marker)
				continue;

			Widget root = marker.GetRootWidget();
			if (root)
				root.RemoveFromHierarchy();
		}
	}

	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		LL_UpdateMarkerAges();
		LL_EnforceEditedMarkerHidden();

		if (m_wLLDialogRoot)
		{
			// One-frame gap so the opening double-click cannot land in the dialog context.
			if (!m_bLLDialogDelayed)
			{
				m_bLLDialogDelayed = true;
				return;
			}

			GetGame().GetInputManager().ActivateContext("MapMarkerEditContext");
		}
	}
}