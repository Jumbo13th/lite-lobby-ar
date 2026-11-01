//! Rounded square with record lines, drawn in the stock menu icon's current tint.
class LL_MapPaintHistoryIcon
{
	protected SCR_MapToolEntry m_Entry;
	protected Widget m_Button;
	protected ImageWidget m_SourceImage;
	protected CanvasWidget m_Canvas;
	protected ref array<ref CanvasWidgetCommand> m_Commands = {};
	protected float m_fScale;
	protected float m_fLeft;
	protected float m_fTop;
	protected int m_iColor = 0xffffffff;

	void LL_MapPaintHistoryIcon(SCR_MapToolEntry entry)
	{
		m_Entry = entry;
	}

	void ~LL_MapPaintHistoryIcon()
	{
		Close();
	}

	void Close()
	{
		if (m_Canvas)
			m_Canvas.RemoveFromHierarchy();
		if (m_SourceImage)
			m_SourceImage.SetVisible(true);
		m_Canvas = null;
		m_SourceImage = null;
		m_Button = null;
		m_Commands.Clear();
	}

	void Update(WorkspaceWidget workspace)
	{
		if (!m_Entry || !m_Entry.m_ButtonComp)
			return;
		Widget button = m_Entry.m_ButtonComp.GetRootWidget();
		if ((m_Button != button || !m_Canvas) && !AttachCanvas(workspace, button))
			return;
		UpdateColor(m_SourceImage.GetColorInt());
		m_SourceImage.SetVisible(false);
		bool changed = LL_MapPaintWidgets.ConfigurePixelCanvas(m_Canvas);
		if (changed || m_Commands.IsEmpty())
			DrawIcon();
	}

	protected bool AttachCanvas(WorkspaceWidget workspace, Widget button)
	{
		Close();
		m_SourceImage = m_Entry.m_ButtonComp.GetImageWidget();
		if (!button || !m_SourceImage)
			return false;
		// The stock button accepts only its existing size-layout child. Add the
		// icon to the overlay that also owns the stock image's scale widget.
		Widget parent = m_SourceImage.GetParent();
		while (parent && !OverlayWidget.Cast(parent))
			parent = parent.GetParent();
		if (!parent)
			return false;
		WidgetFlags flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING;
		m_Canvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, flags, Color.White, 40, parent));
		if (!m_Canvas)
			return false;
		m_Canvas.SetName("LL_MapPaintHistoryIcon");
		OverlaySlot.SetHorizontalAlign(m_Canvas, LayoutHorizontalAlign.Stretch);
		OverlaySlot.SetVerticalAlign(m_Canvas, LayoutVerticalAlign.Stretch);
		m_Button = button;
		m_Canvas.SetIsColorInherited(false);
		return true;
	}

	protected void DrawIcon()
	{
		float width, height;
		m_Canvas.GetScreenSize(width, height);
		if (width <= 0 || height <= 0)
			return;

		float size = Math.Min(width, height) * 0.72;
		m_fScale = size / 24;
		m_fLeft = (width - size) * 0.5;
		m_fTop = (height - size) * 0.5;
		m_Commands.Clear();
		array<float> outline = {};
		array<vector> corners = {Vector(5, 5, 0), Vector(19, 5, 0), Vector(19, 19, 0), Vector(5, 19, 0)};
		foreach (int corner, vector center : corners)
		{
			for (int i = 0; i <= 6; i++)
			{
				float angle = (180 + corner * 90 + i * 15) * Math.DEG2RAD;
				outline.Insert(center[0] + 2.5 * Math.Cos(angle));
				outline.Insert(center[1] + 2.5 * Math.Sin(angle));
			}
		}
		outline.Insert(outline[0]);
		outline.Insert(outline[1]);
		AddLine(outline);
		for (int row = 0; row < 3; row++)
		{
			float y = 7.5 + row * 4.5;
			float right = 17;
			if (row == 2)
				right = 14;
			array<float> recordLine = {7, y, right, y};
			AddLine(recordLine);
			AddRoundedPoint(7, y);
			AddRoundedPoint(right, y);
		}
		m_Canvas.SetDrawCommands(m_Commands);
	}

	protected void AddLine(array<float> points)
	{
		array<float> vertices = {};
		for (int i = 0; i + 1 < points.Count(); i += 2)
		{
			vertices.Insert(m_fLeft + points[i] * m_fScale);
			vertices.Insert(m_fTop + points[i + 1] * m_fScale);
		}
		LL_MapPaintCanvas.AddLines(vertices, 1.5 * m_fScale, m_iColor, m_Commands);
	}

	protected void AddRoundedPoint(float x, float y)
	{
		PolygonDrawCommand cap = new PolygonDrawCommand();
		cap.m_iColor = m_iColor;
		cap.m_Vertices = {};
		m_Canvas.TessellateCircle(Vector(m_fLeft + x * m_fScale, m_fTop + y * m_fScale, 0), 0.75 * m_fScale, 12, cap.m_Vertices);
		m_Commands.Insert(cap);
	}

	protected void UpdateColor(int color)
	{
		if (color == m_iColor)
			return;
		m_iColor = color;
		foreach (CanvasWidgetCommand command : m_Commands)
		{
			LineDrawCommand line = LineDrawCommand.Cast(command);
			if (line)
				line.m_iColor = color;
			PolygonDrawCommand cap = PolygonDrawCommand.Cast(command);
			if (cap)
				cap.m_iColor = color;
		}
		m_Canvas.SetDrawCommands(m_Commands);
	}
}
