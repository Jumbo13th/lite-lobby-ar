// A seated driver could otherwise release the brake the hold set and roll on a slope;
// the stock action is a user action, which the local input lock does not cover.
modded class SCR_ToggleHandbrakeAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (LL_GameModeCoop.IsHardFreezeActive())
		{
			SetCannotPerformReason("#LL-HardFreeze_Title");
			return false;
		}

		return super.CanBePerformedScript(user);
	}
}
