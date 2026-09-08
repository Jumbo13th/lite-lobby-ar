// Guards the map drawing tool's two unguarded button dereferences. Map UI components are
// shared between screens (see LL_M_SCR_MapToolMenuUI), so m_ToolMenuEntry outlives a
// screen while its m_ButtonComp is only valid while that screen's tool bar lives. Reached
// from UpdateLineCount (a cached line restored on the next open) and SetDrawMode (the
// toggled invoker fires inside PopulateToolMenu's own loop).

modded class SCR_MapDrawingUI
{
	// Skipping is harmless: the next SetDrawMode refreshes the counter.
	override void UpdateLineCount()
	{
		if (!m_ToolMenuEntry || !m_ToolMenuEntry.m_ButtonComp)
			return;

		super.UpdateLineCount();
	}

	// Only the tail of the vanilla body touches the button, so the button is rebuilt first.
	override protected void SetDrawMode(bool state, bool cacheDrawn = false)
	{
		LL_RestoreToolButton();

		// Bailing here skips real teardown (listeners stay, the cursor stays in drawing
		// mode and the map cannot be panned). Unreachable on every screen that carries a
		// ToolMenu tree.
		if (!m_ToolMenuEntry || !m_ToolMenuEntry.m_ButtonComp)
			return;

		super.SetDrawMode(state, cacheDrawn);
	}

	// Do not substitute a widget for the missing button: SCR_MapToolEntry.UpdateVisual
	// shows whatever widget it is handed, and a widget created with a null parent lands
	// on the workspace root (a stray compass floating over every screen).
	protected void LL_RestoreToolButton()
	{
		if (!m_ToolMenuEntry || m_ToolMenuEntry.m_ButtonComp || !m_MapEntity)
			return;

		SCR_MapToolMenuUI toolMenu = SCR_MapToolMenuUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (toolMenu)
			toolMenu.LL_EnsureEntryButtons();
	}
}