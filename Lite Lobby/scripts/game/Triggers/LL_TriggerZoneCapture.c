// Capture-the-point trigger with flags. Capture progresses while every condition in the
// list holds at once (each compares one faction's headcount inside the zone against
// another's times a multiplier, or a constant); any failed tick resets the hold timer.
// Capture is final; use LL_TriggerZoneContest for a point that changes hands. An empty
// zone makes comparisons trivially true, so a presence term such as "USSR >= 1" is a
// normal condition.

class LL_TriggerZoneCaptureClass : LL_TriggerZoneBaseClass
{
}

class LL_TriggerZoneCapture : LL_TriggerZoneBase
{
	[Attribute(desc: "Capture conditions — ALL must hold at once (AND). Include a presence term (e.g. attacker >= 1) to avoid an empty zone capturing.", category: "Lite Lobby")]
	protected ref array<ref LL_TriggerFactionCondition> m_aConditions;

	[Attribute("120", UIWidgets.EditBox, "Seconds the conditions must hold continuously to capture.", category: "Lite Lobby")]
	protected float m_fHoldSeconds;

	// Resets on any failed tick.
	protected float m_fHeld;
	protected bool m_bCaptured;

	override string GetObjectiveMarkup()
	{
		string conds = "";
		if (m_aConditions)
		{
			foreach (int i, LL_TriggerFactionCondition c : m_aConditions)
			{
				if (i > 0)
					conds += " " + WidgetManager.Translate("#LL-Trigger_CondJoin") + " ";
				conds += c.Describe();
			}
		}

		int hold = m_fHoldSeconds;

		// The zone is the one clickable object; conditions read as a state.
		return Header("#LL-Trigger_HeaderZoneCapture")
			+ WidgetManager.Translate("#LL-Trigger_ZoneCaptureBody", ZoneBriefingLink(), conds, FormatDuration(hold));
	}

	override protected void OnActivate()
	{
		if (!SetupZone())
		{
			Print("[LL_Trigger] ZoneCapture: zone shape not found or too few points — trigger inert.", LogLevel.WARNING);
			return;
		}

		if (!m_aConditions || m_aConditions.IsEmpty())
		{
			Print("[LL_Trigger] ZoneCapture: no capture conditions set — trigger inert.", LogLevel.WARNING);
			return;
		}

		if (m_fHoldSeconds <= 0)
			m_fHoldSeconds = 1;

		SetFlagOwner(GetDefenderFaction());
		SetFlagRaise(1.0);

		GetGame().GetCallqueue().CallLater(PollTick, TICK_MS, true);
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(PollTick);
	}

	protected void PollTick()
	{
		if (m_bCaptured)
			return;

		// Leaving GAME suspends progress but does not reset it.
		if (!IsEvaluationAllowed())
			return;

		array<IEntity> inside = {};
		CollectInsideCharacters(inside);

		// Presence for both sides; dying keeps what was already earned.
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			stats.RecordZonePresence(this, inside, TICK_MS / 1000.0);

		if (IsDominating(inside))
			m_fHeld += TICK_MS / 1000.0;
		else
			m_fHeld = 0;

		float progress = Math.Clamp(m_fHeld / m_fHoldSeconds, 0, 1);

		if (progress >= 1.0)
		{
			Capture();
			return;
		}

		SetFlagRaise(1.0 - progress);
	}

	protected bool IsDominating(notnull array<IEntity> insideChars)
	{
		map<string, int> counts = new map<string, int>();
		CountByFaction(counts, insideChars);

		foreach (LL_TriggerFactionCondition c : m_aConditions)
		{
			if (!c.Evaluate(counts))
				return false;
		}
		return true;
	}

	protected void Capture()
	{
		m_bCaptured = true;
		GetGame().GetCallqueue().Remove(PollTick);

		SetFlagOwner(GetAttackerFaction());
		SetFlagRaise(1.0);

		// Never captured means the pool resolves to the defenders at game end.
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			stats.NotifyZoneCaptured(this);

		Fire();
	}

	// With a pool the resolution writes its own attributed timeline entry.
	override protected bool ReportsOwnStats()
	{
		return GetStatPoints() > 0;
	}

	// Ownership is one-way: the attacker once captured, else still the defender.
	override FactionKey GetZoneHolderFaction()
	{
		if (m_bCaptured)
			return GetAttackerFaction();
		return GetDefenderFaction();
	}
}