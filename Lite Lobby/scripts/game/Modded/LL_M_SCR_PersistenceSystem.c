// The load result is a protected event with no invoker; a failed native load must refuse
// the resume on its own.

modded class SCR_PersistenceSystem
{
	override protected void OnAfterLoad(bool success)
	{
		super.OnAfterLoad(success);

		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && Replication.IsServer())
			gameMode.OnNativeLoadResult_S(success);
	}
}
