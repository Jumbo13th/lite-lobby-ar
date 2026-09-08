// Map marker radial on a map opened without a controlled character. Visibility:
// SCR_InfoDisplayExtended is entity-gated and the default flags lack WITHOUT_ENTITY, so
// the protected m_bCanShow is forced at its single consumption point. Layer: a
// menu-hosted map needs the radial in the ALWAYS_TOP root, set in DisplayStartDrawInit
// because the raw InfoDisplay events are sealed.

modded class SCR_MapRadialDisplay
{
	override protected bool DisplayStartDrawInit(IEntity owner)
	{
		m_eLayer = EHudLayers.ALWAYS_TOP;
		return super.DisplayStartDrawInit(owner);
	}

	override void Show(bool show, float speed = UIConstants.FADE_RATE_INSTANT, EAnimationCurve curve = EAnimationCurve.LINEAR)
	{
		m_bCanShow = true;
		super.Show(show, speed, curve);
	}
}