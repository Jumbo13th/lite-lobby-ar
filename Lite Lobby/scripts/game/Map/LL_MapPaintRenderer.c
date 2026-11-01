//! Owns map canvases, command buffers and redraw invalidation.
class LL_MapPaintRenderer
{
	protected SCR_MapEntity m_Map;
	protected ref LL_MapPaintStrokeRenderer m_StrokeRenderer;
	protected CanvasWidget m_GridCanvas;
	protected CanvasWidget m_Canvas;
	protected CanvasWidget m_PreviewCanvas;
	protected CanvasWidget m_CursorCanvas;
	protected ref array<ref CanvasWidgetCommand> m_GridCommands = {};
	// CanvasWidget retains a pointer to this array, so keep it alive until canvas removal.
	protected ref array<ref CanvasWidgetCommand> m_Commands = {};
	protected ref array<ref CanvasWidgetCommand> m_PreviewCommands = {};
	protected ref array<ref CanvasWidgetCommand> m_CursorCommands = {};
	protected bool m_bDirty = true;
	protected bool m_bDocumentDirty = true;
	protected bool m_bGridDirty = true;
	protected bool m_bCursorVisible;
	protected int m_iDocumentVersion = -1;
	protected int m_iPreviewVersion = -1;
	protected vector m_LastPan;
	protected float m_fLastZoom;
	protected float m_fLastCanvasX;
	protected float m_fLastCanvasY;
	protected float m_fLastCanvasWidth;
	protected float m_fLastCanvasHeight;

	void LL_MapPaintRenderer(SCR_MapEntity mapEntity)
	{
		m_Map = mapEntity;
		m_StrokeRenderer = new LL_MapPaintStrokeRenderer(mapEntity);
	}

	void ~LL_MapPaintRenderer()
	{
		Close();
	}

	CanvasWidget GetCanvas()
	{
		return m_Canvas;
	}

#ifdef LL_MAP_PAINT_INPUT_TEST
	array<ref CanvasWidgetCommand> LL_TestBuildStrokeCommands(LL_MapPaintStroke stroke)
	{
		UpdateCanvasProjection();
		array<ref CanvasWidgetCommand> commands = {};
		float x, y;
		m_Canvas.GetScreenPos(x, y);
		m_StrokeRenderer.Render(stroke, m_Canvas, commands, x, y);
		return commands;
	}#endif

	bool Open(WorkspaceWidget workspace, Widget frame)
	{
		m_Canvas = LL_MapPaintWidgets.CreatePaintCanvas(workspace, frame, "LL_MapPaintCanvas", 10);
		m_PreviewCanvas = LL_MapPaintWidgets.CreatePaintCanvas(workspace, frame, "LL_MapPaintPreviewCanvas", 11);
		m_CursorCanvas = LL_MapPaintWidgets.CreatePaintCanvas(workspace, frame, "LL_MapPaintCursorCanvas", 12);
		if (!m_Canvas || !m_PreviewCanvas || !m_CursorCanvas)
		{
			Close();
			return false;
		}
		if (LL_MapPaintSettings.SPATIAL_GRID_DEBUG_ENABLED)
			m_GridCanvas = LL_MapPaintWidgets.CreatePaintCanvas(workspace, frame, "LL_MapPaintSpatialGridCanvas", 9);
		UpdateCanvasProjection();
		m_Canvas.SetDrawCommands(m_Commands);
		m_PreviewCanvas.SetDrawCommands(m_PreviewCommands);
		m_CursorCanvas.SetDrawCommands(m_CursorCommands);
		if (m_GridCanvas)
			m_GridCanvas.SetDrawCommands(m_GridCommands);
		InvalidateDocument();
		m_bGridDirty = true;
		m_iPreviewVersion = -1;
		return true;
	}

	void Close()
	{
		if (m_Canvas)
			m_Canvas.RemoveFromHierarchy();
		if (m_GridCanvas)
			m_GridCanvas.RemoveFromHierarchy();
		if (m_PreviewCanvas)
			m_PreviewCanvas.RemoveFromHierarchy();
		if (m_CursorCanvas)
			m_CursorCanvas.RemoveFromHierarchy();
		m_Canvas = null;
		m_GridCanvas = null;
		m_PreviewCanvas = null;
		m_CursorCanvas = null;
		m_Commands.Clear();
		m_PreviewCommands.Clear();
		m_CursorCommands.Clear();
		m_GridCommands.Clear();
		m_bCursorVisible = false;
	}

	void InvalidatePreview()
	{
		m_bDirty = true;
	}

	protected void UpdateCanvasProjection()
	{
		bool changed = LL_MapPaintWidgets.ConfigurePixelCanvas(m_Canvas);
		if (LL_MapPaintWidgets.ConfigurePixelCanvas(m_PreviewCanvas))
			changed = true;
		if (LL_MapPaintWidgets.ConfigurePixelCanvas(m_CursorCanvas))
			changed = true;
		if (LL_MapPaintWidgets.ConfigurePixelCanvas(m_GridCanvas))
			changed = true;
		if (changed)
		{
			m_bDocumentDirty = true;
			m_bDirty = true;
			m_bGridDirty = true;
		}
	}

	void InvalidateDocument()
	{
		m_iDocumentVersion = -1;
		m_bDocumentDirty = true;
		m_bDirty = true;
	}

	protected void AddSpatialGridLine(float startX, float startZ, float endX, float endZ, int cell, float canvasX, float canvasY)
	{
		int screenStartX, screenStartY, screenEndX, screenEndY;
		m_Map.WorldToScreen(startX, startZ, screenStartX, screenStartY, true);
		m_Map.WorldToScreen(endX, endZ, screenEndX, screenEndY, true);

		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = 0x6656b4e9;
		line.m_fWidth = 1;
		if (cell % LL_MapPaintDocument.SPATIAL_SUBDIVISIONS == 0)
		{
			line.m_iColor = 0xffe69a22;
			line.m_fWidth = 2;
		}
		line.m_Vertices = {};
		line.m_Vertices.Insert(screenStartX - canvasX);
		line.m_Vertices.Insert(screenStartY - canvasY);
		line.m_Vertices.Insert(screenEndX - canvasX);
		line.m_Vertices.Insert(screenEndY - canvasY);
		m_GridCommands.Insert(line);
	}

	protected void RenderSpatialGrid()
	{
		if (!m_GridCanvas || !m_Map)
			return;

		m_GridCommands.Clear();
		float canvasX, canvasY;
		m_GridCanvas.GetScreenPos(canvasX, canvasY);
		Widget mapWidget = m_Map.GetMapWidget();
		if (!mapWidget)
		{
			m_GridCanvas.SetDrawCommands(m_GridCommands);
			m_bGridDirty = false;
			return;
		}

		float viewLeft, viewTop, viewWidth, viewHeight;
		mapWidget.GetScreenPos(viewLeft, viewTop);
		mapWidget.GetScreenSize(viewWidth, viewHeight);
		vector origin = m_Map.Offset();
		vector size = m_Map.Size();
		float leafWidth = size[0] / (float)LL_MapPaintDocument.SPATIAL_LEAF_COUNT;
		float leafHeight = size[2] / (float)LL_MapPaintDocument.SPATIAL_LEAF_COUNT;
		if (viewWidth <= 0 || viewHeight <= 0 || leafWidth <= 0 || leafHeight <= 0)
		{
			m_GridCanvas.SetDrawCommands(m_GridCommands);
			m_bGridDirty = false;
			return;
		}

		int screenLeft = (int)Math.Floor(viewLeft);
		int screenTop = (int)Math.Floor(viewTop);
		int screenRight = (int)Math.Ceil(viewLeft + viewWidth);
		int screenBottom = (int)Math.Ceil(viewTop + viewHeight);
		float worldTopLeftX, worldTopLeftZ;
		float worldTopRightX, worldTopRightZ;
		float worldBottomLeftX, worldBottomLeftZ;
		float worldBottomRightX, worldBottomRightZ;
		m_Map.ScreenToWorld(screenLeft, screenTop, worldTopLeftX, worldTopLeftZ);
		m_Map.ScreenToWorld(screenRight, screenTop, worldTopRightX, worldTopRightZ);
		m_Map.ScreenToWorld(screenLeft, screenBottom, worldBottomLeftX, worldBottomLeftZ);
		m_Map.ScreenToWorld(screenRight, screenBottom, worldBottomRightX, worldBottomRightZ);
		float visibleMinX = Math.Min(Math.Min(worldTopLeftX, worldTopRightX), Math.Min(worldBottomLeftX, worldBottomRightX));
		float visibleMaxX = Math.Max(Math.Max(worldTopLeftX, worldTopRightX), Math.Max(worldBottomLeftX, worldBottomRightX));
		float visibleMinZ = Math.Min(Math.Min(worldTopLeftZ, worldTopRightZ), Math.Min(worldBottomLeftZ, worldBottomRightZ));
		float visibleMaxZ = Math.Max(Math.Max(worldTopLeftZ, worldTopRightZ), Math.Max(worldBottomLeftZ, worldBottomRightZ));
		int firstCellX = (int)Math.Floor((visibleMinX - origin[0]) / leafWidth);
		int lastCellX = (int)Math.Ceil((visibleMaxX - origin[0]) / leafWidth);
		int firstCellZ = (int)Math.Floor((visibleMinZ - origin[2]) / leafHeight);
		int lastCellZ = (int)Math.Ceil((visibleMaxZ - origin[2]) / leafHeight);
		for (int cellX = firstCellX; cellX <= lastCellX; cellX++)
		{
			float worldX = origin[0] + cellX * leafWidth;
			AddSpatialGridLine(worldX, visibleMinZ, worldX, visibleMaxZ, cellX, canvasX, canvasY);
		}
		for (int cellZ = firstCellZ; cellZ <= lastCellZ; cellZ++)
		{
			float worldZ = origin[2] + cellZ * leafHeight;
			AddSpatialGridLine(visibleMinX, worldZ, visibleMaxX, worldZ, cellZ, canvasX, canvasY);
		}

		m_GridCanvas.SetDrawCommands(m_GridCommands);
		m_bGridDirty = false;
	}

	protected void RenderDocument(LL_MapPaintDocument document, SCR_MapMarkerSyncComponent sync)
	{
		LL_MapPaintDocument visibleDocument = LL_MapPaintStrokeRenderer.BuildVisibleDocument(document, sync);
		m_Commands.Clear();
		float x, y;
		m_Canvas.GetScreenPos(x, y);
		foreach (LL_MapPaintStroke stroke : visibleDocument.m_Strokes)
			m_StrokeRenderer.Render(stroke, m_Canvas, m_Commands, x, y);
		m_Canvas.SetDrawCommands(m_Commands);
		m_bDocumentDirty = false;
	}

	protected void RenderPreview(SCR_MapMarkerSyncComponent sync, LL_MapPaintStroke stroke)
	{
		m_PreviewCommands.Clear();
		float canvasX, canvasY;
		m_PreviewCanvas.GetScreenPos(canvasX, canvasY);
		if (sync)
		{
			foreach (LL_MapPaintPreview preview : sync.LL_GetPaintPreviews())
			{
				if (!preview.m_Stroke.m_bAnchorLine)
					m_StrokeRenderer.Render(preview.m_Stroke, m_PreviewCanvas, m_PreviewCommands, canvasX, canvasY);
			}
		}
		if (stroke && !stroke.m_bAnchorLine)
			m_StrokeRenderer.Render(stroke, m_PreviewCanvas, m_PreviewCommands, canvasX, canvasY);
		m_PreviewCanvas.SetDrawCommands(m_PreviewCommands);
		m_bDirty = false;
	}

	protected void RenderCursor(bool canPaint, LL_EMapPaintMode mode)
	{
		if (canPaint)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			float radius = workspace.DPIScale(LL_MapPaintSettings.BRUSH_WIDTH * 0.5);
			if (mode == LL_EMapPaintMode.ERASER)
				radius = workspace.DPIScale(LL_MapPaintSettings.ERASER_RADIUS);
			else if (mode == LL_EMapPaintMode.ANCHOR)
				radius = LL_MapPaintAnchorStyle.GetScreenWidth(m_Map.GetCurrentZoom()) * 0.5;
			float canvasX, canvasY;
			m_CursorCanvas.GetScreenPos(canvasX, canvasY);
			vector center = Vector(workspace.DPIScale(SCR_MapCursorInfo.x) - canvasX, workspace.DPIScale(SCR_MapCursorInfo.y) - canvasY, 0);
			LineDrawCommand cursor = new LineDrawCommand();
			cursor.m_iColor = 0xff202020;
			cursor.m_iOutlineColor = 0xffffffff;
			cursor.m_fOutlineWidth = 1;
			cursor.m_fWidth = 1;
			cursor.m_bShouldEnclose = true;
			cursor.m_Vertices = {};
			m_CursorCanvas.TessellateCircle(center, radius, 32, cursor.m_Vertices);
			m_CursorCommands.Clear();
			m_CursorCommands.Insert(cursor);
			m_CursorCanvas.SetDrawCommands(m_CursorCommands);
			m_bCursorVisible = true;
		}
		else if (m_bCursorVisible)
		{
			m_CursorCommands.Clear();
			m_CursorCanvas.SetDrawCommands(m_CursorCommands);
			m_bCursorVisible = false;
		}
	}

	void Update(LL_MapPaintDocument document, SCR_MapMarkerSyncComponent sync, LL_MapPaintStroke stroke, bool canPaint, LL_EMapPaintMode mode)
	{
		if (!m_Canvas || !document)
			return;
		UpdateCanvasProjection();
		if (document.GetVersion() != m_iDocumentVersion)
		{
			m_iDocumentVersion = document.GetVersion();
			m_bDocumentDirty = true;
			m_bDirty = true;
		}
		if (sync && sync.LL_GetPreviewVersion() != m_iPreviewVersion)
		{
			m_iPreviewVersion = sync.LL_GetPreviewVersion();
			m_bDocumentDirty = true;
			m_bDirty = true;
		}
		else if (!sync && m_iPreviewVersion != -1)
		{
			m_iPreviewVersion = -1;
			m_bDocumentDirty = true;
			m_bDirty = true;
		}

		float canvasX, canvasY, width, height;
		m_Canvas.GetScreenPos(canvasX, canvasY);
		m_Canvas.GetScreenSize(width, height);
		vector pan = m_Map.GetCurrentPan();
		float zoom = m_Map.GetCurrentZoom();
		bool transformChanged = pan != m_LastPan || zoom != m_fLastZoom
			|| canvasX != m_fLastCanvasX || canvasY != m_fLastCanvasY || width != m_fLastCanvasWidth || height != m_fLastCanvasHeight;
		if (transformChanged)
		{
			m_bDocumentDirty = true;
			m_bDirty = true;
			m_bGridDirty = true;
		}
		if (LL_MapPaintSettings.SPATIAL_GRID_DEBUG_ENABLED && m_bGridDirty)
			RenderSpatialGrid();
		if (m_bDocumentDirty)
			RenderDocument(document, sync);
		if (m_bDirty)
			RenderPreview(sync, stroke);
		if (mode != LL_EMapPaintMode.NONE || m_bCursorVisible)
			RenderCursor(canPaint, mode);

		m_LastPan = pan;
		m_fLastZoom = zoom;
		m_fLastCanvasX = canvasX;
		m_fLastCanvasY = canvasY;
		m_fLastCanvasWidth = width;
		m_fLastCanvasHeight = height;
	}
}
