// Restriction zone that enforces only while the freeze countdown runs. The countdown is
// replicated, so every client's zone self-disables at zero with no server teardown.

class LL_FreezeZoneComponentClass : LL_ZoneRestrictionComponentClass
{
}

class LL_FreezeZoneComponent : LL_ZoneRestrictionComponent
{
	override protected bool IsEnforcementActive()
	{
		LL_GameModeCoop gameMode = LL_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return false;

		return gameMode.GetFreezeTimeRemaining() > 0;
	}
}