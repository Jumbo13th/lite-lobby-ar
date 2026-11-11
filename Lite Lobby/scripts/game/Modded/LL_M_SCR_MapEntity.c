modded class SCR_MapEntity
{
	protected ref LL_MapPaintDocument m_LL_PaintDocument;
	protected ref LL_MapPaintToolState m_LL_PaintToolState;
	protected ref LL_MapPaintHistory m_LL_LocalPaintHistory;
	protected bool m_LL_bLocalHistoryCleanup;
	protected bool m_LL_bPaintModeActive;

	void ~SCR_MapEntity()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(LL_PruneLocalPaintHistory);
	}

	LL_MapPaintToolState LL_GetPaintToolState()
	{
		if (!m_LL_PaintToolState)
			m_LL_PaintToolState = new LL_MapPaintToolState();
		return m_LL_PaintToolState;
	}

	LL_MapPaintDocument LL_GetPaintDocument()
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			return manager.LL_GetPaintState().m_Document;
		if (!m_LL_PaintDocument)
			m_LL_PaintDocument = new LL_MapPaintDocument();
		return m_LL_PaintDocument;
	}

	LL_MapPaintHistory LL_GetLocalPaintHistory()
	{
		if (!m_LL_LocalPaintHistory)
			m_LL_LocalPaintHistory = new LL_MapPaintHistory();
		return m_LL_LocalPaintHistory;
	}

	void LL_RecordLocalPaintHistory(int owner, int kind, array<int> erasedOwners = null)
	{
		string author;
		if (GetGame().GetPlayerManager())
			author = GetGame().GetPlayerManager().GetPlayerName(owner);
		if (author.IsEmpty())
			author = "#LL-MapPaint_LocalPlayer";
		LL_GetLocalPaintHistory().Record(owner, author, kind, System.GetTickCount(), LL_MapPaintHistory.GetPlayerNames(erasedOwners), erasedOwners);
		if (!m_LL_bLocalHistoryCleanup)
		{
			GetGame().GetCallqueue().CallLater(LL_PruneLocalPaintHistory, 1000, true);
			m_LL_bLocalHistoryCleanup = true;
		}
	}

	protected void LL_PruneLocalPaintHistory()
	{
		m_LL_LocalPaintHistory.Prune(System.GetTickCount());
		if (m_LL_LocalPaintHistory.IsEmpty())
		{
			GetGame().GetCallqueue().Remove(LL_PruneLocalPaintHistory);
			m_LL_bLocalHistoryCleanup = false;
		}
	}

	void LL_SetPaintModeActive(bool active)
	{
		m_LL_bPaintModeActive = active;
	}

	bool LL_IsPaintModeActive()
	{
		return m_LL_bPaintModeActive;
	}

	//! Keep native marker input blocked even while a selected tool is suspended.
	bool LL_IsPaintToolEnabled()
	{
		if (!IsOpen() || !m_LL_PaintToolState)
			return false;
		return m_LL_PaintToolState.m_bEnabled && m_LL_PaintToolState.m_SelectedMode != LL_EMapPaintMode.NONE;
	}

#ifdef LL_MAP_PAINT_INPUT_TEST
	void LL_TestSetZoomBounds(float minimum, float maximum)
	{
		m_fMinZoom = minimum;
		m_fMaxZoom = maximum;
	}
#endif
}
