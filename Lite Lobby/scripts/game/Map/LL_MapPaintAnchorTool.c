//! Maintains an uncommitted start point and advances the chain after successful submission.
class LL_MapPaintAnchorTool
{
	protected bool m_bPressed;
	protected bool m_bHasAnchor;
	protected bool m_bContinuation;
	protected vector m_AnchorPosition;

	void Reset()
	{
		m_bPressed = false;
		m_bHasAnchor = false;
		m_bContinuation = false;
	}

	void BeginPress()
	{
		m_bPressed = true;
	}

	bool ConsumePress()
	{
		bool pressed = m_bPressed;
		m_bPressed = false;
		return pressed;
	}

	LL_MapPaintStroke CreateSegment(vector position, float zoom, int color, int documentPoints)
	{
		if (!m_bHasAnchor)
		{
			m_AnchorPosition = position;
			m_bHasAnchor = true;
			return null;
		}
		int requiredPoints = 2;
		if (m_bContinuation)
			requiredPoints = 1;
		if (documentPoints + requiredPoints > LL_MapPaintDocument.MAX_POINTS)
			return null;
		LL_MapPaintStroke stroke = new LL_MapPaintStroke();
		stroke.m_fWidth = LL_MapPaintAnchorStyle.GetWorldWidth(zoom);
		stroke.m_bAnchorLine = true;
		stroke.m_iColor = LL_MapPaintAnchorStyle.GetColor(color);
		stroke.m_Points.Insert(m_AnchorPosition);
		stroke.m_Points.Insert(position);
		return stroke;
	}

	int GetOperationKind()
	{
		if (m_bContinuation)
			return LL_EMapPaintOperation.ANCHOR_CONTINUE;
		return LL_EMapPaintOperation.ANCHOR;
	}

	void Commit(vector position)
	{
		m_AnchorPosition = position;
		m_bContinuation = true;
	}
}
