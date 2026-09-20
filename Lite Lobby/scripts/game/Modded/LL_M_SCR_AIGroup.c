// A world-placed squad spawns its member list on every load, and a resumed world brings
// the saved members back as well: without this guard every squad exists twice. The
// skipped branch mirrors the base class's path for members another system provides.

modded class SCR_AIGroup
{
	override void EOnInit(IEntity owner)
	{
		if (!SCR_PersistenceSystem.IsLoadInProgress())
		{
			super.EOnInit(owner);
			return;
		}

		m_aAllocatedCompartments = new array<BaseCompartmentSlot>;
		m_GroupUtilityComponent = SCR_AIGroupUtilityComponent.Cast(this.FindComponent(SCR_AIGroupUtilityComponent));

		SCR_SpawnPoint.GetOnSpawnPointFinalizeSpawn().Insert(OnSpawnPointFinalizeSpawn);

		// One-shot flag the base class would have consumed on this group.
		s_bIgnoreSpawning = false;

		InvokeEventOnInit();
	}
}
