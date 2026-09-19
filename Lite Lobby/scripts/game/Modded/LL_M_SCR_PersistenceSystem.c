// The load result is a protected event with no invoker. A failed native load must
// refuse the resume on its own, and its order against the system's ACTIVE state is not
// established, so the game mode is told directly.

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
