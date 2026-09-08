// Taking a slot sets the player-level faction, which triggers the deploy-loadout
// revalidation; this game mode has no loadout manager, so the vanilla handler could only
// error-log. Worlds with a loadout manager are untouched.
modded class SCR_PlayerLoadoutComponent
{
	override protected void OnFactionChanged(FactionAffiliationComponent owner, Faction old, Faction current)
	{
		if (!GetGame().GetLoadoutManager())
			return;

		super.OnFactionChanged(owner, old, current);
	}
}