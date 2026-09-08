// Freeze countdown banner on every HUD, read from the replicated value so late joiners
// see the right state.

class LL_FreezeTimeCounter : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected TextWidget m_wCounterText;
	protected LL_GameModeCoop m_GameModeCoop;

	// The freeze-end cue fires once, on the transition to zero.
	protected bool m_bWasRunning;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wCounterText = TextWidget.Cast(w.FindAnyWidget("FreezeTimeCounterText"));
		m_GameModeCoop = LL_GameModeCoop.GetInstance();

		w.SetVisible(false);

		GetGame().GetCallqueue().CallLater(UpdateDisplay, 200, true);
	}

	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);
		GetGame().GetCallqueue().Remove(UpdateDisplay);
	}

	protected void UpdateDisplay()
	{
		if (!m_GameModeCoop)
			m_GameModeCoop = LL_GameModeCoop.GetInstance();
		if (!m_GameModeCoop || !m_wRoot)
			return;

		float remaining = m_GameModeCoop.GetFreezeTimeRemaining();

		// Tracked on the value, not visibility, so a zone warning hiding the timer is not
		// mistaken for the freeze ending. Per client, so everyone hears it.
		if (m_bWasRunning && remaining <= 0)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TASK_CREATED, true);
		m_bWasRunning = remaining > 0;

		// A zone-exit warning takes priority over the countdown.
		bool show = remaining > 0 && !LL_ZoneRestrictionComponent.IsAnyWarningActive();
		m_wRoot.SetVisible(show);
		if (!show)
			return;

		// Ceil so the last second reads 00:01 and the banner vanishes at zero.
		int total = Math.Ceil(remaining);
		int minutes = total / 60;
		int seconds = total - minutes * 60;
		if (m_wCounterText)
			m_wCounterText.SetTextFormat("%1:%2", minutes.ToString(2), seconds.ToString(2));
	}
}