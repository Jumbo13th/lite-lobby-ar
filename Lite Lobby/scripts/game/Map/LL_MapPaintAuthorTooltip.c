//! Finds the topmost stroke under the cursor and displays its author in map space.
class LL_MapPaintAuthorTooltip
{
	protected static const float TOOLTIP_WIDTH = 220;
	protected static const float TOOLTIP_HEIGHT = 28;
	protected static const int BACKGROUND_COLOR = LL_MapPaintSettings.PANEL_COLOR;

	protected FrameWidget m_wAuthorTooltip;
	protected CanvasWidget m_wAuthorTooltipBackground;
	protected TextWidget m_wAuthorTooltipText;
	// CanvasWidget keeps a pointer to the commands, so retain them until removal.
	protected ref array<ref CanvasWidgetCommand> m_BackgroundCommands = {};
	protected ref array<ref LL_MapPaintSpatialSegment> m_HoverCandidates = {};
	protected int m_iAuthorTooltipOwner = -1;

	void ~LL_MapPaintAuthorTooltip()
	{
		Close();
	}

	void Close()
	{
		if (m_wAuthorTooltip)
			m_wAuthorTooltip.RemoveFromHierarchy();
		m_wAuthorTooltip = null;
		m_wAuthorTooltipBackground = null;
		m_wAuthorTooltipText = null;
		m_BackgroundCommands.Clear();
		m_HoverCandidates.Clear();
		m_iAuthorTooltipOwner = -1;
	}

	void Open(WorkspaceWidget workspace, Widget frame)
	{
		Close();
		m_wAuthorTooltip = FrameWidget.Cast(LL_MapPaintWidgets.CreatePanelWidget(workspace, frame, WidgetType.FrameWidgetTypeID, "LL_MapPaintAuthorTooltip", 0, 0, 0, TOOLTIP_WIDTH, TOOLTIP_HEIGHT, 0xffffffff, true));
		if (!m_wAuthorTooltip)
			return;

		m_wAuthorTooltip.SetZOrder(40);
		m_wAuthorTooltip.SetVisible(false);
		m_wAuthorTooltipBackground = LL_MapPaintWidgets.CreatePaintCanvas(workspace, m_wAuthorTooltip, "LL_MapPaintAuthorTooltipBackground", 0);
		if (!m_wAuthorTooltipBackground)
		{
			Close();
			return;
		}
		m_wAuthorTooltipBackground.SetIsColorInherited(false);
		m_wAuthorTooltipText = LL_MapPaintWidgets.CreatePanelText(workspace, m_wAuthorTooltip, "LL_MapPaintAuthorTooltipText", 8, 1, TOOLTIP_WIDTH - 8, TOOLTIP_HEIGHT - 1, string.Empty);
		if (!m_wAuthorTooltipText)
			Close();
	}

	protected void UpdateBackground()
	{
		if (!m_wAuthorTooltipBackground)
			return;

		bool changed = LL_MapPaintWidgets.ConfigurePixelCanvas(m_wAuthorTooltipBackground);
		if (!changed && !m_BackgroundCommands.IsEmpty())
			return;

		float width, height;
		m_wAuthorTooltipBackground.GetScreenSize(width, height);
		if (width <= 0 || height <= 0)
			return;

		// Draw a solid rectangle directly, bypassing the default panel style.
		PolygonDrawCommand background = new PolygonDrawCommand();
		background.m_iColor = BACKGROUND_COLOR;
		background.m_Vertices = {0, 0, width, 0, width, height, 0, height};
		m_BackgroundCommands.Clear();
		m_BackgroundCommands.Insert(background);
		m_wAuthorTooltipBackground.SetDrawCommands(m_BackgroundCommands);
	}

	protected LL_MapPaintStroke FindStrokeAtPosition(LL_MapPaintDocument document, vector position, float hitTolerance)
	{
		if (!document)
			return null;

		document.QuerySpatialGrid(position, hitTolerance, m_HoverCandidates);
		LL_MapPaintStroke foundStroke;
		int foundDrawOrder = -1;
		foreach (LL_MapPaintSpatialSegment segment : m_HoverCandidates)
		{
			LL_MapPaintStroke stroke = segment.m_Stroke;
			if (!stroke || stroke.m_iOwner < 0 || stroke.m_Points.IsEmpty())
				continue;

			float radius = stroke.m_fWidth * 0.5 + hitTolerance;
			bool hit;
			if (segment.m_iSegmentIndex == 0)
			{
				if (vector.DistanceSq(stroke.m_Points[0], position) <= radius * radius)
					hit = true;
			}
			else
			{
				float first, last;
				int pointIndex = segment.m_iSegmentIndex;
				hit = LL_MapPaintGeometry.CircleInterval(stroke.m_Points[pointIndex - 1], stroke.m_Points[pointIndex], position, radius, first, last);
			}
			if (hit && stroke.m_iDrawOrder > foundDrawOrder)
			{
				foundStroke = stroke;
				foundDrawOrder = stroke.m_iDrawOrder;
			}
		}
		return foundStroke;
	}

	protected void HideAuthorTooltip()
	{
		m_iAuthorTooltipOwner = -1;
		if (m_wAuthorTooltip)
			m_wAuthorTooltip.SetVisible(false);
	}

	void Update(WorkspaceWidget workspace, CanvasWidget canvas, LL_MapPaintDocument document, vector position, float hitTolerance, bool blocked)
	{
		if (!workspace || !m_wAuthorTooltip || !m_wAuthorTooltipText || !canvas || blocked)
		{
			HideAuthorTooltip();
			return;
		}

		float canvasX, canvasY, canvasWidth, canvasHeight;
		canvas.GetScreenPos(canvasX, canvasY);
		canvas.GetScreenSize(canvasWidth, canvasHeight);
		float cursorX = workspace.DPIScale(SCR_MapCursorInfo.x);
		float cursorY = workspace.DPIScale(SCR_MapCursorInfo.y);
		if (cursorX < canvasX || cursorY < canvasY || cursorX >= canvasX + canvasWidth || cursorY >= canvasY + canvasHeight)
		{
			HideAuthorTooltip();
			return;
		}

		LL_MapPaintStroke stroke = FindStrokeAtPosition(document, position, hitTolerance);
		if (!stroke)
		{
			HideAuthorTooltip();
			return;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		string authorName;
		if (playerManager)
			authorName = playerManager.GetPlayerName(stroke.m_iOwner);
		if (authorName.IsEmpty())
			authorName = WidgetManager.Translate("#LL-MapPaint_Player", stroke.m_iOwner.ToString());
		if (m_iAuthorTooltipOwner != stroke.m_iOwner || !m_wAuthorTooltip.IsVisible())
			m_wAuthorTooltipText.SetText(authorName);

		float tooltipWidth = FrameSlot.GetSizeX(m_wAuthorTooltip);
		float tooltipHeight = FrameSlot.GetSizeY(m_wAuthorTooltip);
		float frameWidth = workspace.DPIUnscale(canvasWidth);
		float frameHeight = workspace.DPIUnscale(canvasHeight);
		float localX = workspace.DPIUnscale(cursorX - canvasX + 14);
		float localY = workspace.DPIUnscale(cursorY - canvasY + 18);
		if (localX + tooltipWidth > frameWidth)
			localX = workspace.DPIUnscale(cursorX - canvasX - 10) - tooltipWidth;
		if (localY + tooltipHeight > frameHeight)
			localY = workspace.DPIUnscale(cursorY - canvasY - 10) - tooltipHeight;
		localX = Math.Max(0, Math.Min(localX, frameWidth - tooltipWidth));
		localY = Math.Max(0, Math.Min(localY, frameHeight - tooltipHeight));
		FrameSlot.SetPos(m_wAuthorTooltip, localX, localY);
		m_iAuthorTooltipOwner = stroke.m_iOwner;
		m_wAuthorTooltip.SetVisible(true);
		UpdateBackground();
	}
}
