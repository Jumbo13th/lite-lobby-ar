// Qualification gate on hull and turret entry; the turret action defers to super. The
// seat-switch action replaces CanBePerformedScript without calling super and is gated in
// LL_M_SCR_SwitchSeatUserAction. After super so vanilla's more informative refusals keep
// the prompt when they apply.
modded class SCR_GetInUserAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		LL_VehicleAccess access = LL_VehicleAccess.GetInstance();
		if (!access)
			return true;

		LocalizedString reason;
		if (access.CanUseCompartment(user, GetCompartmentSlot(), reason))
			return true;

		SetCannotPerformReason(reason);
		return false;
	}
}