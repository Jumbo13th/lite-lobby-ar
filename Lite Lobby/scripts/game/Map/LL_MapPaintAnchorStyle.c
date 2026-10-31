//! Width at placement, map-space conversion and color policy for anchor chains.
class LL_MapPaintAnchorStyle
{
	static const float WIDTH = 16;
	static const float MIN_WIDTH = 16;
	static const float MAX_WIDTH = 24;
	static const float REFERENCE_ZOOM = 0.25;
	static const int ALPHA = 128;

	static float GetScreenWidth(float zoom)
	{
		float width = WIDTH * REFERENCE_ZOOM / Math.Max(zoom, 0.0001);
		return Math.Clamp(width, MIN_WIDTH, MAX_WIDTH);
	}

	static float GetWorldWidth(float zoom)
	{
		return GetScreenWidth(zoom) / Math.Max(zoom, 0.0001);
	}

	static int GetColor(int color)
	{
		return (color & 0x00ffffff) | (ALPHA << 24);
	}
}
