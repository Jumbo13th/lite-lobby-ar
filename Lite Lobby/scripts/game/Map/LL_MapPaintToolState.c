enum LL_EMapPaintMode
{
	NONE,
	BRUSH,
	ERASER,
	LINE,
	ANCHOR,
	CIRCLE
}

//! Stores the tool choice separately from whether drawing is currently enabled.
class LL_MapPaintToolState
{
	LL_EMapPaintMode m_SelectedMode;
	bool m_bEnabled;

	bool IsSelected(LL_EMapPaintMode mode)
	{
		return m_bEnabled && m_SelectedMode == mode;
	}

	void Select(LL_EMapPaintMode mode)
	{
		m_SelectedMode = mode;
		m_bEnabled = mode != LL_EMapPaintMode.NONE;
	}

	void Reset()
	{
		Select(LL_EMapPaintMode.NONE);
	}
}
