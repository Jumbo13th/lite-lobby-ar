// Map squads can queue member spawns before persistence exists; the queue can outlive
// the load. Restored bodies must stay the only members, including after casualties.

modded class SCR_AIGroup
{
	protected bool m_bLLResumeSpawnSuppressed;

	override void EOnInit(IEntity owner)
	{
		if (LL_ShouldSuppressMemberSpawning())
			IgnoreSpawning(true);

		super.EOnInit(owner);
	}

	override bool ExpandOneMember()
	{
		if (LL_ShouldSuppressMemberSpawning())
		{
			InvokeEventOnInit();
			return false;
		}

		return super.ExpandOneMember();
	}

	// A failed expansion alone is retried by the native queue; completion drops it.
	override bool IsExpandComplete()
	{
		if (LL_ShouldSuppressMemberSpawning())
		{
			InvokeEventOnInit();
			return true;
		}

		return super.IsExpandComplete();
	}

	protected bool LL_ShouldSuppressMemberSpawning()
	{
		if (m_bLLResumeSpawnSuppressed)
			return true;
		if (!IsLoaded() || GetWorld().IsEditMode())
			return false;

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		bool restored = persistence && persistence.WasDataLoaded();
		if (!restored)
		{
			SaveGameManager manager = GetGame().GetSaveGameManager();
			if (!manager || !manager.GetActiveSave())
				return false;
			if (persistence && persistence.GetState() >= EPersistenceSystemState.ACTIVE)
				return false;
		}

		m_bLLResumeSpawnSuppressed = true;
		if (Replication.IsServer())
			Print(string.Format("[LL_Lobby] Resume: member spawning suppressed for map squad '%1'", GetName()), LogLevel.NORMAL);
		return true;
	}
}
