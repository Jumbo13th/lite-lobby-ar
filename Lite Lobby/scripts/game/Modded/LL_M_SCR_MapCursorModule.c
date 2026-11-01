[BaseContainerProps()]
modded class SCR_MapCursorModule
{
	protected bool LL_IsPaintHistoryHovered()
	{
		SCR_MapDrawingUI drawing = SCR_MapDrawingUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapDrawingUI));
		return drawing && drawing.LL_IsPaintHistoryHovered();
	}

#ifdef LL_MAP_PAINT_INPUT_TEST
	void LL_TestHistoryWheelInput(int direction)
	{
		if (direction > 0)
			OnInputZoomWheelUp(120, EActionTrigger.PRESSED);
		else
			OnInputZoomWheelDown(120, EActionTrigger.PRESSED);
	}
#endif

	override protected void OnInputZoomWheelUp(float value, EActionTrigger reason)
	{
		// Widget wheel consumption does not stop the map's independent input actions.
		if (LL_IsPaintHistoryHovered())
			return;
		super.OnInputZoomWheelUp(value, reason);
	}

	override protected void OnInputZoomWheelDown(float value, EActionTrigger reason)
	{
		if (LL_IsPaintHistoryHovered())
			return;
		super.OnInputZoomWheelDown(value, reason);
	}

	override bool HandleContextualMenu(bool doClose = false)
	{
		if (!doClose && m_MapEntity.LL_IsPaintToolEnabled())
			return false;
		return super.HandleContextualMenu(doClose);
	}

	#ifdef LL_MAP_PAINT_SMOKE
	void LL_TestSetMapDrag(bool active)
	{
		// HandleDrag delegates to the map interaction UI, which is not initialized
		// by the smoke test. Set the cursor bit directly to exercise CanPaint's
		// behavior when the live map cursor reports a drag state.
		if (active)
			SetCursorState(EMapCursorState.CS_DRAG);
		else
			UnsetCursorState(EMapCursorState.CS_DRAG);
	}

	void LL_TestDispatchPaintInput(bool pressed)
	{
		SCR_MapDrawingUI drawing = SCR_MapDrawingUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapDrawingUI));
		if (drawing)
		{
			if (pressed)
				drawing.LL_HandlePaintInput(1, EActionTrigger.DOWN);
			else
				drawing.LL_HandlePaintInput(1, EActionTrigger.UP);
		}
	}
	#endif

	override protected void OnInputDrag(float value, EActionTrigger reason)
	{
		if (m_MapEntity.LL_IsPaintModeActive())
		{
			// LL_MapPaintUI owns the MapDrag listener while the map is open. Keep
			// this cursor callback from panning on press, but do not dispatch the
			// paint gesture a second time. Release still clears any stale drag state.
			if (reason == EActionTrigger.DOWN)
				return;
		}

		super.OnInputDrag(value, reason);
	}
}
