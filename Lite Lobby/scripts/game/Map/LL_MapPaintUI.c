//! Coordinates map lifecycle, tool activation and local drawing gestures.
class LL_MapPaintUI
{
	protected SCR_MapEntity m_Map;
	protected SCR_MapDrawingUI m_Owner;
	protected SCR_MapCursorModule m_Cursor;
	protected LL_MapPaintToolState m_ToolState;
	protected ref LL_MapPaintPanel m_Panel;
	protected SCR_MapToolEntry m_PaintMenuEntry;
	protected SCR_MapToolEntry m_HistoryMenuEntry;
	protected ref LL_MapPaintHistoryIcon m_HistoryIcon;
	protected ref LL_MapPaintHistoryPanel m_HistoryPanel;
	protected ref LL_MapPaintRenderer m_Renderer;
	protected ref LL_MapPaintAuthorTooltip m_AuthorTooltip;
	protected ref LL_MapPaintHotkeys m_Hotkeys;
	protected ref LL_MapPaintAnchorTool m_AnchorTool;
	protected ref LL_MapPaintDocument m_Document;
	protected ref LL_MapPaintStroke m_Stroke;
	protected ref array<vector> m_ErasePoints = {};
	protected SCR_MapMarkerSyncComponent m_PaintSync;
	protected LL_EMapPaintMode m_Mode;
	protected bool m_bOpen;
	protected bool m_bGesture;
	protected bool m_bSentBatch;
	protected float m_fSendTime;
	protected float m_fEraserSize;
	protected vector m_vCircleCenter;
	protected vector m_vCircleEdge;

	void LL_MapPaintUI(SCR_MapEntity mapEntity, SCR_MapDrawingUI owner)
	{
		m_Map = mapEntity;
		m_Owner = owner;
		m_Document = mapEntity.LL_GetPaintDocument();
		m_ToolState = mapEntity.LL_GetPaintToolState();
		SCR_MapToolMenuUI toolMenu = SCR_MapToolMenuUI.Cast(mapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (toolMenu)
		{
			m_PaintMenuEntry = toolMenu.RegisterToolMenuEntry("{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "marker", int.MAX);
			m_PaintMenuEntry.m_OnClick.Insert(LL_ToggleToolPanel);
			m_PaintMenuEntry.SetVisible(false);
			m_HistoryMenuEntry = toolMenu.RegisterToolMenuEntry("{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "watch", int.MAX);
			m_HistoryMenuEntry.m_OnClick.Insert(LL_ToggleHistoryPanel);
			m_HistoryMenuEntry.SetVisible(false);
		}
		m_Panel = new LL_MapPaintPanel(this, m_ToolState, m_PaintMenuEntry);
		m_HistoryPanel = new LL_MapPaintHistoryPanel();
		m_HistoryIcon = new LL_MapPaintHistoryIcon(m_HistoryMenuEntry);
		m_Renderer = new LL_MapPaintRenderer(mapEntity);
		m_AuthorTooltip = new LL_MapPaintAuthorTooltip();
		m_Hotkeys = new LL_MapPaintHotkeys(this, m_ToolState, m_Panel);
		m_AnchorTool = new LL_MapPaintAnchorTool();
	}

	void OnOpen(MapConfiguration config)
	{
		m_Cursor = SCR_MapCursorModule.Cast(m_Map.GetMapModule(SCR_MapCursorModule));
		Widget frame = config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!frame || !m_Cursor)
			return;
		ConfigureDocument();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!m_Renderer.Open(workspace, frame))
			return;
		m_Panel.Open(workspace);
		m_AuthorTooltip.Open(workspace, frame);
		m_bOpen = true;
		RefreshSynchronization();
		RefreshButtons();
		SetInteractionListeners(true);
		m_Hotkeys.Open();
	}

	void OnClose()
	{
		StopActiveMode();
		m_ToolState.Reset();
		if (m_bOpen)
			SetInteractionListeners(false);
		m_Hotkeys.Close();
		m_bOpen = false;
		m_Panel.Close();
		m_HistoryPanel.Close();
		m_HistoryIcon.Close();
		if (m_HistoryMenuEntry)
		{
			m_HistoryMenuEntry.SetActive(false);
			m_HistoryMenuEntry.SetVisible(false);
		}
		m_AuthorTooltip.Close();
		m_Renderer.Close();
		m_Cursor = null;
	}

	protected void ConfigureDocument()
	{
		if (m_Document)
			m_Document.ConfigureSpatialGrid(m_Map.Offset(), m_Map.Size());
	}

	protected void RefreshSynchronization()
	{
		m_PaintSync = SCR_MapMarkerSyncComponent.LL_GetLocalPaintSync();
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			manager.LL_EnsurePaintSync();
	}

	protected void SetInteractionListeners(bool enabled)
	{
		// Both actions are needed: MapSelect also catches short clicks filtered by MapDrag.
		InputManager input = GetGame().GetInputManager();
		if (enabled)
		{
			input.AddActionListener("MapDrag", EActionTrigger.DOWN, OnPress);
			input.AddActionListener("MapDrag", EActionTrigger.UP, OnRelease);
			input.AddActionListener("MapSelect", EActionTrigger.DOWN, OnPress);
			input.AddActionListener("MapSelect", EActionTrigger.UP, OnDropdownSelect);
			input.AddActionListener("MapContextualMenu", EActionTrigger.DOWN, OnMapPointerInteraction);
			input.AddActionListener("MapPanDrag", EActionTrigger.DOWN, OnMapPointerInteraction);
			m_Map.GetOnMapPan().Insert(OnMapPan);
			m_Map.GetOnMapZoom().Insert(OnMapZoom);
		}
		else
		{
			input.RemoveActionListener("MapDrag", EActionTrigger.DOWN, OnPress);
			input.RemoveActionListener("MapDrag", EActionTrigger.UP, OnRelease);
			input.RemoveActionListener("MapSelect", EActionTrigger.DOWN, OnPress);
			input.RemoveActionListener("MapSelect", EActionTrigger.UP, OnDropdownSelect);
			input.RemoveActionListener("MapContextualMenu", EActionTrigger.DOWN, OnMapPointerInteraction);
			input.RemoveActionListener("MapPanDrag", EActionTrigger.DOWN, OnMapPointerInteraction);
			m_Map.GetOnMapPan().Remove(OnMapPan);
			m_Map.GetOnMapZoom().Remove(OnMapZoom);
		}
	}

	protected void OnMapPointerInteraction(float value, EActionTrigger reason)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float x = workspace.DPIScale(SCR_MapCursorInfo.x);
		float y = workspace.DPIScale(SCR_MapCursorInfo.y);
		Widget historyButton;
		if (m_HistoryMenuEntry && m_HistoryMenuEntry.m_ButtonComp)
			historyButton = m_HistoryMenuEntry.m_ButtonComp.GetRootWidget();
		// Let the history button toggle on release instead of closing on press
		// and reopening the same window when its click callback arrives.
		if (m_HistoryPanel.IsOpen() && !m_HistoryPanel.ContainsPoint(x, y) && !LL_MapPaintWidgets.IsPointInsideWidget(historyButton, x, y))
			m_HistoryPanel.Close();
		// Controls inside the tool window and its picker remain interactive.
		if (IsCursorOverMap())
			m_Panel.DismissTools();
	}

	protected void OnMapPan(float x, float y, bool adjustedPan)
	{
		m_Panel.DismissTools();
	}

	protected void OnMapZoom(float zoom)
	{
		m_Panel.DismissTools();
	}

#ifdef LL_MAP_PAINT_INPUT_TEST
	bool LL_TestCanUseToolHotkeys()
	{
		return m_Hotkeys.CanUse();
	}

	Widget LL_TestGetPaintPanel()
	{
		return m_Panel.LL_GetPanelWidget();
	}

	SCR_MapToolEntry LL_TestGetPaintMenuEntry()
	{
		return m_PaintMenuEntry;
	}

	SCR_MapToolEntry LL_TestGetHistoryMenuEntry()
	{
		return m_HistoryMenuEntry;
	}

	LL_MapPaintRenderer LL_TestGetRenderer()
	{
		return m_Renderer;
	}

	LL_MapPaintStroke LL_TestGetGestureStroke()
	{
		return m_Stroke;
	}
#endif

	protected bool IsPaintAllowed(SCR_MapMarkerManagerComponent manager)
	{
		return LL_MapPaintPermissions.IsPaintAllowed(m_Map, m_Cursor, m_PaintSync, m_bOpen, manager);
	}

	protected void RefreshButtons()
	{
		m_Panel.Refresh(IsPaintAllowed(SCR_MapMarkerManagerComponent.GetInstance()));
		if (m_HistoryMenuEntry)
		{
			bool allowed = CanReadHistory();
			if (!allowed)
				m_HistoryPanel.Close();
			m_HistoryMenuEntry.SetVisible(m_bOpen);
			m_HistoryMenuEntry.SetEnabled(allowed);
			m_HistoryMenuEntry.SetActive(allowed && m_HistoryPanel.IsOpen());
			if (m_HistoryMenuEntry.m_ButtonComp)
				m_HistoryMenuEntry.m_ButtonComp.SetEnabled(allowed, false);
			m_HistoryIcon.Update(GetGame().GetWorkspace());
		}
	}

	protected bool CanReadHistory()
	{
		if (!m_bOpen || m_Map.GetMapConfig().MapEntityMode == EMapEntityMode.SPAWNSCREEN)
			return false;
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (LL_MapPaintPermissions.IsLocalPaintFallback(manager, m_PaintSync))
			return true;
		return manager && m_PaintSync && !manager.LL_GetLocalPaintFactionKey().IsEmpty();
	}

	void LL_ToggleHistoryPanel()
	{
		if (m_HistoryPanel.IsOpen())
			m_HistoryPanel.Close();
		else if (CanReadHistory())
			m_HistoryPanel.Open(GetGame().GetWorkspace(), m_HistoryMenuEntry, m_Map, m_PaintSync);
		RefreshButtons();
	}

	void LL_OnPanelChanged()
	{
		m_Renderer.InvalidatePreview();
		RefreshButtons();
	}

	void LL_ToggleToolPanel()
	{
		m_HistoryPanel.Close();
		if (LL_CanUseToolHotkeys())
			m_Panel.ToggleTools();
	}

	bool LL_CanUseToolHotkeys()
	{
		return m_bOpen && IsPaintAllowed(SCR_MapMarkerManagerComponent.GetInstance());
	}

	void LL_RestoreToolState(LL_EMapPaintMode mode, bool enabled)
	{
		StopActiveMode();
		m_ToolState.m_SelectedMode = mode;
		m_ToolState.m_bEnabled = enabled;
		TryActivateSelectedMode();
		RefreshButtons();
	}

	void LL_SelectTool(LL_EMapPaintMode mode)
	{
		m_Hotkeys.ResetTemporaryTools();
		m_Panel.SetColorPickerOpen(false);
		if (m_ToolState.IsSelected(mode))
			Disable();
		else
			SetMode(mode);
	}

	void SetMode(LL_EMapPaintMode mode)
	{
		m_ToolState.Select(mode);
		TryActivateSelectedMode();
		RefreshButtons();
	}

	protected void TryActivateSelectedMode()
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!m_ToolState.m_bEnabled || m_ToolState.m_SelectedMode == LL_EMapPaintMode.NONE || !IsPaintAllowed(manager))
			return;
		if (m_Mode == m_ToolState.m_SelectedMode && m_Map.LL_IsPaintModeActive())
			return;

		if (m_Mode != LL_EMapPaintMode.NONE)
			StopActiveMode();
		if (!m_Cursor)
			return;

		m_Owner.LL_StopLineDrawing();
		bool drawEnabled = m_Cursor.HandleDraw(true);
		if (!drawEnabled && !(m_Cursor.GetCursorState() & EMapCursorState.CS_DRAW))
			return;

		m_Mode = m_ToolState.m_SelectedMode;
		m_Map.LL_SetPaintModeActive(true);
		m_Renderer.InvalidatePreview();
	}

	protected void StopActiveMode()
	{
		EndGesture();
		m_AnchorTool.Reset();
		if (m_Mode != LL_EMapPaintMode.NONE && m_Cursor)
			m_Cursor.HandleDraw(false);
		m_Mode = LL_EMapPaintMode.NONE;
		m_Map.LL_SetPaintModeActive(false);
		m_Renderer.InvalidatePreview();
	}

	void Disable()
	{
		m_Hotkeys.ResetTemporaryTools();
		m_ToolState.m_bEnabled = false;
		StopActiveMode();
		RefreshButtons();
	}

	void OnEntryToggled(SCR_MapToolEntry entry)
	{
		if (entry == m_PaintMenuEntry || entry == m_HistoryMenuEntry)
			return;
		m_HistoryPanel.Close();
		m_Panel.SetToolsOpen(false);
		if (m_ToolState.m_bEnabled)
			Disable();
	}

	protected bool CanPaint()
	{
		if (m_Mode == LL_EMapPaintMode.NONE || !m_Document || !IsPaintAllowed(SCR_MapMarkerManagerComponent.GetInstance()))
			return false;
		// CS_DRAG is allowed; the cursor integration suppresses panning while painting.
		return IsCursorOverMap() && !LL_MapPaintWidgets.HasBlockingControl(true);
	}

	bool LL_IsHistoryHovered()
	{
		if (!m_bOpen)
			return false;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		return m_HistoryPanel.ContainsPoint(workspace.DPIScale(SCR_MapCursorInfo.x), workspace.DPIScale(SCR_MapCursorInfo.y));
	}

	protected bool IsCursorOverMap()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float x = workspace.DPIScale(SCR_MapCursorInfo.x);
		float y = workspace.DPIScale(SCR_MapCursorInfo.y);
		return !m_Panel.ContainsPoint(x, y) && !m_HistoryPanel.ContainsPoint(x, y) && LL_MapPaintWidgets.IsPointInsideWidget(m_Map.GetMapWidget(), x, y);
	}

#ifdef LL_MAP_PAINT_SMOKE
	bool LL_TestStartGesture()
	{
		OnPress(1, EActionTrigger.DOWN);
		bool started = m_bGesture;
		CancelGesture();
		return started;
	}

	bool LL_TestGestureActive()
	{
		return m_bGesture;
	}

	void LL_TestCancelGesture()
	{
		CancelGesture();
	}
#endif

	void LL_HandleMapInput(float value, EActionTrigger reason)
	{
		if (reason == EActionTrigger.DOWN)
			OnPress(value, reason);
		else if (reason == EActionTrigger.UP)
			OnRelease(value, reason);
	}

	protected vector CursorWorldPosition()
	{
		float x, y;
		m_Map.GetMapCursorWorldPosition(x, y);
		return Vector(x, 0, y);
	}

	protected float PixelsToWorld(float pixels)
	{
		return GetGame().GetWorkspace().DPIScale(pixels) / Math.Max(m_Map.GetCurrentZoom(), 0.0001);
	}

	protected bool IsDragShape()
	{
		return m_Mode == LL_EMapPaintMode.LINE || m_Mode == LL_EMapPaintMode.CIRCLE;
	}

	protected void OnPress(float value, EActionTrigger reason)
	{
		OnMapPointerInteraction(value, reason);
		// The color popup consumes map input, but tool selection is deferred to release.
		m_Panel.BeginPointerPress();
		if (m_Panel.IsColorPickerOpen())
			return;
		if (m_bGesture)
			return;

		if (!CanPaint())
			return;
		if (m_Mode == LL_EMapPaintMode.ANCHOR)
		{
			m_AnchorTool.BeginPress();
			return;
		}

		vector position = CursorWorldPosition();
		if (m_Mode == LL_EMapPaintMode.BRUSH || IsDragShape())
		{
			int requiredPoints = 1;
			if (m_Mode == LL_EMapPaintMode.LINE)
				requiredPoints = 2;
			else if (m_Mode == LL_EMapPaintMode.CIRCLE)
				requiredPoints = LL_MapPaintCircleGeometry.MIN_SEGMENTS + 1;
			if (m_Document.GetPointCount() + requiredPoints > LL_MapPaintDocument.MAX_POINTS)
				return;
			m_Stroke = new LL_MapPaintStroke();
			m_Stroke.m_fWidth = PixelsToWorld(LL_MapPaintSettings.BRUSH_WIDTH);
			m_Stroke.m_iColor = m_Panel.GetBrushColor();
			if (m_Mode == LL_EMapPaintMode.CIRCLE)
			{
				m_vCircleCenter = position;
				m_vCircleEdge = position;
			}
			else
				m_Stroke.m_Points.Insert(position);
			if (m_Mode == LL_EMapPaintMode.LINE)
				m_Stroke.m_Points.Insert(position);
		}
		else
		{
			m_ErasePoints.Clear();
			m_ErasePoints.Insert(position);
			m_fEraserSize = PixelsToWorld(LL_MapPaintSettings.ERASER_RADIUS);
		}
		m_bSentBatch = false;
		m_fSendTime = 0;
		m_bGesture = true;
		m_Renderer.InvalidatePreview();
	}

	protected void OnDropdownSelect(float value, EActionTrigger reason)
	{
		OnRelease(value, reason);
	}

	protected void OnRelease(float value, EActionTrigger reason)
	{
		if (m_Panel.HandleDropdownInput())
			return;
		if (m_Mode == LL_EMapPaintMode.ANCHOR)
		{
			if (m_AnchorTool.ConsumePress())
			{
				if (CanPaint())
					PlaceAnchorPoint();
			}
			return;
		}
		if (m_bGesture && CanPaint())
		{
			if (IsDragShape())
				UpdateShapeGesture();
			else
				SampleGesture(true);
		}
		EndGesture();
	}

	protected void PlaceAnchorPoint()
	{
		if (!m_Document)
			return;
		vector position = CursorWorldPosition();
		m_Stroke = m_AnchorTool.CreateSegment(position, m_Map.GetCurrentZoom(), m_Panel.GetBrushColor(), m_Document.GetPointCount());
		if (!m_Stroke)
			return;
		m_bSentBatch = false;
		bool submitted = FlushGesture();
		CancelGesture();
		if (submitted)
			m_AnchorTool.Commit(position);
	}

	protected void EndGesture()
	{
		if (m_bGesture)
			FlushGesture();
		CancelGesture();
	}

	protected void SampleGesture(bool finalPoint = false)
	{
		vector position = CursorWorldPosition();
		array<vector> points = m_ErasePoints;
		if (m_Mode == LL_EMapPaintMode.BRUSH && m_Stroke)
			points = m_Stroke.m_Points;
		// A stalled connection must never let a packet grow past the server limit.
		if (points.Count() >= LL_MapPaintProtocol.BATCH_POINTS)
		{
			FlushGesture();
			if (points.Count() >= LL_MapPaintProtocol.BATCH_POINTS)
				return;
		}
		float spacing = PixelsToWorld(LL_MapPaintSettings.SAMPLE_SPACING);
		if (finalPoint)
			spacing = 0;
		if (!points.IsEmpty() && vector.DistanceSq(points[points.Count() - 1], position) > spacing * spacing)
		{
			points.Insert(position);
			m_Renderer.InvalidatePreview();
		}
		if (points.Count() >= LL_MapPaintProtocol.BATCH_POINTS)
			FlushGesture();
	}

	protected void UpdateShapeGesture()
	{
		if (!m_Stroke)
			return;

		vector position = CursorWorldPosition();
		if (m_Mode == LL_EMapPaintMode.CIRCLE)
		{
			if (m_vCircleEdge != position)
			{
				m_vCircleEdge = position;
				LL_MapPaintCircleGeometry.BuildPoints(m_vCircleCenter, position, m_Stroke.m_Points);
				m_Renderer.InvalidatePreview();
			}
			return;
		}
		if (m_Stroke.m_Points.Count() != 2)
			return;
		if (m_Stroke.m_Points[1] != position)
		{
			m_Stroke.m_Points[1] = position;
			m_Renderer.InvalidatePreview();
		}
	}

	protected bool FlushGesture()
	{
		array<vector> points = m_ErasePoints;
		int kind = LL_EMapPaintOperation.ERASE;
		float size = m_fEraserSize;
		if (m_Stroke)
		{
			points = m_Stroke.m_Points;
			kind = LL_EMapPaintOperation.BRUSH;
			if (m_Mode == LL_EMapPaintMode.LINE)
				kind = LL_EMapPaintOperation.LINE;
			else if (m_Mode == LL_EMapPaintMode.CIRCLE)
			{
				kind = LL_EMapPaintOperation.CIRCLE;
				// Send two controls once; every receiver reconstructs the same contour.
				points = {m_vCircleCenter, m_vCircleEdge};
			}
			if (m_Stroke.m_bAnchorLine)
				kind = m_AnchorTool.GetOperationKind();
			size = m_Stroke.m_fWidth;
		}
		if (points.IsEmpty() || (m_bSentBatch && points.Count() == 1))
			return false;
		int color = m_Panel.GetBrushColor();
		if (m_Stroke)
			color = m_Stroke.m_iColor;

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		bool localFallback = LL_MapPaintPermissions.IsLocalPaintFallback(manager, m_PaintSync);
		if (localFallback)
		{
			if (!m_Document || !LL_MapPaintProtocol.Validate(kind, size, points))
				return false;
			PlayerController localController = GetGame().GetPlayerController();
			int ownerId = -1;
			if (localController)
				ownerId = localController.GetPlayerId();
			array<int> erasedOwners;
			if (kind == LL_EMapPaintOperation.ERASE)
				erasedOwners = {};
			bool changed = LL_MapPaintProtocol.Apply(m_Document, kind, size, color, points, ownerId, erasedOwners);
			if (LL_MapPaintProtocol.IsStroke(kind) && !changed)
				return false;
			if (changed)
				m_Map.LL_RecordLocalPaintHistory(ownerId, kind, erasedOwners);
		}
		else if (!m_PaintSync || !m_PaintSync.LL_SubmitPaint(kind, size, color, points))
			return false;
		vector last = points[points.Count() - 1];
		points.Clear();
		points.Insert(last); // Share the endpoint across batches without joining separate gestures.
		m_bSentBatch = true;
		m_fSendTime = 0;
		m_Renderer.InvalidatePreview();
		return true;
	}

	protected void CancelGesture()
	{
		m_bGesture = false;
		m_Stroke = null;
		m_ErasePoints.Clear();
		m_Renderer.InvalidatePreview();
	}

	protected void UpdateDocument()
	{
		LL_MapPaintDocument activeDocument = m_Map.LL_GetPaintDocument();
		if (activeDocument == m_Document)
			return;
		// Never submit an old faction's in-progress gesture to the new document.
		m_HistoryPanel.Close();
		CancelGesture();
		StopActiveMode();
		m_Document = activeDocument;
		ConfigureDocument();
		m_Renderer.InvalidateDocument();
	}

	protected void UpdateGesture(float timeSlice)
	{
		if (!m_bGesture)
			return;
		if (CanPaint())
		{
			if (IsDragShape())
				UpdateShapeGesture();
			else
				SampleGesture();
		}
		else
			EndGesture(); // Never connect across dialogs, map panning or UI controls.
		m_fSendTime += timeSlice;
		if (m_bGesture && !IsDragShape() && m_fSendTime >= LL_MapPaintProtocol.SEND_INTERVAL)
			FlushGesture();
	}

	void Update(float timeSlice)
	{
		if (!m_bOpen || !m_Renderer.GetCanvas())
			return;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_Panel.UpdateIcons(workspace);
		RefreshSynchronization();
		UpdateDocument();
		m_HistoryPanel.Update();
		m_Hotkeys.Update();
		// Tool menus and marker permissions can change after OnOpen.
		if (m_Mode != LL_EMapPaintMode.NONE && !IsPaintAllowed(SCR_MapMarkerManagerComponent.GetInstance()))
			StopActiveMode();
		TryActivateSelectedMode();
		RefreshButtons();
		bool tooltipBlocked = m_Panel.IsColorPickerOpen() || LL_IsHistoryHovered() || LL_MapPaintWidgets.HasBlockingControl(true);
		m_AuthorTooltip.Update(workspace, m_Renderer.GetCanvas(), m_Document, CursorWorldPosition(), PixelsToWorld(5), tooltipBlocked);
		UpdateGesture(timeSlice);
		m_Renderer.Update(m_Document, m_PaintSync, m_Stroke, CanPaint(), m_Mode);
	}
}
