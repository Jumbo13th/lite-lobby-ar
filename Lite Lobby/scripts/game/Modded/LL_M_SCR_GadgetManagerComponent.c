// Refuses gadget keys during a hard freeze (the disable flags do not cover them and the
// freeze has no menu), and the map key while the player holds an object marked "block
// map while held": raising the map stows whatever is in the hands, which would pocket
// an object meant to stay in the world. The other gadget keys still stow deliberately.

modded class SCR_GadgetManagerComponent
{
	override protected void OnGadgetInput(float value, EActionTrigger reason)
	{
		if (LL_GameModeCoop.IsHardFreezeActive())
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}

		if (GetGadgetInputAction() == EGadgetType.MAP && LL_TrackerComponent.BlocksMapWhileHeld(GetHeldGadget()))
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}

		super.OnGadgetInput(value, reason);
	}
};