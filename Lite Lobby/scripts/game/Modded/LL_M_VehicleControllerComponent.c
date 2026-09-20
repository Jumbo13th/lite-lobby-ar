// No engine starts during a hard freeze. The hold's own start on a pinned aircraft comes
// through the same native hook and announces itself.
modded class VehicleControllerComponent
{
	override bool OnBeforeEngineStart()
	{
		if (LL_GameModeCoop.IsHardFreezeActive() && !LL_VehicleHold.IsStartingHeldEngine())
			return false;

		return super.OnBeforeEngineStart();
	}
}
