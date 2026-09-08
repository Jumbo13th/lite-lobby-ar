// A dead player keeps controlling the corpse, whose empty blood hit zone drains the
// colour grade to grey; the grade is a world post-process that also covers the spectator
// camera, and the vanilla manager hides only widget effects on a non-player camera.

modded class SCR_DesaturationEffect
{
	override protected void UpdateEffect(float timeSlice)
	{
		LL_SpectatorManager spectatorManager = LL_SpectatorManager.GetInstance();
		if (spectatorManager && spectatorManager.IsSpectating())
		{
			ClearEffects();
			return;
		}

		super.UpdateEffect(timeSlice);
	}
}