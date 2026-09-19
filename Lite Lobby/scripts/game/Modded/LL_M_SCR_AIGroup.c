// A world-placed squad spawns its member list on every world load, while a resumed
// world also brings the saved members back through the persistence system: without
// this guard every squad would exist twice. The game's own ambient patrols carry the
// same check. The skipped branch mirrors the base class's own path for a group whose
// members another system provides.

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
