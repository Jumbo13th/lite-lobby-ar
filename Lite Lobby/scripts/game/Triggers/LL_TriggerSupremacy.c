// Fires when one side outnumbers the other by a set ratio, so a decided match can be
// called before the last body drops. Counts alive playables, players and bots alike.
// One component watches one direction; place a second with the factions swapped for
// the other. Announce only; ending the round stays the Game Master's decision.
class LL_TriggerSupremacyClass : LL_TriggerComponentClass
{
}

class LL_TriggerSupremacy : LL_TriggerComponent
{
	[Attribute("US", UIWidgets.EditBox, "Faction that must pull ahead. Type the faction key, e.g. US, USSR, FIA. In 'US has 3x USSR' this is US.", category: "Lite Lobby")]
	protected FactionKey m_sFactionKey;

	[Attribute("USSR", UIWidgets.EditBox, "Faction it must outnumber. In 'US has 3x USSR' this is USSR. Anyone belonging to a third faction is not counted on either side.", category: "Lite Lobby")]
	protected FactionKey m_sVersusFaction;

	[Attribute("3", UIWidgets.EditBox, "How many times the other side's alive count this faction needs. 3 means 24-vs-8 fires, 24-vs-9 does not.", category: "Lite Lobby")]
	protected float m_fSupremacyRatio;

	// 0 vs 0 satisfies any ratio; both sides must have fielded somebody first.
	protected bool m_bBothSeen;

	// A ratio below 1 fires the instant the sides are drawn up.
	protected const float MIN_RATIO = 1;

	override string GetObjectiveMarkup()
	{
		// Reuses the zone conditions' sentence so the wording is identical everywhere.
		string edge = WidgetManager.Translate("#LL-TriggerCond_VersusFaction",
			FactionName(m_sFactionKey),
			WidgetManager.Translate("#LL-TriggerCond_OpAtLeast"),
			LL_TriggerFactionCondition.MultiplierPhrase(m_fSupremacyRatio),
			FactionName(m_sVersusFaction));

		return Header("#LL-Trigger_HeaderSupremacy")
			+ WidgetManager.Translate("#LL-Trigger_SupremacyBody", edge);
	}

	override protected void OnActivate()
	{
		if (m_fSupremacyRatio < MIN_RATIO)
		{
			Print(string.Format("[LL_Trigger] Supremacy '%1': ratio %2 is below %3 — raised, or it would fire at H-hour.",
				GetStatKey(), m_fSupremacyRatio.ToString(), MIN_RATIO.ToString()), LogLevel.WARNING);
			m_fSupremacyRatio = MIN_RATIO;
		}

		// Either leaves a trigger that polls forever and can never fire.
		if (m_sFactionKey == "" || m_sVersusFaction == "")
			Print(string.Format("[LL_Trigger] Supremacy '%1': a faction key is empty — this trigger can never fire.", GetStatKey()), LogLevel.WARNING);
		else if (m_sFactionKey == m_sVersusFaction)
			Print(string.Format("[LL_Trigger] Supremacy '%1': both keys are '%2' — a faction cannot outnumber itself.", GetStatKey(), m_sFactionKey), LogLevel.WARNING);

		// Bodies spawn in over several ticks at the switch to GAME, and a side whose first
		// man appears while the other has twenty up is momentarily 20:1. Nobody can die
		// during freeze, so waiting misses no result.
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
		GetGame().GetCallqueue().CallLater(PollTick, TICK_MS, true);
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(WaitForFreezeEnd);
		GetGame().GetCallqueue().Remove(PollTick);
	}

	protected void PollTick()
	{
		if (!IsEvaluationAllowed())
			return;

		int mine, theirs;
		CountBothSides(mine, theirs);

		if (mine > 0 && theirs > 0)
			m_bBothSeen = true;

		// Backstop for a mission authored with no freeze at all.
		if (!m_bBothSeen)
			return;

		// A wiped-out side satisfies any ratio; the seen-both guard proves they were on the board.
		if (mine >= theirs * m_fSupremacyRatio)
			Fire();
	}

	protected void CountBothSides(out int mine, out int theirs)
	{
		mine = 0;
		theirs = 0;

		array<IEntity> chars = {};
		CollectAlivePlayableCharacters(chars);

		foreach (IEntity ent : chars)
		{
			FactionKey key = GetEntityFactionKey(ent);
			if (key == m_sFactionKey)
				mine++;
			else if (key == m_sVersusFaction)
				theirs++;
		}
	}
}