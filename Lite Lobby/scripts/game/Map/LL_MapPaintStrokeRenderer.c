//! Projects stroke geometry and builds the canvas commands used by every paint layer.
class LL_MapPaintStrokeRenderer
{
	protected SCR_MapEntity m_Map;

	void LL_MapPaintStrokeRenderer(SCR_MapEntity mapEntity)
	{
		m_Map = mapEntity;
	}

	void Render(LL_MapPaintStroke stroke, CanvasWidget canvas, array<ref CanvasWidgetCommand> commands, float canvasX, float canvasY)
	{
		// Anchor lines keep their map width and zoom with the terrain.
		float penWidth = stroke.GetScreenWidth(m_Map.GetCurrentZoom(), GetGame().GetWorkspace().DPIScale(LL_MapPaintSettings.BRUSH_WIDTH));
		float minimumPointSpacing = penWidth * 0.5;
		float minimumPointSpacingSq = minimumPointSpacing * minimumPointSpacing;
		array<float> vertices = {};
		int pointCount = stroke.m_Points.Count();
		for (int i = 0; i < pointCount; i++)
		{
			vector point = stroke.m_Points[i];
			int x, y;
			m_Map.WorldToScreen(point[0], point[2], x, y, true);

			// Old strokes can have thousands of samples that collapse to the same
			// screen pixels after zooming out. Drop samples closer than half a stroke
			// width while keeping both ends, which avoids overlapping short segments.
			int vertexCount = vertices.Count();
			bool isEndpoint = i == 0 || i == pointCount - 1;
			if (vertexCount >= 2 && !isEndpoint && !stroke.m_bAnchorLine)
			{
				float dx = x - vertices[vertexCount - 2];
				float dy = y - vertices[vertexCount - 1];
				if (dx * dx + dy * dy < minimumPointSpacingSq)
					continue;
			}
			else if (vertexCount >= 2 && isEndpoint && x == vertices[vertexCount - 2] && y == vertices[vertexCount - 1])
			{
				continue;
			}

			vertices.Insert(x - canvasX);
			vertices.Insert(y - canvasY);
		}
		int vertexCount = vertices.Count();
		if (stroke.m_bAnchorLine)
		{
			LL_MapPaintAnchorGeometry.AddLines(vertices, penWidth, LL_MapPaintAnchorStyle.GetColor(stroke.m_iColor), commands);
			return;
		}
		LL_MapPaintCanvas.AddLines(vertices, penWidth, stroke.m_iColor, commands);
		if (vertexCount >= 2)
		{
			AddCap(canvas, commands, Vector(vertices[0], vertices[1], 0), penWidth * 0.5, stroke.m_iColor);
			if (vertexCount >= 4)
				AddCap(canvas, commands, Vector(vertices[vertexCount - 2], vertices[vertexCount - 1], 0), penWidth * 0.5, stroke.m_iColor);
		}
	}

	protected void AddCap(CanvasWidget canvas, array<ref CanvasWidgetCommand> commands, vector center, float radius, int color)
	{
		PolygonDrawCommand cap = new PolygonDrawCommand();
		cap.m_iColor = color;
		cap.m_Vertices = {};
		canvas.TessellateCircle(center, radius, 12, cap.m_Vertices);
		commands.Insert(cap);
	}

	//! Join pending anchor segments to a render copy so shared corners blend once.
	static LL_MapPaintDocument BuildVisibleDocument(LL_MapPaintDocument document, SCR_MapMarkerSyncComponent sync)
	{
		LL_MapPaintDocument visibleDocument = document;
		if (sync)
		{
			foreach (LL_MapPaintPreview preview : sync.LL_GetPaintPreviews())
			{
				LL_MapPaintStroke stroke = preview.m_Stroke;
				if (!stroke.m_bAnchorLine)
					continue;
				if (visibleDocument == document)
					visibleDocument = document.CopyForPreview();
				visibleDocument.AddAnchorSegment(stroke.m_Points, stroke.m_fWidth, stroke.m_iColor, stroke.m_iOwner, preview.m_bAnchorContinuation);
			}
		}
		return visibleDocument;
	}
}
