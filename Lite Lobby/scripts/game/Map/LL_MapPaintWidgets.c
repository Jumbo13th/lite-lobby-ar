//! Shared widget construction, screen bounds and input hit testing.
class LL_MapPaintWidgets
{
	//! Window positions and FrameSlot dimensions use DPI-unscaled UI coordinates.
	static vector GetScreenSizeInUIUnits(WorkspaceWidget workspace)
	{
		float width, height;
		workspace.GetScreenSize(width, height);
		return Vector(workspace.DPIUnscale(width), workspace.DPIUnscale(height), 0);
	}

	static bool IsMapMenuVertical(Widget button)
	{
		if (!button || !button.GetParent())
			return false;
		float width, height;
		button.GetParent().GetScreenSize(width, height);
		return height > width;
	}

	//! Choose the roomier side perpendicular to the stock menu's orientation.
	static void PositionPanelBesideMenu(WorkspaceWidget workspace, Widget window, Widget button)
	{
		if (!window || !button || !button.GetParent())
			return;
		float x, y, buttonWidth, buttonHeight;
		button.GetScreenPos(x, y);
		button.GetScreenSize(buttonWidth, buttonHeight);
		x = workspace.DPIUnscale(x);
		y = workspace.DPIUnscale(y);
		buttonWidth = workspace.DPIUnscale(buttonWidth);
		buttonHeight = workspace.DPIUnscale(buttonHeight);
		vector screen = GetScreenSizeInUIUnits(workspace);
		float left = x;
		float top = y;
		float gap = LL_MapPaintSettings.PAINT_PANEL_GAP;
		if (IsMapMenuVertical(button))
		{
			if (x >= screen[0] - x - buttonWidth)
				left = x - FrameSlot.GetSizeX(window) - gap;
			else
				left = x + buttonWidth + gap;
		}
		else
		{
			if (y >= screen[1] - y - buttonHeight)
				top = y - FrameSlot.GetSizeY(window) - gap;
			else
				top = y + buttonHeight + gap;
		}
		PositionWindow(workspace, window, left, top);
	}

	static void PositionWindow(WorkspaceWidget workspace, Widget window, float left, float top, float margin = LL_MapPaintSettings.PAINT_PANEL_MARGIN)
	{
		vector screen = GetScreenSizeInUIUnits(workspace);
		left = Math.Max(margin, Math.Min(left, screen[0] - FrameSlot.GetSizeX(window) - margin));
		top = Math.Max(margin, Math.Min(top, screen[1] - FrameSlot.GetSizeY(window) - margin));
		FrameSlot.SetAnchorMin(window, 0, 0);
		FrameSlot.SetAnchorMax(window, 0, 0);
		FrameSlot.SetPos(window, left, top);
	}

	static Widget CreatePanelWidget(WorkspaceWidget workspace, Widget parent, WidgetType type, string name, int userId, float left, float top, float right, float bottom, int color, bool ignoreCursor = false)
	{
		WidgetFlags flags = WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.NOFOCUS | WidgetFlags.INHERIT_CLIPPING;
		if (ignoreCursor)
			flags = flags | WidgetFlags.IGNORE_CURSOR;
		Widget widget = workspace.CreateWidget(type, flags, Color.FromInt(color), 30, parent);
		if (!widget)
			return null;

		widget.SetName(name);
		widget.SetIsColorInherited(false);
		UIWidget uiWidget = UIWidget.Cast(widget);
		if (uiWidget)
		{
			uiWidget.SetDefaultStyle();
			widget.SetColorInt(color);
		}
		if (userId)
			widget.SetUserID(userId);
		FrameSlot.SetAnchorMin(widget, 0, 0);
		FrameSlot.SetAnchorMax(widget, 0, 0);
		FrameSlot.SetSizeToContent(widget, false);
		FrameSlot.SetPos(widget, left, top);
		FrameSlot.SetSize(widget, right - left, bottom - top);
		return widget;
	}

	static TextWidget CreatePanelText(WorkspaceWidget workspace, Widget parent, string name, float left, float top, float right, float bottom, string text, int color = 0xfff0f3f5)
	{
		TextWidget label = TextWidget.Cast(CreatePanelWidget(workspace, parent, WidgetType.TextWidgetTypeID, name, 0, left, top, right, bottom, color, true));
		if (label)
		{
			label.SetFont("{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt");
			label.SetExactFontSize(16);
			label.SetText(text);
		}
		return label;
	}

	static FrameWidget CreateMapPanelRoot(WorkspaceWidget workspace, string name, float leftOffset, float top, float width, float height)
	{
		// A window attached to the workspace can render above sibling game menus.
		// A child of the map menu cannot escape its parent's stacking layer.
		WidgetFlags flags = WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.NOFOCUS;
		FrameWidget root = FrameWidget.Cast(workspace.CreateWidgetInWorkspace(WidgetType.FrameWidgetTypeID, 0, 0, 0, 0, flags, Color.White, LL_MapPaintSettings.PAINT_UI_TOPMOST_Z_ORDER));
		if (!root)
			return null;
		root.SetName(name);
		root.SetIsColorInherited(false);
		FrameSlot.SetAnchorMin(root, 1, 1);
		FrameSlot.SetAnchorMax(root, 1, 1);
		FrameSlot.SetSizeToContent(root, false);
		FrameSlot.SetPos(root, leftOffset, top);
		FrameSlot.SetSize(root, width, height);
		return root;
	}

	static CanvasWidget CreatePaintCanvas(WorkspaceWidget workspace, Widget parent, string name, int zOrder)
	{
		WidgetFlags flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING;
		CanvasWidget canvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, flags, Color.White, zOrder, parent));
		if (!canvas)
			return null;

		canvas.SetName(name);
		FrameSlot.SetAnchorMin(canvas, 0, 0);
		FrameSlot.SetAnchorMax(canvas, 1, 1);
		FrameSlot.SetOffsets(canvas, 0, 0, 0, 0);
		return canvas;
	}

	//! Draw commands are already projected to physical pixels, not virtual map units.
	static bool ConfigurePixelCanvas(CanvasWidget canvas)
	{
		if (!canvas)
			return false;
		float width, height;
		canvas.GetScreenSize(width, height);
		if (width <= 0 || height <= 0)
			return false;
		bool changed;
		vector units = canvas.GetSizeInUnits();
		if (Math.AbsFloat(units[0] - width) > 0.01 || Math.AbsFloat(units[1] - height) > 0.01)
		{
			canvas.SetSizeInUnits(Vector(width, height, 0));
			changed = true;
		}
		if (Math.AbsFloat(canvas.GetZoom() - 1) > 0.0001)
		{
			canvas.SetZoom(1);
			changed = true;
		}
		if (canvas.GetOffsetPx() != vector.Zero)
		{
			canvas.SetOffsetPx(vector.Zero);
			changed = true;
		}
		return changed;
	}

	static bool IsPointInsideWidget(Widget widget, float x, float y)
	{
		if (!widget || !widget.IsVisibleInHierarchy())
			return false;

		float left, top, width, height;
		widget.GetScreenPos(left, top);
		widget.GetScreenSize(width, height);
		return width > 0 && height > 0 && x >= left && y >= top && x < left + width && y < top + height;
	}

	//! Ancestors must be checked because cursor hits often name a child label or icon.
	static bool HasBlockingControl(bool anyButton = false)
	{
		foreach (Widget widget : SCR_MapCursorModule.GetMapWidgetsUnderCursor())
		{
			Widget ancestor = widget;
			while (ancestor)
			{
				if ((anyButton && ButtonWidget.Cast(ancestor)) || XComboBoxWidget.Cast(ancestor) || EditBoxWidget.Cast(ancestor) || SliderWidget.Cast(ancestor))
					return true;
				ancestor = ancestor.GetParent();
			}
		}
		return false;
	}
}
