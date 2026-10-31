// Map squads can queue member spawns before persistence exists; the queue can outlive
// the load. Groups spawned during the game are recreated by the save while it is still
// being applied. Restored bodies must stay the only members, including after casualties.

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
		if (GetWorld().IsEditMode())
			return false;

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		string kind = "map squad";
		if (!IsLoaded())
		{
			if (!persistence || !persistence.WasDataLoaded() || persistence.GetState() >= EPersistenceSystemState.ACTIVE)
				return false;

			kind = "restored group";
		}
		else if (!persistence || !persistence.WasDataLoaded())
		{
			SaveGameManager manager = GetGame().GetSaveGameManager();
			if (!manager || !manager.GetActiveSave())
				return false;
			if (persistence && persistence.GetState() >= EPersistenceSystemState.ACTIVE)
				return false;
		}

		m_bLLResumeSpawnSuppressed = true;
		if (Replication.IsServer())
			Print(string.Format("[LL_Lobby] Resume: member spawning suppressed for %1 '%2'", kind, GetName()), LogLevel.NORMAL);
		return true;
	}
}
