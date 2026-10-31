//! One toolbar icon owns its canvas commands and rebuilds only when display scale changes.
class LL_MapPaintToolIcon
{
	protected CanvasWidget m_Canvas;
	protected ButtonWidget m_Button;
	protected LL_EMapPaintMode m_Mode;
	protected float m_fToolIconScale = -1;
	protected ref array<ref CanvasWidgetCommand> m_Commands = {};

	void LL_MapPaintToolIcon(WorkspaceWidget workspace, Widget parent, ButtonWidget button, string name, LL_EMapPaintMode mode)
	{
		if (!parent || !button)
			return;
		m_Button = button;
		m_Mode = mode;
		WidgetFlags flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.BLEND;
		m_Canvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, flags, Color.White, 31, parent));
		if (!m_Canvas)
			return;
		m_Canvas.SetName(name);
		Update(workspace);
	}

	void ~LL_MapPaintToolIcon()
	{
		if (m_Canvas)
			m_Canvas.RemoveFromHierarchy();
	}

	void LL_SetEnabled(bool enabled)
	{
		if (m_Canvas)
		{
			if (enabled)
				m_Canvas.SetOpacity(1);
			else
				m_Canvas.SetOpacity(0.35);
		}
	}

	void Update(WorkspaceWidget workspace)
	{
		if (!m_Canvas || !m_Button)
			return;
		// Match the button's hit area; canvas commands use physical pixels.
		vector position = FrameSlot.GetPos(m_Button);
		vector size = FrameSlot.GetSize(m_Button);
		FrameSlot.SetPos(m_Canvas, position[0], position[1]);
		FrameSlot.SetSize(m_Canvas, size[0], size[1]);
		float scale = GetToolIconScale(workspace);
		if (Math.AbsFloat(scale - m_fToolIconScale) < 0.01)
			return;
		m_fToolIconScale = scale;
		m_Commands.Clear();
		switch (m_Mode)
		{
			case LL_EMapPaintMode.BRUSH: CreateBrushIcon(); break;
			case LL_EMapPaintMode.ERASER: CreateEraserIcon(); break;
			case LL_EMapPaintMode.LINE: CreateLineIcon(); break;
			case LL_EMapPaintMode.ANCHOR: CreateAnchorIcon(); break;
			case LL_EMapPaintMode.CIRCLE: CreateCircleIcon(); break;
		}
		m_Canvas.SetDrawCommands(m_Commands);
	}

	protected float GetToolIconScale(WorkspaceWidget workspace)
	{
		if (!workspace)
			return 1;

		// Canvas draw commands use physical pixels. Scale them from the 1920x1080
		// reference resolution instead of relying on the user DPI setting.
		float scaleX = workspace.GetWidth() / 1920.0;
		float scaleY = workspace.GetHeight() / 1080.0;
		return Math.Max(0.25, Math.Min(scaleX, scaleY));
	}

	protected void AddIconLine(array<ref CanvasWidgetCommand> commands, float x1, float y1, float x2, float y2, float width)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = 0xfff0f3f5;
		line.m_fWidth = width * m_fToolIconScale;
		line.m_Vertices = {};
		float iconInset = (LL_MapPaintSettings.TOOL_BUTTON_SIZE - 24) * 0.5;
		line.m_Vertices.Insert((iconInset + x1) * m_fToolIconScale);
		line.m_Vertices.Insert((iconInset + y1) * m_fToolIconScale);
		line.m_Vertices.Insert((iconInset + x2) * m_fToolIconScale);
		line.m_Vertices.Insert((iconInset + y2) * m_fToolIconScale);
		commands.Insert(line);
	}

	protected void CreateBrushIcon()
	{
		// Keep the pencil centered while using a slim shaft and a clear pointed nib.
		AddIconLine(m_Commands, 1.8, 22, 3.2, 17.3, 1.4);
		AddIconLine(m_Commands, 3.2, 17.3, 16.1, 4, 1.4);
		AddIconLine(m_Commands, 16.1, 4, 18.1, 2, 1.4);
		AddIconLine(m_Commands, 18.1, 2, 21.8, 5.7, 1.4);
		AddIconLine(m_Commands, 21.8, 5.7, 19.7, 7.8, 1.4);
		AddIconLine(m_Commands, 19.7, 7.8, 6.8, 20.7, 1.4);
		AddIconLine(m_Commands, 6.8, 20.7, 1.8, 22, 1.4);
		AddIconLine(m_Commands, 16.7, 3.4, 20.4, 7.1, 1.1);
		AddIconLine(m_Commands, 3.2, 17.3, 6.8, 20.7, 1.3);
		AddIconLine(m_Commands, 2.5, 19.4, 4.5, 21.3, 1);
		AddIconLine(m_Commands, 2.1, 21.2, 4.2, 21.8, 1);
	}

	protected void CreateEraserIcon()
	{
		AddIconLine(m_Commands, 4, 12, 12, 4, 2.5);
		AddIconLine(m_Commands, 12, 4, 20, 12, 2.5);
		AddIconLine(m_Commands, 20, 12, 12, 20, 2.5);
		AddIconLine(m_Commands, 12, 20, 4, 12, 2.5);
		AddIconLine(m_Commands, 8, 16, 16, 8, 1.5);
	}

	protected void CreateCircleIcon()
	{
		LineDrawCommand circle = new LineDrawCommand();
		circle.m_iColor = 0xfff0f3f5;
		circle.m_fWidth = 2 * m_fToolIconScale;
		circle.m_bShouldEnclose = true;
		circle.m_Vertices = {};
		float center = (LL_MapPaintSettings.TOOL_BUTTON_SIZE * 0.5) * m_fToolIconScale;
		m_Canvas.TessellateCircle(Vector(center, center, 0), 8 * m_fToolIconScale, 32, circle.m_Vertices);
		m_Commands.Insert(circle);
	}

	protected void CreateLineIcon()
	{
		AddIconLine(m_Commands, 4, 20, 20, 4, 3);
	}

	protected void AddIconDot(float x, float y)
	{
		PolygonDrawCommand dot = new PolygonDrawCommand();
		dot.m_iColor = 0xfff0f3f5;
		dot.m_Vertices = {};
		float iconInset = (LL_MapPaintSettings.TOOL_BUTTON_SIZE - 24) * 0.5;
		m_Canvas.TessellateCircle(Vector((iconInset + x) * m_fToolIconScale, (iconInset + y) * m_fToolIconScale, 0), 3 * m_fToolIconScale, 12, dot.m_Vertices);
		m_Commands.Insert(dot);
	}

	protected void CreateAnchorIcon()
	{
		AddIconLine(m_Commands, 5, 19, 19, 5, 2);
		AddIconDot(5, 19);
		AddIconDot(19, 5);
	}
}
