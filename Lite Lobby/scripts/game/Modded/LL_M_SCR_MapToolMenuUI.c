// Re-points the map tool menu at the screen that is actually opening. SetupMapConfig
// returns the cached config and UI component instances when the map mode repeats, and
// every fullscreen map here shares one SCR_MapToolMenuUI, whose cached tool-bar widgets
// then point into a dead tree; vanilla dereferences them unconditionally.

modded class SCR_MapToolMenuUI
{
	protected bool m_bLLPopulating;

	override void OnMapOpen(MapConfiguration config)
	{
		Widget menuRoot;
		if (m_MapEntity)
			menuRoot = m_MapEntity.GetMapMenuRoot();

		if (menuRoot)
		{
			// Comparing first keeps a reopened screen free of tree walks.
			Widget toolMenuRoot = menuRoot.FindAnyWidget(m_sToolMenuRootName);
			if (toolMenuRoot && toolMenuRoot != m_wToolMenuRoot)
			{
				m_wToolMenuRoot = toolMenuRoot;
				m_wToolMenuBar = toolMenuRoot.FindAnyWidget(m_sToolBarName);
			}
		}

		// No toolbar on this screen: skip the component rather than let vanilla
		// dereference null. OnMapClose null-checks the root.
		if (!m_wToolMenuRoot || !m_wToolMenuBar)
			return;

		// PopulateToolMenu appends and OnMapClose never removes; the spectator map's menu
		// stays alive between opens, so every M press would stack another set of tools.
		SCR_WidgetHelper.RemoveAllChildren(m_wToolMenuBar);

		// Same re-entrancy window as LL_EnsureEntryButtons.
		m_bLLPopulating = true;
		super.OnMapOpen(config);
		m_bLLPopulating = false;
	}

	//! Rebuilds the tool buttons if any entry lost its live one: PopulateToolMenu assigns
	//! buttons entry by entry, and an event fired from inside that loop can reach an
	//! entry that still holds a dead pointer. No-op without a tool bar.
	void LL_EnsureEntryButtons()
	{
		// Re-entrancy guard: PopulateToolMenu fires the toggled invoker back into a caller
		// that lands here while entries are still buttonless. Unguarded, this hard-locks.
		if (m_bLLPopulating || !m_wToolMenuRoot || !m_wToolMenuBar)
			return;

		bool missing = false;
		foreach (SCR_MapToolEntry entry : m_aMenuEntries)
		{
			if (entry && !entry.m_ButtonComp)
			{
				missing = true;
				break;
			}
		}

		if (!missing)
			return;

		m_bLLPopulating = true;
		SCR_WidgetHelper.RemoveAllChildren(m_wToolMenuBar);
		PopulateToolMenu();
		m_bLLPopulating = false;
	}
}