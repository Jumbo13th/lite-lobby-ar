// Holds the video options at the mission's minimum (LL_GraphicsPolicy); the floor
// applied on join would otherwise last until the player opens the settings. A
// pass-through with no lobby game mode or the option off.
modded class SCR_VideoSettingsSubMenu : SCR_SettingsSubMenuBase
{
	// Shadow quality's spinbox skips the lowest step (stored 1..3 shown as 0..2), the
	// same -1 vanilla applies.
	protected const string WIDGET_SHADOW_QUALITY = "ShadowQuality";
	protected const string WIDGET_DISTANT_SHADOWS = "DistantShadows";
	protected const string WIDGET_GRASS_LOD = "GrassLOD";
	protected const string WIDGET_CONTACT_SHADOWS = "SSDO";

	protected bool m_bLL_SyncQueued;

	override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
	{
		super.OnTabCreate(menuRoot, buttonsLayout, index);

		// A preset or an external edit may have dropped a value since the join-time pass.
		LL_ApplyMinimums();
		LL_SyncEnforcedRows();
	}

	override void OnTabRemove()
	{
		// The deferred sync outlives the tab otherwise.
		GetGame().GetCallqueue().Remove(LL_DeferredSync);
		super.OnTabRemove();
	}

	override void OnMenuItemChanged(SCR_SettingsBindingBase binding)
	{
		if (m_bLoadingSettings || !binding || !LL_IsEnforcedRow(binding.GetWidgetName()))
		{
			super.OnMenuItemChanged(binding);
			return;
		}

		// The binding has already written the lower value to the module.
		bool changed = LL_ApplyMinimums();
		super.OnMenuItemChanged(binding);

		if (changed)
			LL_QueueSync();
	}

	override protected void OnQualityPresetChanged(SCR_ComboBoxComponent combobox, int itemIndex)
	{
		// A preset rewrites every setting at once; the low presets are the easy way around the floor.
		super.OnQualityPresetChanged(combobox, itemIndex);

		if (LL_ApplyMinimums())
			LL_QueueSync();
	}

	// Raises the modules and marks the preset combo Custom.
	protected bool LL_ApplyMinimums()
	{
		if (!LL_GraphicsPolicy.ApplyMinimums(m_Video, m_Pipeline))
			return false;

		SetQualityPresetIndex(WIDGET_SHADOW_QUALITY);
		return true;
	}

	// Writing a row from inside its own change handler re-enters the binding.
	protected void LL_QueueSync()
	{
		if (m_bLL_SyncQueued)
			return;

		m_bLL_SyncQueued = true;
		GetGame().GetCallqueue().CallLater(LL_DeferredSync, 0, false);
	}

	protected void LL_DeferredSync()
	{
		m_bLL_SyncQueued = false;
		LL_SyncEnforcedRows();
	}

	// The widgets end up showing what is really set.
	protected void LL_SyncEnforcedRows()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		if (!m_wRoot || !mode || !mode.EnforceMinGraphics())
			return;

		// Guards the change handlers while the rows are written back.
		m_bLoadingSettings = true;

		LL_ClampRow(WIDGET_SHADOW_QUALITY, mode.GetMinShadowQuality() - 1);
		LL_ClampRow(WIDGET_DISTANT_SHADOWS, mode.GetMinDistantShadows());
		LL_ClampRow(WIDGET_GRASS_LOD, mode.GetMinGrassLod());
		LL_ClampRow(WIDGET_CONTACT_SHADOWS, mode.GetMinContactShadows());

		m_bLoadingSettings = false;
	}

	// Never lowers a row.
	protected void LL_ClampRow(string widgetName, int minIndex)
	{
		SCR_SpinBoxComponent spinBox = SCR_SpinBoxComponent.GetSpinBoxComponent(widgetName, m_wRoot);
		if (!spinBox)
			return;

		minIndex = Math.ClampInt(minIndex, 0, spinBox.GetNumItems() - 1);
		if (spinBox.GetCurrentIndex() < minIndex)
			spinBox.SetCurrentItem(minIndex);
	}

	protected bool LL_IsEnforcedRow(string widgetName)
	{
		return widgetName == WIDGET_SHADOW_QUALITY
			|| widgetName == WIDGET_DISTANT_SHADOWS
			|| widgetName == WIDGET_GRASS_LOD
			|| widgetName == WIDGET_CONTACT_SHADOWS;
	}
}