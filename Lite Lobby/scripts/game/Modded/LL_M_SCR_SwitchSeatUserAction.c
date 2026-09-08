// Qualification gate on the in-vehicle seat switch. SCR_SwitchSeatAction replaces
// CanBePerformedScript without calling super, so the gate on SCR_GetInUserAction never
// runs for it; a passenger could switch straight into the gunner seat. super here is the
// stock switch-seat implementation, so its own refusals keep priority.
modded class SCR_SwitchSeatAction
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