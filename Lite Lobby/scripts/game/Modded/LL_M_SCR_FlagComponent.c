// Adds an owning faction and a raise level to the vanilla flag as replicated properties
// with apply-on-change callbacks; the stock component only swaps a material locally.
// Needs an RplComponent on the flag entity and a SlotManagerComponent with a "Flag" slot
// for the physical raise; without the slot only the material flips.

modded class SCR_FlagComponent
{
	// "" = nobody holds it.
	[RplProp(onRplName: "LL_OnFlagStateChanged")]
	protected FactionKey m_sLLOwnerFaction;

	// 0 = fully lowered, 1 = fully raised; driven each tick during a capture.
	[RplProp(onRplName: "LL_OnFlagStateChanged")]
	protected float m_fLLRaiseLevel = 1.0;

	// Fraction of the mast's local height the flag travels.
	protected const float LL_RAISE_TRAVEL = 0.85;

	protected SlotManagerComponent m_LLSlotManager;
	protected bool m_bLLSlotReady;
	protected float m_fLLBaseHeight;

	// What is currently ON the mesh, so the expensive material swap runs only when
	// What is on the mesh, so the material swap runs only when the appearance changes.
	protected bool m_bLLNeutral;
	protected FactionKey m_sLLShownFaction;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_LLSlotManager = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
		if (!m_LLSlotManager)
			return;

		EntitySlotInfo slot = m_LLSlotManager.GetSlotByName("Flag");
		if (!slot)
			return;

		vector m[4];
		slot.GetLocalTransform(m);
		m_fLLBaseHeight = m[3][1];
		m_bLLSlotReady = true;
	}

	void LL_SetOwnerFaction_S(FactionKey key)
	{
		if (!Replication.IsServer() || m_sLLOwnerFaction == key)
			return;

		m_sLLOwnerFaction = key;
		LL_ApplyFlag();
		Replication.BumpMe();
	}

	void LL_SetRaiseLevel_S(float level)
	{
		if (!Replication.IsServer())
			return;

		level = Math.Clamp(level, 0, 1);
		if (Math.AbsFloat(level - m_fLLRaiseLevel) < 0.001)
			return;

		m_fLLRaiseLevel = level;
		LL_ApplyFlag();
		Replication.BumpMe();
	}

	FactionKey LL_GetOwnerFaction()
	{
		return m_sLLOwnerFaction;
	}

	// The single decision about the mast's look. Neutral = no owner and fully down; the
	// raise level is part of the test because a zone that starts neutral asks for owner
	// "", the default, so no owner-side replication ever reaches a proxy.
	protected void LL_ApplyFlag()
	{
		bool neutral = (m_sLLOwnerFaction == "" && m_fLLRaiseLevel <= 0);

		if (neutral)
		{
			// ChangeMaterial does mesh work; not on the per-tick raise updates.
			if (!m_bLLNeutral)
			{
				m_bLLNeutral = true;
				m_sLLShownFaction = "";
				LL_ApplyDefaultMaterial();
			}
		}
		else if (m_bLLNeutral || m_sLLShownFaction != m_sLLOwnerFaction)
		{
			SCR_Faction f = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_sLLOwnerFaction));
			if (f)
			{
				m_bLLNeutral = false;
				m_sLLShownFaction = m_sLLOwnerFaction;
				ChangeMaterial(f.GetFactionFlagMaterial());
			}
		}

		LL_ApplyRaiseLevel();
	}

	// The pole's own default flag, the material vanilla raises in EOnInit. An empty
	// resource would hide the flag entity and leave a naked mast.
	protected void LL_ApplyDefaultMaterial()
	{
		SCR_FlagComponentClass prefabData = SCR_FlagComponentClass.Cast(GetComponentData(GetOwner()));
		if (prefabData)
			ChangeMaterial(prefabData.GetDefaultMaterial(), prefabData.GetDefaultMLOD());
	}

	protected void LL_ApplyRaiseLevel()
	{
		if (!m_bLLSlotReady)
			return;

		EntitySlotInfo slot = m_LLSlotManager.GetSlotByName("Flag");
		if (!slot)
			return;

		// Additive offset: 0 at fully raised, toward the mast base as the level drops.
		float offsetY = m_fLLBaseHeight * (m_fLLRaiseLevel - 1.0) * LL_RAISE_TRAVEL;
		vector m[4];
		m[0] = Vector(1, 0, 0);
		m[1] = Vector(0, 1, 0);
		m[2] = Vector(0, 0, 1);
		m[3] = Vector(0, offsetY, 0);
		slot.SetAdditiveTransformLS(m);
	}

	void LL_OnFlagStateChanged()
	{
		LL_ApplyFlag();
	}
}