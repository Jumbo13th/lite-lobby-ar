//! Shared tool dimensions, control IDs and workspace layers.
class LL_MapPaintSettings
{
	static const int PANEL_COLOR = 0xf2020304;
	static const int CONTROL_COLOR = 0xff020304;
	static const int HOVER_COLOR = 0xff191919;
	static const int ACCENT_COLOR = 0xffc26414;
	static const int SELECTED_COLOR = 0xff6b3a12;
	static const float BRUSH_WIDTH = 4;
	static const float ERASER_RADIUS = 16;
	static const float SAMPLE_SPACING = 2;
	static const bool SPATIAL_GRID_DEBUG_ENABLED = false;
	static const int BRUSH_COLOR = 0xffff0000;
	static const int COLOR_BUTTON_ID = 2;
	static const int TOOL_BRUSH_ID = 5;
	static const int TOOL_ERASER_ID = 6;
	static const int TOOL_LINE_ID = 9;
	static const int TOOL_ANCHOR_ID = 16;
	static const int TOOL_CIRCLE_ID = 17;
	static const int COLOR_PRESET_FIRST_ID = 10;
	static const int COLOR_PRESET_LAST_ID = 15;
	// Reserve int.MAX for the game's cursor, above both paint windows.
	static const int PAINT_UI_TOPMOST_Z_ORDER = int.MAX - 2;
	static const float PAINT_PANEL_MARGIN = 16;
	static const float PAINT_PANEL_GAP = 12;
	static const float TOOL_BUTTON_SIZE = 42;
	static const float TOOL_BUTTON_GAP = 6;
	static const float TOOLBAR_PADDING = 8;
	static const float PAINT_TOOLBAR_WIDTH = 2 * TOOLBAR_PADDING + 6 * TOOL_BUTTON_SIZE + 5 * TOOL_BUTTON_GAP;
	static const float PAINT_TOOLBAR_HEIGHT = 2 * TOOLBAR_PADDING + TOOL_BUTTON_SIZE;
	static const float PAINT_COLOR_PICKER_WIDTH = 260;
	static const float PAINT_COLOR_PICKER_HEIGHT = 160;
}
