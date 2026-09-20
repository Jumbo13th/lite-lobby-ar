// Nobody leaves a seat during a hard freeze: the stock get-out checks know nothing about
// the hold, and a body climbing out of a pinned aircraft would fall. Entry and seat
// changes are refused in LL_VehicleAccess.CanUseCompartment, which every seat action asks.
modded class SCR_GetOutAction
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

modded class SCR_JumpOutAction
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
