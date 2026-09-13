// This call is the game's only entry into its AI enemy-marking system. Returning before
// it means no report marker ever exists on the server, so there is nothing to broadcast,
// nothing for join-in-progress to carry and nothing to expire. Without a lobby game mode
// the game keeps its own behaviour.

modded class SCR_AIGroupPerception
{
	override protected void MarkEnemyOnMap(notnull SCR_AITargetInfo target)
	{
		LL_GameModeCoop gameMode = LL_GameModeCoop.GetInstance();
		if (gameMode && !gameMode.AllowAiSpotReports())
			return;

		super.MarkEnemyOnMap(target);
	}
}
