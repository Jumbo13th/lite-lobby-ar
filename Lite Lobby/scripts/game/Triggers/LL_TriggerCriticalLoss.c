// Fires when a faction's alive playable count, players and bots alike, drops to or below
// a threshold. For a named AI force that is not made of slots use LL_TriggerDefeatGroup.

class LL_TriggerCriticalLossClass : LL_TriggerComponentClass
{
}

class LL_TriggerCriticalLoss : LL_TriggerComponent
{
	[Attribute("USSR", UIWidgets.EditBox, "Faction whose alive playables (players + bots) are counted.", category: "Lite Lobby")]
	protected FactionKey m_sFactionKey;

	[Attribute("0", UIWidgets.EditBox, "Fire when alive playables of the faction are <= this count.", category: "Lite Lobby")]
	protected int m_iThreshold;

	// Without this the trigger fires at GAME start, before bodies have spawned.
	protected bool m_bSeenAlive;

	override string GetObjectiveMarkup()
	{
		return Header("#LL-Trigger_HeaderCriticalLoss")
			+ WidgetManager.Translate("#LL-Trigger_CriticalLossBody", FactionName(m_sFactionKey), m_iThreshold.ToString());
	}

	override protected void OnActivate()
	{
		GetGame().GetCallqueue().CallLater(PollTick, TICK_MS, true);
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(PollTick);
	}

	protected void PollTick()
	{
		if (!IsEvaluationAllowed())
			return;

		int alive = CountAlive();
		if (alive > m_iThreshold)
			m_bSeenAlive = true;

		if (m_bSeenAlive && alive <= m_iThreshold)
			Fire();
	}

	protected int CountAlive()
	{
		array<IEntity> chars = {};
		CollectAlivePlayableCharacters(chars);

		int count = 0;
		foreach (IEntity ent : chars)
		{
			if (GetEntityFactionKey(ent) == m_sFactionKey)
				count++;
		}
		return count;
	}
}