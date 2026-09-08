// Fires when a named AI group is wiped out, on the group's OnEmpty event (the vanilla
// kill task's signal) with a polled backstop. Resolution is retried because AI groups
// spawn a beat after the game starts. Statistics: the pool splits evenly across the
// members present at resolve; each member's share goes to the unit of the player who
// downed him; the denominator is fixed so reinforcements never inflate or dilute paid
// shares. The timeline gets one line when the whole group falls.

class LL_TriggerDefeatGroupClass : LL_TriggerComponentClass
{
}

class LL_TriggerDefeatGroup : LL_TriggerComponent
{
	[Attribute("", UIWidgets.EditBox, "Entity name of the SCR_AIGroup that must be eliminated.", category: "Lite Lobby")]
	protected string m_sGroupEntityName;

	[Attribute("", UIWidgets.EditBox, "Friendly group name shown in the briefing text (e.g. 'the enemy officers'). Empty = the group's editable/entity name.", category: "Lite Lobby")]
	protected string m_sGroupDisplayName;

	[Attribute("0", UIWidgets.EditBox, "Statistics: point pool split evenly across the group's members — each member's share goes to the unit of the player who killed him. Members killed by AI or the environment pay nobody. 0 = no stat award.", category: "Lite Lobby")]
	protected float m_fStatPoints;

	[Attribute("0", UIWidgets.EditBox, "Statistics: per-player cap — a unit's total from this pool never exceeds this × its contributing players. 0 = uncapped.", category: "Lite Lobby")]
	protected float m_fStatMaxPerPlayer;

	// Bounded resolution retry: ~15 s at the 1 Hz activation tick.
	protected const int MAX_RESOLVE_TRIES = 15;
	protected int m_iResolveTries;

	// Consecutive zero-agent readings the poll needs; the OnEmpty path stays instant.
	protected const int WIPE_CONFIRM_TICKS = 3;
	protected int m_iWipeConfirmations;

	protected SCR_AIGroup m_Group;

	protected ref array<SCR_DamageManagerComponent> m_aWatched = {};
	protected ref array<bool> m_aWatchedCounted = {};
	protected int m_iShareMembers;
	protected string m_sGroupFactionKey;

	override float GetStatPoints()
	{
		return m_fStatPoints;
	}

	override float GetStatMaxPerPlayer()
	{
		return m_fStatMaxPerPlayer;
	}

	override string GetObjectiveMarkup()
	{
		string grp = m_sGroupDisplayName;
		if (grp == "")
			grp = EntityDisplayName(m_sGroupEntityName);
		if (grp == "")
			grp = WidgetManager.Translate("#LL-Trigger_GroupFallback");

		return Header("#LL-Trigger_HeaderElimination") + WidgetManager.Translate("#LL-Trigger_EliminationBody", grp);
	}

	override protected void OnActivate()
	{
		GetGame().GetCallqueue().CallLater(TryResolve, TICK_MS, true);
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(TryResolve);
		GetGame().GetCallqueue().Remove(CheckWipedTick);
		if (m_Group)
			m_Group.GetOnEmpty().Remove(OnGroupEmpty);

		// The last member's damage event may land after OnEmpty fired this disarm.
		SweepDownedMembers();

		foreach (SCR_DamageManagerComponent dmg : m_aWatched)
		{
			if (dmg)
				dmg.GetOnDamageStateChanged().Remove(OnMemberDamage);
		}
		m_aWatched.Clear();
	}

	protected void TryResolve()
	{
		if (m_Group)
			return;

		IEntity ent = GetGame().GetWorld().FindEntityByName(m_sGroupEntityName);
		SCR_AIGroup group = SCR_AIGroup.Cast(ent);
		if (group)
		{
			// A found group with no agents has not spawned its members yet; treating that
			// as a wipe would announce a defeat at mission start.
			if (group.GetAgentsCount() == 0)
				return;

			m_Group = group;
			GetGame().GetCallqueue().Remove(TryResolve);
			m_Group.GetOnEmpty().Insert(OnGroupEmpty);
			HookMembers();
			GetGame().GetCallqueue().CallLater(CheckWipedTick, TICK_MS, true);
			return;
		}

		m_iResolveTries++;
		if (m_iResolveTries >= MAX_RESOLVE_TRIES)
		{
			Print(string.Format("[LL_Trigger] DefeatGroup: AI group '%1' not found — trigger inert.", m_sGroupEntityName), LogLevel.WARNING);
			GetGame().GetCallqueue().Remove(TryResolve);
		}
	}

	protected void HookMembers()
	{
		if (GetStatPoints() <= 0)
			return;

		// The stats manager refuses shares whose killer matches the group's own faction.
		Faction groupFaction = m_Group.GetFaction();
		if (groupFaction)
			m_sGroupFactionKey = groupFaction.GetFactionKey();

		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity body = agent.GetControlledEntity();
			if (!body)
				continue;

			SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.GetDamageManager(body);
			if (!dmg)
				continue;

			m_iShareMembers++;
			dmg.GetOnDamageStateChanged().Insert(OnMemberDamage);
			m_aWatched.Insert(dmg);

			// Pre-dead at resolve: counts toward the wipe, nobody earns him.
			m_aWatchedCounted.Insert(dmg.IsDestroyed());
		}
	}

	protected void OnMemberDamage(EDamageState state)
	{
		if (!IsEvaluationAllowed())
			return;

		SweepDownedMembers();
	}

	protected void SweepDownedMembers()
	{
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (!stats || m_iShareMembers <= 0)
			return;

		float share = GetStatPoints() / m_iShareMembers;

		for (int i = 0; i < m_aWatched.Count(); i++)
		{
			if (m_aWatchedCounted[i])
				continue;

			SCR_DamageManagerComponent dmg = m_aWatched[i];
			if (!dmg || !dmg.IsDestroyed())
				continue;

			m_aWatchedCounted[i] = true;

			int killerId = -1;
			Instigator killer = dmg.GetInstigator();
			if (killer)
				killerId = killer.GetInstigatorPlayerID();

			if (killerId > 0)
				stats.RecordGroupMemberKill(this, killerId, share, m_sGroupFactionKey);
		}
	}

	protected void OnGroupEmpty(AIGroup group)
	{
		if (!IsEvaluationAllowed())
			return;
		Fire();
	}

	// Backstop for the one-shot OnEmpty: AI keeps fighting through a hard freeze while
	// evaluation is gated off, so the event can fire once and be dropped; a group deleted
	// outright raises no event at all.
	protected void CheckWipedTick()
	{
		if (!IsEvaluationAllowed())
			return;

		// Re-resolved by name: a deleted group takes its agents with it.
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByName(m_sGroupEntityName));
		if (group && group.GetAgentsCount() > 0)
		{
			m_iWipeConfirmations = 0;
			return;
		}

		// One zero reading is not proof: the engine reports it for a live group whose
		// members have not spawned, and a trigger fires only once.
		m_iWipeConfirmations++;
		if (m_iWipeConfirmations < WIPE_CONFIRM_TICKS)
			return;

		Fire();
	}
}