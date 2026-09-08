// A dead player keeps controlling the corpse, and the death blur is a world post-process
// that also covers the spectator camera. Keep the spectator view sharp; gameplay blur
// resumes once the player controls a living character.

modded class SCR_DepthOfFieldEffect
{
	override void UpdateEffect(float timeSlice)
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