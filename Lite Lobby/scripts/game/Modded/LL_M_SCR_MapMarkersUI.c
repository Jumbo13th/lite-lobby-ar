modded class SCR_MapMarkersUI
{
	override protected void OnInputQuickMarkerMenu(float value, EActionTrigger reason)
	{
		if (m_MapEntity.LL_IsPaintToolEnabled())
			return;
		super.OnInputQuickMarkerMenu(value, reason);
	}

	override protected void DelayedQuickMarkerMenu()
	{
		// Drawing may have been enabled after the input queued this callback.
		if (m_MapEntity.LL_IsPaintToolEnabled())
			return;
		super.DelayedQuickMarkerMenu();
	}

#ifdef LL_MAP_PAINT_INPUT_TEST
	void LL_TestRunDelayedQuickMarkerMenu()
	{
		DelayedQuickMarkerMenu();
	}
#endif
}
