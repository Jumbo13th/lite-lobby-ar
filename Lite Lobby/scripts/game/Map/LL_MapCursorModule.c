// Cursor module for the briefing map (Configs/Map/LL_MapBriefing.conf). The vanilla
// module zooms on every wheel turn, including turns meant for the mission description
// panel over the map; wheel input is ignored while the cursor is over a scroll widget.

[BaseContainerProps()]
class LL_MapCursorModule : SCR_MapCursorModule
{
	protected bool IsCursorOverScrollWidget()
	{
		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);

		array<Widget> outWidgets = {};
		WidgetManager.TraceWidgets(mouseX, mouseY, GetGame().GetWorkspace(), outWidgets);

		foreach (Widget widget : outWidgets)
		{
			if (widget.IsInherited(ScrollLayoutWidget))
				return true;
		}

		return false;
	}

	override protected void OnInputZoomWheelUp(float value, EActionTrigger reason)
	{
		if (m_CursorState & STATE_ZOOM_RESTRICTED)
			return;

		if (IsCursorOverScrollWidget())
			return;

		// Vanilla's zoom math; super cannot be called because it zooms regardless.
		float targetPPU = m_MapEntity.GetTargetZoomPPU();
		value = value * m_fZoomMultiplierWheel;
		m_MapEntity.ZoomSmooth(targetPPU + targetPPU * (value * 0.001), m_fZoomAnimTime, false);
	}

	override protected void OnInputZoomWheelDown(float value, EActionTrigger reason)
	{
		if (m_CursorState & STATE_ZOOM_RESTRICTED)
			return;

		if (IsCursorOverScrollWidget())
			return;

		float targetPPU = m_MapEntity.GetTargetZoomPPU();
		value = value * m_fZoomMultiplierWheel;
		m_MapEntity.ZoomSmooth(targetPPU - targetPPU / 2 * (value * 0.001), m_fZoomAnimTime, false);
	}
}