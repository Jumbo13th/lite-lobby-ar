//! Routes toolbar and color-picker input to the active paint UI.
class LL_MapPaintPanelHandler : ScriptedWidgetEventHandler
{
	protected LL_MapPaintUI m_Owner;
	protected LL_MapPaintPanel m_Panel;

	void LL_MapPaintPanelHandler(LL_MapPaintUI owner, LL_MapPaintPanel panel)
	{
		m_Owner = owner;
		m_Panel = panel;
	}

	Widget LL_GetPanelWidget()
	{
		if (!m_Panel)
			return null;
		return m_Panel.LL_GetPanelWidget();
	}

	bool LL_HandleDropdownClickAt(float x, float y)
	{
		if (!m_Panel)
			return false;
		return m_Panel.LL_HandleDropdownClickAt(x, y);
	}

#ifdef LL_MAP_PAINT_SMOKE
	bool LL_TestStartGesture()
	{
		if (!m_Owner)
			return false;
		return m_Owner.LL_TestStartGesture();
	}

	bool LL_TestGestureActive()
	{
		return m_Owner && m_Owner.LL_TestGestureActive();
	}

	void LL_TestCancelGesture()
	{
		if (m_Owner)
			m_Owner.LL_TestCancelGesture();
	}
#endif

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Owner || !m_Panel || button != 0)
			return false;

		if (w.GetUserID() == LL_MapPaintSettings.COLOR_BUTTON_ID)
			m_Panel.LL_ToggleColorPicker();
		else if (w.GetUserID() >= LL_MapPaintSettings.COLOR_PRESET_FIRST_ID && w.GetUserID() <= LL_MapPaintSettings.COLOR_PRESET_LAST_ID)
			m_Panel.LL_SelectBrushColor(w.GetUserID());
		else
			return m_Panel.LL_SelectTool(w);

		return true;
	}

	override bool OnChange(Widget w, bool finished)
	{
		if (!m_Panel)
			return false;

		return m_Panel.LL_OnColorPickerChanged(w, finished);
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Panel)
			m_Panel.LL_SetWidgetHovered(w, true);

		return false;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Panel)
			m_Panel.LL_SetWidgetHovered(w, false);

		return false;
	}

}

//! Keeps each toolbar button, its mode and its icon together in display order.
class LL_MapPaintPanelTool
{
	ButtonWidget m_Button;
	LL_EMapPaintMode m_Mode;
	Widget m_Selection;
	ref LL_MapPaintToolIcon m_Icon;
}

//! Owns toolbar, color picker, their input state and all workspace windows.
class LL_MapPaintPanel
{
	protected LL_MapPaintUI m_Owner;
	protected LL_MapPaintToolState m_ToolState;
	protected SCR_MapToolEntry m_MenuEntry;
	protected bool m_bOpen;
	protected bool m_bToolsOpen;
	protected bool m_bVerticalTools;
	protected bool m_bColorPickerOpen;
	protected bool m_bColorPickerAtCursor;
	protected bool m_bIgnoreColorPickerRelease;
	protected int m_iBrushColor = LL_MapPaintSettings.BRUSH_COLOR;
	protected int m_iPickerRed = 255;
	protected int m_iPickerGreen = 0;
	protected int m_iPickerBlue = 0;
	protected FrameWidget m_wPaintPanel;
	protected Widget m_wPanelBackground;
	protected Widget m_wHoveredTool;
	protected FrameWidget m_wToolHint;
	protected TextWidget m_wToolHintText;
	protected ref array<ref LL_MapPaintPanelTool> m_Tools = {};
	protected ButtonWidget m_wColorButton;
	protected FrameWidget m_wColorPicker;
	protected ref array<ButtonWidget> m_ColorPresetButtons = {};
	protected SliderWidget m_wColorRedSlider;
	protected SliderWidget m_wColorGreenSlider;
	protected SliderWidget m_wColorBlueSlider;
	protected TextWidget m_wColorRedValue;
	protected TextWidget m_wColorGreenValue;
	protected TextWidget m_wColorBlueValue;
	protected ref LL_MapPaintPanelHandler m_PanelHandler;

	void LL_MapPaintPanel(LL_MapPaintUI owner, LL_MapPaintToolState toolState, SCR_MapToolEntry menuEntry)
	{
		m_Owner = owner;
		m_ToolState = toolState;
		m_MenuEntry = menuEntry;
	}

	void ~LL_MapPaintPanel()
	{
		Close();
	}

	void Close()
	{
		m_bOpen = false;
		m_bToolsOpen = false;
		m_bVerticalTools = false;
		m_bColorPickerOpen = false;
		m_bColorPickerAtCursor = false;
		m_bIgnoreColorPickerRelease = false;
		if (m_MenuEntry)
		{
			m_MenuEntry.SetActive(false);
			m_MenuEntry.SetVisible(false);
		}
		m_Tools.Clear();
		if (m_wPaintPanel)
			m_wPaintPanel.RemoveFromHierarchy();
		if (m_wColorPicker)
			m_wColorPicker.RemoveFromHierarchy();
		m_wPaintPanel = null;
		m_wPanelBackground = null;
		m_wHoveredTool = null;
		if (m_wToolHint)
			m_wToolHint.RemoveFromHierarchy();
		m_wToolHint = null;
		m_wToolHintText = null;
		m_wColorButton = null;
		m_wColorPicker = null;
		m_ColorPresetButtons.Clear();
		m_wColorRedSlider = null;
		m_wColorGreenSlider = null;
		m_wColorBlueSlider = null;
		m_wColorRedValue = null;
		m_wColorGreenValue = null;
		m_wColorBlueValue = null;
		m_PanelHandler = null;
	}

	void Open(WorkspaceWidget workspace)
	{
		m_PanelHandler = new LL_MapPaintPanelHandler(m_Owner, this);
		m_wPaintPanel = LL_MapPaintWidgets.CreateMapPanelRoot(workspace, "LL_MapPaintToolbar", -(LL_MapPaintSettings.PAINT_PANEL_MARGIN + LL_MapPaintSettings.PAINT_TOOLBAR_WIDTH), -(LL_MapPaintSettings.PAINT_PANEL_MARGIN + LL_MapPaintSettings.PAINT_TOOLBAR_HEIGHT), LL_MapPaintSettings.PAINT_TOOLBAR_WIDTH, LL_MapPaintSettings.PAINT_TOOLBAR_HEIGHT);
		if (!m_wPaintPanel)
			return;
		m_wPaintPanel.SetVisible(false);
		m_wPanelBackground = LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wPaintPanel, WidgetType.PanelWidgetTypeID, "LL_MapPaintPanelBackground", 0, 0, 0, LL_MapPaintSettings.PAINT_TOOLBAR_WIDTH, LL_MapPaintSettings.PAINT_TOOLBAR_HEIGHT, LL_MapPaintSettings.PANEL_COLOR, true);
		CreateDrawingTool(workspace, "Brush", LL_MapPaintSettings.TOOL_BRUSH_ID, LL_EMapPaintMode.BRUSH);
		CreateDrawingTool(workspace, "Line", LL_MapPaintSettings.TOOL_LINE_ID, LL_EMapPaintMode.LINE);
		CreateDrawingTool(workspace, "Circle", LL_MapPaintSettings.TOOL_CIRCLE_ID, LL_EMapPaintMode.CIRCLE);
		CreateDrawingTool(workspace, "Anchor", LL_MapPaintSettings.TOOL_ANCHOR_ID, LL_EMapPaintMode.ANCHOR);
		CreateDrawingTool(workspace, "Eraser", LL_MapPaintSettings.TOOL_ERASER_ID, LL_EMapPaintMode.ERASER);

		m_wColorButton = CreateToolButton(workspace, "LL_MapPaintColorButton", LL_MapPaintSettings.COLOR_BUTTON_ID, m_iBrushColor);
		ConfigureToolLayout(workspace, false, true);

		CreateColorPicker(workspace);
		m_wToolHint = LL_MapPaintWidgets.CreateMapPanelRoot(workspace, "LL_MapPaintToolHint", 0, 0, 260, 28);
		if (m_wToolHint)
		{
			LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wToolHint, WidgetType.PanelWidgetTypeID, "LL_MapPaintHintBackground", 0, 0, 0, 260, 28, LL_MapPaintSettings.PANEL_COLOR, true);
			m_wToolHintText = LL_MapPaintWidgets.CreatePanelText(workspace, m_wToolHint, "LL_MapPaintHintText", 8, 2, 252, 26, string.Empty);
			m_wToolHint.SetFlags(WidgetFlags.IGNORE_CURSOR);
			m_wToolHint.SetVisible(false);
		}
		m_bOpen = true;
	}

	void ToggleTools()
	{
		SetToolsOpen(!m_bToolsOpen);
	}

	void SetToolsOpen(bool open)
	{
		m_bToolsOpen = open && m_bOpen;
		if (!m_bToolsOpen && m_wToolHint)
			m_wToolHint.SetVisible(false);
		if (!m_bToolsOpen)
			SetColorPickerOpen(false);
		m_Owner.LL_OnPanelChanged();
	}

	void DismissTools()
	{
		if (m_bToolsOpen || m_bColorPickerOpen)
			SetToolsOpen(false);
	}

	protected void ConfigureToolLayout(WorkspaceWidget workspace, bool vertical, bool force = false)
	{
		if (!force && m_bVerticalTools == vertical)
			return;
		m_bVerticalTools = vertical;
		float width = LL_MapPaintSettings.PAINT_TOOLBAR_WIDTH;
		float height = LL_MapPaintSettings.PAINT_TOOLBAR_HEIGHT;
		if (vertical)
		{
			width = LL_MapPaintSettings.PAINT_TOOLBAR_HEIGHT;
			height = LL_MapPaintSettings.PAINT_TOOLBAR_WIDTH;
		}
		FrameSlot.SetSize(m_wPaintPanel, width, height);
		if (m_wPanelBackground)
			FrameSlot.SetSize(m_wPanelBackground, width, height);
		float padding = LL_MapPaintSettings.TOOLBAR_PADDING;
		float step = LL_MapPaintSettings.TOOL_BUTTON_SIZE + LL_MapPaintSettings.TOOL_BUTTON_GAP;
		for (int index = 0; index <= m_Tools.Count(); index++)
		{
			ButtonWidget toolButton = m_wColorButton;
			if (index < m_Tools.Count())
				toolButton = m_Tools[index].m_Button;
			if (!toolButton)
				continue;
			if (vertical)
				FrameSlot.SetPos(toolButton, padding, padding + index * step);
			else
				FrameSlot.SetPos(toolButton, padding + index * step, padding);
		}
		UpdateIcons(workspace);
	}

	protected Widget GetMenuButton()
	{
		if (!m_MenuEntry || !m_MenuEntry.m_ButtonComp)
			return null;
		return m_MenuEntry.m_ButtonComp.GetRootWidget();
	}

	//! Open on the side of the stock menu with the most screen space.
	protected void PositionTools(WorkspaceWidget workspace)
	{
		Widget button = GetMenuButton();
		if (!button || !button.GetParent())
			return;
		ConfigureToolLayout(workspace, LL_MapPaintWidgets.IsMapMenuVertical(button));
		LL_MapPaintWidgets.PositionPanelBesideMenu(workspace, m_wPaintPanel, button);
		if (!m_bColorPickerAtCursor)
			PositionColorPickerAtDefault();
	}

	protected void PositionWindow(WorkspaceWidget workspace, Widget window, float left, float top, float margin = LL_MapPaintSettings.PAINT_PANEL_MARGIN)
	{
		LL_MapPaintWidgets.PositionWindow(workspace, window, left, top, margin);
	}

	protected void CreateColorPicker(WorkspaceWidget workspace)
	{
		float pickerRightOffset = LL_MapPaintSettings.PAINT_PANEL_MARGIN + LL_MapPaintSettings.PAINT_COLOR_PICKER_WIDTH;
		float pickerBottomOffset = LL_MapPaintSettings.PAINT_PANEL_MARGIN + LL_MapPaintSettings.PAINT_TOOLBAR_HEIGHT + LL_MapPaintSettings.PAINT_PANEL_GAP + LL_MapPaintSettings.PAINT_COLOR_PICKER_HEIGHT;
		m_wColorPicker = LL_MapPaintWidgets.CreateMapPanelRoot(workspace, "LL_MapPaintColorPicker", -pickerRightOffset, -pickerBottomOffset, LL_MapPaintSettings.PAINT_COLOR_PICKER_WIDTH, LL_MapPaintSettings.PAINT_COLOR_PICKER_HEIGHT);
		if (!m_wColorPicker)
			return;
		m_wColorPicker.SetVisible(false);
		LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wColorPicker, WidgetType.PanelWidgetTypeID, "LL_MapPaintColorPickerBackground", 0, 0, 0, 260, 160, LL_MapPaintSettings.PANEL_COLOR, true);

		for (int i = 0; i < 6; i++)
			CreateColorPresetButton(workspace, LL_MapPaintSettings.COLOR_PRESET_FIRST_ID + i, 25 + i * 36, 8);

		CreateColorChannel(workspace, "Red", "R", 72, m_iPickerRed, m_wColorRedSlider, m_wColorRedValue);
		CreateColorChannel(workspace, "Green", "G", 100, m_iPickerGreen, m_wColorGreenSlider, m_wColorGreenValue);
		CreateColorChannel(workspace, "Blue", "B", 128, m_iPickerBlue, m_wColorBlueSlider, m_wColorBlueValue);

		UpdateColorPickerDisplay();
	}

	protected void CreateColorChannel(WorkspaceWidget workspace, string name, string label, float top, int value, out SliderWidget slider, out TextWidget valueLabel)
	{
		string prefix = "LL_MapPaintColor" + name;
		LL_MapPaintWidgets.CreatePanelText(workspace, m_wColorPicker, prefix + "Label", 10, top + 2, 26, top + 24, label);
		slider = CreateColorPickerSlider(workspace, prefix + "Slider", 28, top, 210, top + 24, value);
		valueLabel = LL_MapPaintWidgets.CreatePanelText(workspace, m_wColorPicker, prefix + "Value", 216, top + 2, 250, top + 24, value.ToString());
	}

	protected ButtonWidget CreateToolButton(WorkspaceWidget workspace, string name, int userId, int color = LL_MapPaintSettings.CONTROL_COLOR)
	{
		float size = LL_MapPaintSettings.TOOL_BUTTON_SIZE;
		ButtonWidget button = ButtonWidget.Cast(LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wPaintPanel, WidgetType.ButtonWidgetTypeID, name, userId, 0, 0, size, size, color));
		if (button)
			button.AddHandler(m_PanelHandler);
		return button;
	}

	protected void CreateColorPresetButton(WorkspaceWidget workspace, int userId, float left, float top)
	{
		ButtonWidget preset = ButtonWidget.Cast(LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wColorPicker, WidgetType.ButtonWidgetTypeID, "LL_MapPaintColorPreset" + userId.ToString(), userId, left, top, left + 30, top + 30, GetPresetColor(userId)));
		if (!preset)
			return;
		preset.AddHandler(m_PanelHandler);
		m_ColorPresetButtons.Insert(preset);
	}

	protected int GetPresetColor(int userId)
	{
		switch (userId)
		{
			case 11: return 0xff0000ff;
			case 12: return 0xff00ff00;
			case 13: return 0xffffff00;
			case 14: return 0xff000000;
			case 15: return 0xffffffff;
		}
		return LL_MapPaintSettings.BRUSH_COLOR;
	}

	protected SliderWidget CreateColorPickerSlider(WorkspaceWidget workspace, string name, float left, float top, float right, float bottom, int value)
	{
		SliderWidget slider = SliderWidget.Cast(LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wColorPicker, WidgetType.SliderWidgetTypeID, name, 0, left, top, right, bottom, 0xffffffff));
		if (!slider)
			return null;

		slider.SetRange(0, 255);
		slider.SetStep(1);
		slider.SetCurrent(value);
		slider.AddHandler(m_PanelHandler);
		slider.SetEnabled(false);
		return slider;
	}

	protected void CreateDrawingTool(WorkspaceWidget workspace, string name, int userId, LL_EMapPaintMode mode)
	{
		LL_MapPaintPanelTool tool = new LL_MapPaintPanelTool();
		tool.m_Mode = mode;
		tool.m_Button = CreateToolButton(workspace, "LL_MapPaint" + name + "Button", userId);
		if (tool.m_Button)
		{
			tool.m_Selection = LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wPaintPanel, WidgetType.PanelWidgetTypeID, "LL_MapPaintSelection", 0, 4, 38, 38, 41, LL_MapPaintSettings.ACCENT_COLOR, true);
			if (tool.m_Selection)
				tool.m_Selection.SetVisible(false);
		}
		tool.m_Icon = new LL_MapPaintToolIcon(workspace, m_wPaintPanel, tool.m_Button, "LL_MapPaint" + name + "Icon", mode);
		m_Tools.Insert(tool);
	}

	protected LL_MapPaintPanelTool FindDrawingTool(Widget widget)
	{
		foreach (LL_MapPaintPanelTool tool : m_Tools)
		{
			if (tool.m_Button == widget)
				return tool;
		}
		return null;
	}

	bool LL_SelectTool(Widget widget)
	{
		LL_MapPaintPanelTool tool = FindDrawingTool(widget);
		if (!tool)
			return false;
		m_Owner.LL_SelectTool(tool.m_Mode);
		return true;
	}

	void UpdateIcons(WorkspaceWidget workspace)
	{
		foreach (LL_MapPaintPanelTool tool : m_Tools)
		{
			tool.m_Icon.Update(workspace);
			if (tool.m_Selection && tool.m_Button)
			{
				vector position = FrameSlot.GetPos(tool.m_Button);
				FrameSlot.SetPos(tool.m_Selection, position[0] + 4, position[1] + 38);
			}
		}
	}

	void Refresh(bool allowed)
	{
		if (m_MenuEntry)
		{
			// Keep the stock menu layout stable while drawing permissions change.
			m_MenuEntry.SetVisible(m_bOpen);
			m_MenuEntry.SetEnabled(allowed);
			m_MenuEntry.SetActive(allowed && m_bToolsOpen);
			if (m_MenuEntry.m_ButtonComp)
				m_MenuEntry.m_ButtonComp.SetEnabled(allowed, false);
		}
		if ((!allowed || !m_bToolsOpen) && m_wToolHint)
			m_wToolHint.SetVisible(false);
		if (!m_wPaintPanel)
			return;

		PositionTools(GetGame().GetWorkspace());
		m_wPaintPanel.SetZOrder(LL_MapPaintSettings.PAINT_UI_TOPMOST_Z_ORDER);
		m_wPaintPanel.SetVisible(allowed && m_bToolsOpen);
		if (m_wColorPicker)
		{
			m_wColorPicker.SetZOrder(LL_MapPaintSettings.PAINT_UI_TOPMOST_Z_ORDER + 1);
			m_wColorPicker.SetVisible(allowed && m_bColorPickerOpen);
		}
		foreach (LL_MapPaintPanelTool tool : m_Tools)
		{
			if (tool.m_Button)
				tool.m_Button.SetEnabled(allowed);
		}
		if (m_wColorButton)
		{
			m_wColorButton.SetEnabled(allowed && (!m_ToolState.m_bEnabled || m_ToolState.m_SelectedMode != LL_EMapPaintMode.ERASER));
			m_wColorButton.SetColorInt(m_iBrushColor);
		}
		UpdateToolButtonColors();
	}

	protected void UpdateToolButtonColors()
	{
		foreach (LL_MapPaintPanelTool tool : m_Tools)
		{
			UpdateToolButton(tool.m_Button, tool.m_Mode);
			if (tool.m_Selection)
				tool.m_Selection.SetVisible(m_ToolState.IsSelected(tool.m_Mode));
			if (tool.m_Button)
				tool.m_Icon.LL_SetEnabled(tool.m_Button.IsEnabled());
		}
	}

	protected void UpdateToolButton(ButtonWidget button, LL_EMapPaintMode mode)
	{
		if (!button)
			return;
		int color = LL_MapPaintSettings.CONTROL_COLOR;
		if (m_ToolState.IsSelected(mode))
			color = LL_MapPaintSettings.SELECTED_COLOR;
		else if (button == m_wHoveredTool && button.IsEnabled())
			color = LL_MapPaintSettings.HOVER_COLOR;
		if (!button.IsEnabled())
			color = LL_MapPaintSettings.CONTROL_COLOR;
		button.SetColorInt(color);
	}

	int GetBrushColor()
	{
		return m_iBrushColor;
	}

	void LL_SelectBrushColor(int userId)
	{
		if (!m_bOpen || userId < LL_MapPaintSettings.COLOR_PRESET_FIRST_ID || userId > LL_MapPaintSettings.COLOR_PRESET_LAST_ID)
			return;

		m_iBrushColor = GetPresetColor(userId);
		LoadColorPickerChannels();
		SetColorPickerOpen(false);
		m_Owner.LL_OnPanelChanged();
	}

	protected void LoadColorPickerChannels()
	{
		int alpha;
		Color.UnpackInt(m_iBrushColor, alpha, m_iPickerRed, m_iPickerGreen, m_iPickerBlue);
		if (m_wColorRedSlider)
			m_wColorRedSlider.SetCurrent(m_iPickerRed);
		if (m_wColorGreenSlider)
			m_wColorGreenSlider.SetCurrent(m_iPickerGreen);
		if (m_wColorBlueSlider)
			m_wColorBlueSlider.SetCurrent(m_iPickerBlue);
		UpdateColorPickerDisplay();
	}

	protected void UpdateColorPickerDisplay()
	{
		if (m_wColorRedValue)
			m_wColorRedValue.SetText(m_iPickerRed.ToString());
		if (m_wColorGreenValue)
			m_wColorGreenValue.SetText(m_iPickerGreen.ToString());
		if (m_wColorBlueValue)
			m_wColorBlueValue.SetText(m_iPickerBlue.ToString());
	}

	bool LL_OnColorPickerChanged(Widget widget, bool finished)
	{
		SliderWidget slider = SliderWidget.Cast(widget);
		if (!slider)
			return false;

		int value = Math.ClampInt((int)Math.Round(slider.GetCurrent()), 0, 255);
		if (slider == m_wColorRedSlider)
			m_iPickerRed = value;
		else if (slider == m_wColorGreenSlider)
			m_iPickerGreen = value;
		else if (slider == m_wColorBlueSlider)
			m_iPickerBlue = value;
		else
			return false;

		m_iBrushColor = Color.FromRGBA(m_iPickerRed, m_iPickerGreen, m_iPickerBlue, 255).PackToInt();
		UpdateColorPickerDisplay();
		m_Owner.LL_OnPanelChanged();
		return true;
	}

	Widget LL_GetPanelWidget()
	{
		return m_wPaintPanel;
	}

	protected string LL_GetToolHint(LL_EMapPaintMode mode)
	{
		switch (mode)
		{
			case LL_EMapPaintMode.LINE: return "#LL-MapPaint_Line";
			case LL_EMapPaintMode.CIRCLE: return "#LL-MapPaint_Circle";
			case LL_EMapPaintMode.ANCHOR: return "#LL-MapPaint_Anchor";
			case LL_EMapPaintMode.ERASER: return "#LL-MapPaint_Eraser";
		}
		return "#LL-MapPaint_Brush";
	}

	void LL_SetWidgetHovered(Widget widget, bool hovered)
	{
		ButtonWidget button = ButtonWidget.Cast(widget);
		if (!button)
			return;

		if (hovered && !button.IsEnabled())
			return;
		if (hovered)
			m_wHoveredTool = widget;
		else if (m_wHoveredTool == widget)
			m_wHoveredTool = null;
		if (m_wToolHint && m_wToolHintText)
		{
			m_wToolHint.SetVisible(hovered);
			if (hovered)
			{
				string key = "#LL-MapPaint_Color";
				LL_MapPaintPanelTool tool = FindDrawingTool(widget);
				if (tool)
					key = LL_GetToolHint(tool.m_Mode);
				m_wToolHintText.SetText(WidgetManager.Translate(key));
				LL_MapPaintWidgets.PositionPanelBesideMenu(GetGame().GetWorkspace(), m_wToolHint, widget);
			}
		}

		if (!hovered && FindDrawingTool(widget))
		{
			UpdateToolButtonColors();
			return;
		}
		foreach (ButtonWidget preset : m_ColorPresetButtons)
		{
			if (widget == preset)
			{
				preset.SetColorInt(GetPresetColor(preset.GetUserID()));
				return;
			}
		}
		if (widget == m_wColorButton)
		{
			button.SetColorInt(m_iBrushColor);
			return;
		}

		if (hovered)
			button.SetColorInt(LL_MapPaintSettings.HOVER_COLOR);
		else
			button.SetColorInt(LL_MapPaintSettings.CONTROL_COLOR);
	}

	bool LL_HandleDropdownClickAt(float x, float y)
	{
		if (!m_bOpen || !m_bColorPickerOpen)
			return false;

		foreach (ButtonWidget preset : m_ColorPresetButtons)
		{
			if (LL_MapPaintWidgets.IsPointInsideWidget(preset, x, y))
			{
				LL_SelectBrushColor(preset.GetUserID());
				return true;
			}
		}
		if (LL_MapPaintWidgets.IsPointInsideWidget(m_wColorButton, x, y) || LL_MapPaintWidgets.IsPointInsideWidget(m_wColorPicker, x, y))
			return true;

		SetColorPickerOpen(false);
		return true;
	}

	void LL_ToggleColorPicker()
	{
		if (!m_wColorPicker)
			return;

		m_bColorPickerAtCursor = false;
		PositionColorPickerAtDefault();
		SetColorPickerOpen(!m_bColorPickerOpen);
		m_bIgnoreColorPickerRelease = m_bColorPickerOpen;
	}

	void LL_ToggleColorPickerAtCursor()
	{
		if (!m_wColorPicker || !m_bOpen)
			return;

		if (m_bColorPickerOpen)
		{
			SetColorPickerOpen(false);
			return;
		}

		m_bColorPickerAtCursor = true;
		PositionColorPickerAtCursor();
		m_bIgnoreColorPickerRelease = false;
		SetColorPickerOpen(true);
	}

	protected void PositionColorPickerAtCursor()
	{
		if (!m_wColorPicker)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		vector screenSize = LL_MapPaintWidgets.GetScreenSizeInUIUnits(workspace);

		float pickerWidth = FrameSlot.GetSizeX(m_wColorPicker);
		float pickerHeight = FrameSlot.GetSizeY(m_wColorPicker);
		float cursorX = SCR_MapCursorInfo.x;
		float cursorY = SCR_MapCursorInfo.y;
		float left = cursorX + 12;
		float top = cursorY + 12;
		if (left + pickerWidth > screenSize[0])
			left = cursorX - pickerWidth - 12;
		if (top + pickerHeight > screenSize[1])
			top = cursorY - pickerHeight - 12;
		PositionWindow(workspace, m_wColorPicker, left, top, 0);
	}

	protected void PositionColorPickerAtDefault()
	{
		if (!m_wColorPicker || !m_wPaintPanel)
			return;

		vector position = FrameSlot.GetPos(m_wPaintPanel);
		float top = position[1] - LL_MapPaintSettings.PAINT_PANEL_GAP - LL_MapPaintSettings.PAINT_COLOR_PICKER_HEIGHT;
		if (top < 0)
			top = position[1] + FrameSlot.GetSizeY(m_wPaintPanel) + LL_MapPaintSettings.PAINT_PANEL_GAP;
		PositionWindow(GetGame().GetWorkspace(), m_wColorPicker, position[0], top);
	}

	void SetColorPickerOpen(bool open)
	{
		open = open && m_bOpen && m_wColorPicker;
		m_bColorPickerOpen = open;
		if (!open)
		{
			m_bColorPickerAtCursor = false;
			m_bIgnoreColorPickerRelease = false;
			PositionColorPickerAtDefault();
		}
		if (open)
			LoadColorPickerChannels();

		if (m_wColorPicker)
			m_wColorPicker.SetVisible(open);
		if (m_wColorRedSlider)
			m_wColorRedSlider.SetEnabled(open);
		if (m_wColorGreenSlider)
			m_wColorGreenSlider.SetEnabled(open);
		if (m_wColorBlueSlider)
			m_wColorBlueSlider.SetEnabled(open);
		foreach (ButtonWidget preset : m_ColorPresetButtons)
			preset.SetEnabled(open);
	}

	bool HandleDropdownInput()
	{
		if (!m_bColorPickerOpen)
			return false;
		if (m_bIgnoreColorPickerRelease)
			return true;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float cursorX = workspace.DPIScale(SCR_MapCursorInfo.x);
		float cursorY = workspace.DPIScale(SCR_MapCursorInfo.y);
		LL_HandleDropdownClickAt(cursorX, cursorY);
		return true;
	}

	bool IsColorPickerOpen()
	{
		return m_bColorPickerOpen;
	}

	void BeginPointerPress()
	{
		m_bIgnoreColorPickerRelease = false;
	}

	bool ContainsPoint(float x, float y)
	{
		Widget menuButton = GetMenuButton();
		if (menuButton && LL_MapPaintWidgets.IsPointInsideWidget(menuButton.GetParent(), x, y))
			return true;
		return LL_MapPaintWidgets.IsPointInsideWidget(m_wPaintPanel, x, y) || LL_MapPaintWidgets.IsPointInsideWidget(m_wColorPicker, x, y);
	}
}
