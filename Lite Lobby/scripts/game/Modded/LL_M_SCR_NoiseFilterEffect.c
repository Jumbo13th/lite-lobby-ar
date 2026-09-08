// A dead player keeps controlling the corpse, and the death event sets the shared audio
// variable "CharacterLifeState" that drives the muffled low-pass; nothing clears it
// because the controlled entity never changes. Pinning ALIVE each frame while spectating
// is what vanilla writes when the respawn menu opens; the event handlers take the
// variable back once the player controls a living character.

modded class SCR_NoiseFilterEffect
{
	override void UpdateEffect(float timeSlice)
	{
		LL_SpectatorManager spectatorManager = LL_SpectatorManager.GetInstance();
		if (spectatorManager && spectatorManager.IsSpectating())
		{
			AudioSystem.SetVariableByName("CharacterLifeState", ECharacterLifeState.ALIVE,
				"{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf");
			return;
		}

		super.UpdateEffect(timeSlice);
	}
}