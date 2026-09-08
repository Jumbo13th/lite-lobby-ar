// Fires when every listed target reports DESTROYED. Event-driven on each target's
// damage manager, with a poll for targets deleted outright. A target already destroyed
// or missing at activation counts as destroyed.

[BaseContainerProps()]
class LL_TriggerDestroyTarget
{
	[Attribute("", UIWidgets.EditBox, "World entity name of the object that must be destroyed.", category: "Lite Lobby")]
	string m_sEntityName;

	[Attribute("", UIWidgets.EditBox, "Name shown in the Mission Conditions text. Empty = the object's editable/entity name.", category: "Lite Lobby")]
	string m_sDisplayName;

	[Attribute("0", UIWidgets.CheckBox, "Show this target as a clickable link that flies the map to it.", category: "Lite Lobby")]
	bool m_bClickable;
}

class LL_TriggerDestroyClass : LL_TriggerComponentClass
{
}

class LL_TriggerDestroy : LL_TriggerComponent
{
	[Attribute(desc: "Objects that must ALL be destroyed for the trigger to fire.", category: "Lite Lobby")]
	protected ref array<ref LL_TriggerDestroyTarget> m_aTargets;

	[Attribute("0", UIWidgets.EditBox, "Statistics: point pool split evenly across the targets — each target's share goes to the unit of the player who destroyed it. Targets destroyed with no player instigator pay nobody. 0 = no stat award.", category: "Lite Lobby")]
	protected float m_fStatPoints;

	[Attribute("0", UIWidgets.EditBox, "Statistics: per-player cap — a unit's total from this pool never exceeds this × its contributing players. 0 = uncapped.", category: "Lite Lobby")]
	protected float m_fStatMaxPerPlayer;

	override float GetStatPoints()
	{
		return m_fStatPoints;
	}

	override float GetStatMaxPerPlayer()
	{
		return m_fStatMaxPerPlayer;
	}

	// Parallel arrays; the stat award is recorded the moment each target goes down.
	protected ref array<SCR_DamageManagerComponent> m_aWatched = {};
	protected ref array<string> m_aWatchedLabels = {};
	protected ref array<bool> m_aWatchedCounted = {};

	// Every target the designer named, resolved or not: a missing target must not inflate
	// the others' worth.
	protected int m_iShareTargets;

	override string GetObjectiveMarkup()
	{
		if (!m_aTargets || m_aTargets.IsEmpty())
			return Header("#LL-Trigger_HeaderDestruction") + WidgetManager.Translate("#LL-Trigger_DestructionBodyGeneric");

		string list = "";
		foreach (LL_TriggerDestroyTarget t : m_aTargets)
		{
			if (!t || t.m_sEntityName == "")
				continue;

			string entry;
			if (t.m_bClickable)
				entry = ObjectLink(t.m_sEntityName, t.m_sDisplayName);
			else if (t.m_sDisplayName != "")
				entry = t.m_sDisplayName;
			else
				entry = EntityDisplayName(t.m_sEntityName);

			if (list != "")
				list += ", ";
			list += entry;
		}

		if (list == "")
			return Header("#LL-Trigger_HeaderDestruction") + WidgetManager.Translate("#LL-Trigger_DestructionBodyGeneric");

		// Whole-sentence keys with singular and plural variants keep translations grammatical.
		if (m_aTargets.Count() == 1)
			return Header("#LL-Trigger_HeaderDestruction") + WidgetManager.Translate("#LL-Trigger_DestructionBodyOne", list);
		return Header("#LL-Trigger_HeaderDestruction") + WidgetManager.Translate("#LL-Trigger_DestructionBodyMany", list);
	}

	override protected void OnActivate()
	{
		if (!m_aTargets || m_aTargets.IsEmpty())
			return;

		foreach (LL_TriggerDestroyTarget t : m_aTargets)
		{
			if (!t || t.m_sEntityName == "")
				continue;

			m_iShareTargets++;

			IEntity ent = GetGame().GetWorld().FindEntityByName(t.m_sEntityName);
			if (!ent)
				continue;

			SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.GetDamageManager(ent);
			if (!dmg)
				continue;

			dmg.GetOnDamageStateChanged().Insert(OnTargetDamage);
			m_aWatched.Insert(dmg);

			string label = t.m_sDisplayName;
			if (label == "")
				label = EntityDisplayName(t.m_sEntityName);
			m_aWatchedLabels.Insert(label);

			// Pre-wrecked at activation: counts for firing, nobody earns it.
			m_aWatchedCounted.Insert(dmg.IsDestroyed());
		}

		if (AllDestroyed())
		{
			Fire();
			return;
		}

		// Events cannot see a target being deleted (Game Master cleanup, wreck despawn).
		GetGame().GetCallqueue().CallLater(CheckDeletedTargetsTick, TICK_MS, true);
	}

	protected void CheckDeletedTargetsTick()
	{
		if (!IsEvaluationAllowed())
			return;

		if (AllDestroyed())
			Fire();
	}

	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(CheckDeletedTargetsTick);

		foreach (SCR_DamageManagerComponent dmg : m_aWatched)
		{
			if (dmg)
				dmg.GetOnDamageStateChanged().Remove(OnTargetDamage);
		}
		m_aWatched.Clear();
	}

	protected void OnTargetDamage(EDamageState state)
	{
		if (!IsEvaluationAllowed())
			return;

		SweepNewlyDestroyed();

		if (AllDestroyed())
			Fire();
	}

	// The damage manager's last instigator is the best attribution at the DESTROYED transition.
	protected void SweepNewlyDestroyed()
	{
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (!stats)
			return;

		float share = 0;
		if (m_iShareTargets > 0)
			share = GetStatPoints() / m_iShareTargets;

		for (int i = 0; i < m_aWatched.Count(); i++)
		{
			if (m_aWatchedCounted[i])
				continue;

			SCR_DamageManagerComponent dmg = m_aWatched[i];
			if (!dmg || !dmg.IsDestroyed())
				continue;

			m_aWatchedCounted[i] = true;

			int instigatorPlayerId = -1;
			Instigator killer = dmg.GetInstigator();
			if (killer)
				instigatorPlayerId = killer.GetInstigatorPlayerID();

			stats.RecordKeyTargetDestroyed(this, m_aWatchedLabels[i], instigatorPlayerId, share);
		}
	}

	// Each target records its own attributed event; a generic entry would double-report.
	override protected bool ReportsOwnStats()
	{
		return true;
	}

	protected bool AllDestroyed()
	{
		// A named target that no longer resolves is treated as destroyed.
		foreach (LL_TriggerDestroyTarget t : m_aTargets)
		{
			if (!t || t.m_sEntityName == "")
				continue;

			IEntity ent = GetGame().GetWorld().FindEntityByName(t.m_sEntityName);
			if (!ent)
				continue;

			SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.GetDamageManager(ent);
			if (dmg && !dmg.IsDestroyed())
				return false;
		}
		return true;
	}
}