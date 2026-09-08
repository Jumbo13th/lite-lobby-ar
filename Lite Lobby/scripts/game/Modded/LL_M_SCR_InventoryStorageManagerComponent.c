// Refuses the inventory key during a hard freeze. The control disable flags do not
// cover opening the inventory, and the hard freeze deliberately has no menu, so each
// input route out of it is closed individually. Action_OpenInventory is the single
// funnel for the player-initiated open; scripted opens stay untouched.

modded class SCR_InventoryStorageManagerComponent
{
	override void Action_OpenInventory()
	{
		if (LL_GameModeCoop.IsHardFreezeActive())
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}

		super.Action_OpenInventory();
	}
};