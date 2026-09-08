// Fires a fixed number of seconds after the freeze period ends; the mission proper
// begins when players unfreeze. Announces the deadline; it does not end the mission.

class LL_TriggerMissionEndTimerClass : LL_TriggerComponentClass
{
}

class LL_TriggerMissionEndTimer : LL_TriggerComponent
{
	[Attribute("600", UIWidgets.EditBox, "Seconds after freeze ends before the trigger fires.", category: "Lite Lobby")]
	protected int m_iSeconds;

	// Drained by CountdownTick, which skips during a hard freeze or outside GAME.
	protected float m_fSecondsLeft;

	override string GetObjectiveMarkup()
	{
		return Header("#LL-Trigger_HeaderTimeLimit")
			+ WidgetManager.Translate("#LL-Trigger_TimeLimitBody", FormatDuration(m_iSeconds));
	}

	override protected void OnActivate()
	{
		// Re-checked every tick so an admin extending or shortening the freeze is respected.
		GetGame().GetCallqueue().CallLater(WaitForFreezeEnd, TICK_MS, true);
	}

	protected void WaitForFreezeEnd()
	{
		LL_GameModeCoop gm = LL_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gm)
			return;
		if (gm.GetFreezeTimeRemaining() > 0)
			return;

		GetGame().GetCallqueue().Remove(WaitForFreezeEnd);

		m_fSecondsLeft = m_iSeconds;
		if (m_fSecondsLeft < 0)
			m_fSecondsLeft = 0;

		GetGame().GetCallqueue().CallLater(CountdownTick, 1000, true);
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(WaitForFreezeEnd);
		GetGame().GetCallqueue().Remove(CountdownTick);
	}

	// A 1 s countdown rather than one CallLater at the full duration: an absolute
	// deadline keeps running through a hard freeze, and refusing it in the handler
	// would cancel the mission end rather than delay it.
	protected void CountdownTick()
	{
		if (!IsEvaluationAllowed())
			return;

		m_fSecondsLeft -= 1.0;
		if (m_fSecondsLeft > 0)
			return;

		GetGame().GetCallqueue().Remove(CountdownTick);
		Fire();
	}
}