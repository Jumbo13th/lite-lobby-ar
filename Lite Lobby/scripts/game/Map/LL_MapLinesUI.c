// Map lines on the briefing, game and spectator maps (LL_MapBriefing.conf): drawing a line,
// recolouring it in the insert-marker dialog on a double click, rendering the local player's faction
// lines, deleting with the marker delete key and author labels on the marker-info key.
// Requests go through LL_LobbyPlayerComponent; the server decides and delivers everything,
// and nothing is sent while a line is being drawn.

class LL_MapLinesUI : SCR_MapUIBaseComponent
{
	protected const ResourceName CANVAS_LAYOUT = "{1D10A1B2C3D4E5F6}UI/Map/ZoneCanvas.layout";
	protected const ResourceName DIALOG_LAYOUT = "{1D21A7B8C9D0E1F2}UI/Map/InsertMarkerDialog.layout";

	// Map.layout keeps its own drawing layers below the marker widgets (default 0): the
	// map's canvas at -500, the pencil's lines at -400.
	protected const int CANVAS_Z_ORDER = -450;
	protected const int LABEL_Z_ORDER = -440;

	protected const float SCREEN_WIDTH_FRACTION = 0.02;
	protected const float LINE_ALPHA = 0.6;
	protected const float DOT_RADIUS_FACTOR = 0.75;
	protected const int DOT_SEGMENTS = 16;
	protected const float MIN_DRAWN_WIDTH = 1;
	protected const float HIT_MIN_PX = 6;
	protected const float LABEL_GAP_PX = 4;

	protected SCR_MapCursorModule m_CursorModule;
	protected SCR_MapMarkerEntryPlaced m_PlacedConfig;
	protected LL_MapLineCache m_Cache;

	protected Widget m_wCanvasRoot;
	protected CanvasWidget m_wCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	protected ref array<ref LineDrawCommand> m_aLinePool = {};
	protected ref array<ref PolygonDrawCommand> m_aDotPool = {};
	protected ref array<int> m_aPackedColors = {};

	protected bool m_bDirty;
	protected float m_fLastZoom;
	protected float m_fLastPanX;
	protected float m_fLastPanY;

	protected bool m_bDrawing;
	protected ref array<int> m_aDraft = {};
	protected float m_fLastClickTime = -float.MAX;
	protected int m_iLastClickX;
	protected int m_iLastClickY;
	protected bool m_bLastClickAddedPoint;
	protected int m_iDraftColor;
	protected bool m_bPointLimitShown;
	protected float m_fLastSentTime = -float.MAX;

	protected Widget m_wDialogRoot;
	protected LL_InsertMarkerDialog m_Dialog;
	protected bool m_bDialogDelayed;

	protected bool m_bLabelsShown;
	protected ref array<TextWidget> m_aLabels = {};
	protected ref array<int> m_aLabelLineIds = {};

	//! A line in progress or its colour dialog; the marker UI stays out of the way.
	bool IsBusy()
	{
		return m_bDrawing || m_wDialogRoot;
	}

	override void Init()
	{
		m_CursorModule = SCR_MapCursorModule.Cast(m_MapEntity.GetMapModule(SCR_MapCursorModule));

		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerManager && markerManager.GetMarkerConfig())
			m_PlacedConfig = SCR_MapMarkerEntryPlaced.Cast(markerManager.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
	}

	override protected void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			return;

		Widget layoutRoot = GetGame().GetWorkspace().CreateWidgets(CANVAS_LAYOUT, mapFrame);
		if (!layoutRoot)
			return;

		m_wCanvas = CanvasWidget.Cast(layoutRoot);
		if (!m_wCanvas)
			m_wCanvas = CanvasWidget.Cast(layoutRoot.FindAnyWidget("Canvas"));
		if (!m_wCanvas)
		{
			layoutRoot.RemoveFromHierarchy();
			return;
		}

		m_wCanvasRoot = layoutRoot;
		m_wCanvasRoot.SetZOrder(CANVAS_Z_ORDER);

		// Faction colours resolve at runtime, so the palette is read once the map opens.
		m_aPackedColors.Clear();
		if (m_PlacedConfig)
		{
			array<ref SCR_MarkerColorEntry> colors = m_PlacedConfig.GetColorEntries();
			for (int i = 0; i < colors.Count(); i++)
			{
				// A copy: the entry may hand out its own instance, which markers share.
				Color entryColor = m_PlacedConfig.GetColorEntry(i);
				Color color = Color.FromInt(Color.WHITE);
				if (entryColor)
					color = Color.FromInt(entryColor.PackToInt());

				color.SetA(LINE_ALPHA);
				m_aPackedColors.Insert(color.PackToInt());
			}
		}

		m_Cache = LL_MapLineCache.GetLocal();
		if (m_Cache)
			m_Cache.GetOnChanged().Insert(OnCacheChanged);

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.AddActionListener("MapSelect", EActionTrigger.DOWN, OnInputMapSelect);
		inputManager.AddActionListener("MapMarkerDelete", EActionTrigger.DOWN, OnInputDelete);
		inputManager.AddActionListener("LL_MapLineFinish", EActionTrigger.DOWN, OnInputFinish);
		inputManager.AddActionListener("LL_MapLineCancel", EActionTrigger.DOWN, OnInputCancel);

		m_bDirty = true;
	}

	override protected void OnMapClose(MapConfiguration config)
	{
		InputManager inputManager = GetGame().GetInputManager();
		inputManager.RemoveActionListener("MapSelect", EActionTrigger.DOWN, OnInputMapSelect);
		inputManager.RemoveActionListener("MapMarkerDelete", EActionTrigger.DOWN, OnInputDelete);
		inputManager.RemoveActionListener("LL_MapLineFinish", EActionTrigger.DOWN, OnInputFinish);
		inputManager.RemoveActionListener("LL_MapLineCancel", EActionTrigger.DOWN, OnInputCancel);

		CloseDialog();
		DiscardDraft();
		DropLabels();

		if (m_Cache)
			m_Cache.GetOnChanged().Remove(OnCacheChanged);
		m_Cache = null;

		if (m_wCanvasRoot)
			m_wCanvasRoot.RemoveFromHierarchy();

		m_wCanvasRoot = null;
		m_wCanvas = null;

		super.OnMapClose(config);
	}

	override void Update(float timeSlice)
	{
		if (!m_wCanvas)
			return;

		if (IsBusy() && !LL_MapMarkersUI.LL_CanEditMarkers())
		{
			CloseDialog();
			DiscardDraft();
		}

		if (m_wDialogRoot)
		{
			// One-frame gap, as the marker dialog: the Enter that finished the line must
			// not land in the dialog's context.
			if (m_bDialogDelayed)
				GetGame().GetInputManager().ActivateContext("MapMarkerEditContext");
			else
				m_bDialogDelayed = true;
		}
		else if (m_bDrawing)
		{
			// Suppresses the menu's map context, so Enter cannot open the chat and Escape
			// cannot close the map while a line is in progress.
			GetGame().GetInputManager().ActivateContext("LL_MapLineContext");
		}

		Render();
		UpdateLabels();
	}

	protected void OnCacheChanged()
	{
		m_bDirty = true;

		if (m_bLabelsShown)
			DropLabels();
	}

	protected void OnInputMapSelect(float value, EActionTrigger reason)
	{
		if (!m_wCanvas || m_wDialogRoot)
			return;

		if (!m_bDrawing)
		{
			if (GetGame().GetInputManager().GetActionValue("LL_MapLineStart") <= 0)
				return;

			if (!LL_MapMarkersUI.LL_CanEditMarkers())
				return;

			if (m_CursorModule && (m_CursorModule.GetCursorState() & SCR_MapCursorModule.STATE_POPUP_RESTRICTED) != 0)
				return;

			LL_EMapLineRefusal refusal = CheckLimits();
			if (refusal != LL_EMapLineRefusal.NONE)
			{
				ShowRefusal(refusal);
				return;
			}

			StartLine();
			RegisterClick(false);
			return;
		}

		// The marker dialog's double-click rule. The pair's first click added a point the
		// player did not mean; the start point is never taken back.
		if (IsDoubleClick())
		{
			if (m_bLastClickAddedPoint)
				m_aDraft.Resize(m_aDraft.Count() - 2);

			m_fLastClickTime = -float.MAX;
			m_bDirty = true;
			OpenDialog();
			return;
		}

		RegisterClick(AddCursorPoint());
	}

	protected bool IsDoubleClick()
	{
		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);

		return GetGame().GetWorld().GetWorldTime() - m_fLastClickTime <= LL_MapMarkersUI.DOUBLE_CLICK_MS
			&& Math.AbsInt(mouseX - m_iLastClickX) <= LL_MapMarkersUI.DOUBLE_CLICK_MAX_DIST_PX
			&& Math.AbsInt(mouseY - m_iLastClickY) <= LL_MapMarkersUI.DOUBLE_CLICK_MAX_DIST_PX;
	}

	protected void RegisterClick(bool addedPoint)
	{
		m_fLastClickTime = GetGame().GetWorld().GetWorldTime();
		WidgetManager.GetMousePos(m_iLastClickX, m_iLastClickY);
		m_bLastClickAddedPoint = addedPoint;
	}

	protected LL_EMapLineRefusal CheckLimits()
	{
		if (!m_Cache)
			return LL_EMapLineRefusal.NOT_ALLOWED;

		if (m_Cache.CountOwn() >= LL_MapLine.MAX_LINES_PER_PLAYER)
			return LL_EMapLineRefusal.PLAYER_LIMIT;

		if (m_Cache.CountAll() >= LL_MapLine.MAX_LINES_PER_FACTION)
			return LL_EMapLineRefusal.FACTION_LIMIT;

		return LL_EMapLineRefusal.NONE;
	}

	protected void StartLine()
	{
		m_iDraftColor = LL_MapMarkersUI.GetLastColorEntry();
		m_aDraft.Clear();
		m_bPointLimitShown = false;
		m_bDrawing = true;

		AddCursorPoint();
	}

	//! The same width on the screen at any zoom, within the bounds the server accepts. A line
	//! in progress is drawn with it every frame and sent with the value at Enter.
	protected float GetDraftWidth()
	{
		float zoom = m_MapEntity.GetCurrentZoom();
		if (zoom <= 0)
			return LL_MapLine.MAX_WIDTH_M;

		float screenW, screenH;
		GetGame().GetWorkspace().GetScreenSize(screenW, screenH);

		return Math.Clamp(SCREEN_WIDTH_FRACTION * screenH / zoom, LL_MapLine.MIN_WIDTH_M, LL_MapLine.MAX_WIDTH_M);
	}

	protected bool AddCursorPoint()
	{
		if (m_aDraft.Count() >= LL_MapLine.MAX_POINTS * 2)
		{
			if (!m_bPointLimitShown)
				ShowRefusal(LL_EMapLineRefusal.POINT_LIMIT);

			m_bPointLimitShown = true;
			return false;
		}

		float worldX, worldY;
		m_MapEntity.GetMapCursorWorldPosition(worldX, worldY);
		int x = LL_MapLine.ToDecimetres(worldX);
		int y = LL_MapLine.ToDecimetres(worldY);

		int count = m_aDraft.Count();
		if (count >= 2 && m_aDraft[count - 2] == x && m_aDraft[count - 1] == y)
			return false;

		m_aDraft.Insert(x);
		m_aDraft.Insert(y);
		m_bDirty = true;
		return true;
	}

	protected void OnInputFinish(float value, EActionTrigger reason)
	{
		if (!m_bDrawing || m_wDialogRoot || m_aDraft.Count() < 4)
			return;

		// Refused here rather than by the server, so the drawn line is not lost.
		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fLastSentTime < LL_MapLine.MIN_INTERVAL_S * 1000)
		{
			ShowRefusal(LL_EMapLineRefusal.TOO_SOON);
			return;
		}

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (lobbyPlayer)
		{
			array<int> points = {};
			points.Copy(m_aDraft);
			lobbyPlayer.AskAddMapLine(m_iDraftColor, GetDraftWidth(), points);
		}

		m_fLastSentTime = now;
		DiscardDraft();
	}

	protected void OnInputCancel(float value, EActionTrigger reason)
	{
		if (m_bDrawing && !m_wDialogRoot)
			DiscardDraft();
	}

	protected void DiscardDraft()
	{
		if (!m_bDrawing)
			return;

		m_bDrawing = false;
		m_aDraft.Clear();
		m_bDirty = true;
	}

	protected void ShowRefusal(LL_EMapLineRefusal reason)
	{
		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (lobbyPlayer)
			lobbyPlayer.ShowMapLineRefused(reason);
	}

	protected void OpenDialog()
	{
		if (!m_PlacedConfig)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wDialogRoot = workspace.CreateWidgets(DIALOG_LAYOUT, m_RootWidget);
		if (!m_wDialogRoot)
			return;

		m_Dialog = LL_InsertMarkerDialog.Cast(m_wDialogRoot.FindHandler(LL_InsertMarkerDialog));
		if (!m_Dialog)
		{
			m_wDialogRoot.RemoveFromHierarchy();
			m_wDialogRoot = null;
			return;
		}

		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);
		FrameSlot.SetPos(m_wDialogRoot, workspace.DPIUnscale(mouseX), workspace.DPIUnscale(mouseY));

		m_Dialog.InitLine(m_PlacedConfig, m_iDraftColor);
		m_Dialog.GetOnConfirm().Insert(OnDialogConfirm);
		m_Dialog.GetOnCancel().Insert(OnDialogCancel);

		// Console users without the UGC privilege may not share content.
		PlayerController pc = GetGame().GetPlayerController();
		SocialComponent social;
		if (pc)
			social = SocialComponent.Cast(pc.FindComponent(SocialComponent));
		if (social && !social.IsPrivilegedTo(EUserInteraction.UserGeneratedContent))
			m_Dialog.SetConfirmEnabled(false);

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.AddActionListener(UIConstants.MENU_ACTION_SELECT, EActionTrigger.DOWN, OnDialogInputConfirm);
		inputManager.AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnDialogInputBack);

		m_bDialogDelayed = false;

		if (m_CursorModule)
			m_CursorModule.HandleDialog(true);
	}

	protected void CloseDialog()
	{
		if (!m_wDialogRoot)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.RemoveActionListener(UIConstants.MENU_ACTION_SELECT, EActionTrigger.DOWN, OnDialogInputConfirm);
		inputManager.RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnDialogInputBack);

		m_wDialogRoot.RemoveFromHierarchy();
		m_wDialogRoot = null;
		m_Dialog = null;
		m_bDialogDelayed = false;

		if (m_CursorModule)
			m_CursorModule.HandleDialog(false);
	}

	//! Recolours the line in progress and the remembered colour; drawing continues.
	protected void OnDialogConfirm(int iconEntry, int colorEntry, string text)
	{
		m_iDraftColor = colorEntry;
		LL_MapMarkersUI.SetLastColorEntry(colorEntry);
		m_bDirty = true;

		CloseDialog();
	}

	protected void OnDialogCancel()
	{
		CloseDialog();
	}

	// Enter confirms unless the colour list is open: drawing is a mouse feature, so the
	// focused list button must not swallow it.
	protected void OnDialogInputConfirm(float value, EActionTrigger reason)
	{
		if (!m_Dialog || !m_bDialogDelayed || m_Dialog.IsComboOpened())
			return;

		m_Dialog.Confirm();
	}

	protected void OnDialogInputBack(float value, EActionTrigger reason)
	{
		if (!m_Dialog || m_Dialog.IsComboOpened() || m_Dialog.WasEscapeJustConsumed())
			return;

		m_Dialog.Cancel();
	}

	//! Commands are rebuilt only when the view or the lines changed. Every point is the
	//! projected world origin plus its offset times the zoom, which is what
	//! SCR_MapEntity.WorldToScreen computes, at one engine call per frame.
	protected void Render()
	{
		float zoom = m_MapEntity.GetCurrentZoom();
		vector pan = m_MapEntity.GetCurrentPan();
		if (!m_bDirty && zoom == m_fLastZoom && pan[0] == m_fLastPanX && pan[1] == m_fLastPanY)
			return;

		m_bDirty = false;
		m_fLastZoom = zoom;
		m_fLastPanX = pan[0];
		m_fLastPanY = pan[1];

		int originX, originY;
		m_MapEntity.WorldToScreen(0, 0, originX, originY, true);

		// The screen in world metres, from this frame's projection; the map's own visible
		// frame is refreshed inside its update and can lag one frame behind a pan.
		float screenW, screenH;
		GetGame().GetWorkspace().GetScreenSize(screenW, screenH);
		float viewMinX = -originX / zoom;
		float viewMaxX = (screenW - originX) / zoom;
		float viewMinY = (originY - screenH) / zoom;
		float viewMaxY = originY / zoom;

		m_aCommands.Clear();
		int slot;

		if (m_Cache && zoom > 0)
		{
			foreach (LL_MapLine line : m_Cache.GetLines())
			{
				if (line.m_vMax[0] < viewMinX || line.m_vMin[0] > viewMaxX || line.m_vMax[2] < viewMinY || line.m_vMin[2] > viewMaxY)
					continue;

				AddLineCommands(slot, line.m_aPoints, line.m_fWidth, line.m_iColor, originX, originY, zoom);
				slot++;
			}
		}

		if (m_bDrawing && !m_aDraft.IsEmpty())
			AddLineCommands(slot, m_aDraft, GetDraftWidth(), m_iDraftColor, originX, originY, zoom);

		m_wCanvas.SetDrawCommands(m_aCommands);
	}

	protected void AddLineCommands(int slot, array<int> points, float width, int colorEntry, int originX, int originY, float zoom)
	{
		while (m_aLinePool.Count() <= slot)
		{
			LineDrawCommand newLine = new LineDrawCommand();
			newLine.m_Vertices = new array<float>();
			m_aLinePool.Insert(newLine);

			PolygonDrawCommand newDot = new PolygonDrawCommand();
			newDot.m_Vertices = new array<float>();
			m_aDotPool.Insert(newDot);
		}

		int color = Color.WHITE;
		if (colorEntry >= 0 && colorEntry < m_aPackedColors.Count())
			color = m_aPackedColors[colorEntry];

		float drawnWidth = Math.Max(MIN_DRAWN_WIDTH, width * zoom);

		LineDrawCommand lineCommand = m_aLinePool[slot];
		lineCommand.m_Vertices.Clear();
		for (int i = 0; i < points.Count(); i += 2)
		{
			lineCommand.m_Vertices.Insert(originX + LL_MapLine.ToMetres(points[i]) * zoom);
			lineCommand.m_Vertices.Insert(originY - LL_MapLine.ToMetres(points[i + 1]) * zoom);
		}

		lineCommand.m_fWidth = drawnWidth;
		lineCommand.m_iColor = color;
		if (points.Count() >= 4)
			m_aCommands.Insert(lineCommand);

		float centerX = lineCommand.m_Vertices[0];
		float centerY = lineCommand.m_Vertices[1];
		float radius = drawnWidth * DOT_RADIUS_FACTOR;

		PolygonDrawCommand dotCommand = m_aDotPool[slot];
		dotCommand.m_Vertices.Clear();
		for (int s = 0; s < DOT_SEGMENTS; s++)
		{
			float angle = s * Math.PI2 / DOT_SEGMENTS;
			dotCommand.m_Vertices.Insert(centerX + Math.Cos(angle) * radius);
			dotCommand.m_Vertices.Insert(centerY + Math.Sin(angle) * radius);
		}

		dotCommand.m_iColor = color;
		m_aCommands.Insert(dotCommand);
	}

	protected void OnInputDelete(float value, EActionTrigger reason)
	{
		if (!m_wCanvas || IsBusy() || !m_Cache)
			return;

		if (!LL_MapMarkersUI.LL_CanEditMarkers())
			return;

		// One key, one deletion: the player's own marker under the cursor goes first.
		if (IsOwnMarkerUnderCursor())
			return;

		LL_MapLine line = FindDeletableLineUnderCursor();
		if (!line)
			return;

		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (lobbyPlayer)
			lobbyPlayer.AskRemoveMapLine(line.m_iId);
	}

	protected bool IsOwnMarkerUnderCursor()
	{
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerManager)
			return false;

		array<Widget> widgets = SCR_MapCursorModule.GetMapWidgetsUnderCursor();
		foreach (Widget widget : widgets)
		{
			if (!widget.FindHandler(SCR_MapMarkerWidgetComponent))
				continue;

			SCR_MapMarkerBase marker = markerManager.GetMarkerByWidget(widget);
			if (marker && marker.CanBeRemovedByOwner() && SCR_MapMarkersUI.IsOwnedMarker(marker))
				return true;
		}

		return false;
	}

	//! Topmost first, as drawn; the start dot or any segment counts.
	protected LL_MapLine FindDeletableLineUnderCursor()
	{
		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);

		float zoom = m_MapEntity.GetCurrentZoom();
		int originX, originY;
		m_MapEntity.WorldToScreen(0, 0, originX, originY, true);

		bool isAdmin = SCR_Global.IsAdmin();
		array<ref LL_MapLine> lines = m_Cache.GetLines();
		for (int i = lines.Count() - 1; i >= 0; i--)
		{
			LL_MapLine line = lines[i];
			if (!line.m_bOwn && !isAdmin)
				continue;

			float drawnWidth = Math.Max(MIN_DRAWN_WIDTH, line.m_fWidth * zoom);
			float tolerance = Math.Max(drawnWidth * 0.5, HIT_MIN_PX);

			float prevX, prevY;
			for (int p = 0; p < line.GetPointCount(); p++)
			{
				float worldX, worldY;
				line.GetPoint(p, worldX, worldY);
				float x = originX + worldX * zoom;
				float y = originY - worldY * zoom;

				if (p == 0)
				{
					float dotTolerance = Math.Max(drawnWidth * DOT_RADIUS_FACTOR, HIT_MIN_PX);
					if (Math.Sqrt((x - mouseX) * (x - mouseX) + (y - mouseY) * (y - mouseY)) <= dotTolerance)
						return line;
				}
				else if (DistanceToSegment(mouseX, mouseY, prevX, prevY, x, y) <= tolerance)
				{
					return line;
				}

				prevX = x;
				prevY = y;
			}
		}

		return null;
	}

	protected static float DistanceToSegment(float px, float py, float ax, float ay, float bx, float by)
	{
		float dx = bx - ax;
		float dy = by - ay;
		float lengthSq = dx * dx + dy * dy;

		float t;
		if (lengthSq > 0)
			t = Math.Clamp(((px - ax) * dx + (py - ay) * dy) / lengthSq, 0, 1);

		float cx = ax + t * dx - px;
		float cy = ay + t * dy - py;
		return Math.Sqrt(cx * cx + cy * cy);
	}

	//! Polled, as the marker ages are: a release lost while the dialog holds the input
	//! context would strand the labels.
	protected void UpdateLabels()
	{
		bool wanted = GetGame().GetInputManager().GetActionValue("LL_MapMarkerInfo") > 0;
		if (!wanted)
		{
			DropLabels();
			return;
		}

		if (!m_bLabelsShown)
			BuildLabels();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float zoom = m_MapEntity.GetCurrentZoom();
		int originX, originY;
		m_MapEntity.WorldToScreen(0, 0, originX, originY, true);

		foreach (int i, TextWidget label : m_aLabels)
		{
			LL_MapLine line = FindCachedLine(m_aLabelLineIds[i]);
			if (!line || line.GetPointCount() == 0)
			{
				label.SetVisible(false);
				continue;
			}

			float worldX, worldY;
			line.GetPoint(0, worldX, worldY);
			float radius = Math.Max(MIN_DRAWN_WIDTH, line.m_fWidth * zoom) * DOT_RADIUS_FACTOR;
			float x = originX + worldX * zoom + radius + LABEL_GAP_PX;
			float y = originY - worldY * zoom;

			FrameSlot.SetPos(label, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
			label.SetVisible(true);
		}
	}

	protected void BuildLabels()
	{
		m_bLabelsShown = true;
		if (!m_Cache || !m_wCanvas)
			return;

		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			return;

		foreach (LL_MapLine line : m_Cache.GetLines())
		{
			// A label must never eat a click meant for the map.
			Widget created = GetGame().GetWorkspace().CreateWidget(
				WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS,
				Color.White,
				0,
				mapFrame);

			TextWidget label = TextWidget.Cast(created);
			if (!label)
				continue;

			label.SetFont(SCR_MapMarkerWidgetComponent.LL_LABEL_FONT);
			label.SetExactFontSize(SCR_MapMarkerWidgetComponent.LL_META_FONT_SIZE);
			label.SetBold(true);
			label.SetColor(Color.FromInt(SCR_MapMarkerWidgetComponent.LL_META_COLOR));
			label.SetShadow(SCR_MapMarkerWidgetComponent.LL_SHADOW_SIZE);
			label.SetTextWrapping(false);
			label.SetZOrder(LABEL_Z_ORDER);
			label.SetVisible(false);
			FrameSlot.SetSizeToContent(label, true);
			FrameSlot.SetAlignment(label, 0, 0.5);

			// Format parameters, never a finished string: SetText translates anything
			// starting with '#'.
			string age = LL_MapMarkersUI.LL_FormatMarkerAge(line.m_CreatedAt);
			if (age.IsEmpty())
				label.SetTextFormat("%1", line.m_sAuthorName);
			else
				label.SetTextFormat("%1 (%2)", line.m_sAuthorName, age);

			m_aLabels.Insert(label);
			m_aLabelLineIds.Insert(line.m_iId);
		}
	}

	protected void DropLabels()
	{
		foreach (TextWidget label : m_aLabels)
		{
			if (label)
				label.RemoveFromHierarchy();
		}

		m_aLabels.Clear();
		m_aLabelLineIds.Clear();
		m_bLabelsShown = false;
	}

	protected LL_MapLine FindCachedLine(int id)
	{
		if (!m_Cache)
			return null;

		foreach (LL_MapLine line : m_Cache.GetLines())
		{
			if (line.m_iId == id)
				return line;
		}

		return null;
	}
}
