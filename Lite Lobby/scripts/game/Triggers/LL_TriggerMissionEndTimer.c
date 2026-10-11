// Fires a fixed number of seconds after the freeze period ends; the mission proper
// begins when players unfreeze. Announces the deadline; it does not end the mission.
// The countdown is a deadline on the mission clock, not a count of ticks: the clock stands
// still through hard freezes and survives a session resume.

class LL_TriggerMissionEndTimerClass : LL_TriggerComponentClass
{
}

class LL_TriggerMissionEndTimer : LL_TriggerComponent
{
	[Attribute("600", UIWidgets.EditBox, "Seconds after freeze ends before the trigger fires.", category: "Lite Lobby")]
	protected int m_iSeconds;

	// The mission-clock reading the countdown began at; -1 while waiting for the freeze.
	protected float m_fStartedAt = -1;
	protected bool m_bCounting;

	int GetDuration()
	{
		return m_iSeconds;
	}

	// Timed announcements inherit the countdown but must not feed the spectator clock.
	protected bool IsMissionEndTimer()
	{
		return Type() == LL_TriggerMissionEndTimer;
	}

	override string GetObjectiveMarkup()
	{
		return Header("#LL-Trigger_HeaderTimeLimit")
			+ WidgetManager.Translate("#LL-Trigger_TimeLimitBody", FormatDuration(m_iSeconds));
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer() || !IsMissionEndTimer())
			return;

		// The game mode may init after this entity.
		GetGame().GetCallqueue().CallLater(ReportDuration_S, 0, false);
	}

	protected void ReportDuration_S()
	{
		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		if (gm)
			gm.SetMissionEndDuration_S(m_iSeconds);
		else
			GetGame().GetCallqueue().CallLater(ReportDuration_S, 500, false);
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
		StartCountdown(gm.GetElapsedTime());
	}

	protected void StartCountdown(float clockReading)
	{
		m_fStartedAt = clockReading;
		m_bCounting = true;

		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		if (gm && IsMissionEndTimer())
			gm.SetMissionEndStartedAt_S(m_fStartedAt);

		GetGame().GetCallqueue().CallLater(CountdownTick, 1000, true);
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(WaitForFreezeEnd);
		GetGame().GetCallqueue().Remove(CountdownTick);
	}

	protected void CountdownTick()
	{
		if (!IsEvaluationAllowed())
			return;

		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		if (!gm)
			return;

		if (gm.GetElapsedTime() < m_fStartedAt + m_iSeconds)
			return;

		GetGame().GetCallqueue().Remove(CountdownTick);
		Fire();
	}

	// Started = counting, not merely waiting for the freeze to end.
	override bool HasStarted()
	{
		return m_bCounting;
	}

	override protected LL_TriggerState CreateState()
	{
		return new LL_TriggerTimedState();
	}

	override protected void SaveState(LL_TriggerState state)
	{
		LL_TriggerTimedState timed = LL_TriggerTimedState.Cast(state);
		if (timed)
			timed.startedAt = m_fStartedAt;
	}

	// Resumes directly in the countdown: no activation poll, no freeze wait.
	override protected void LoadState(LL_TriggerState state)
	{
		LL_TriggerTimedState timed = LL_TriggerTimedState.Cast(state);
		if (!timed)
			return;

		m_bActivated = true;
		GetGame().GetCallqueue().Remove(TryActivate);
		StartCountdown(timed.startedAt);
	}
}