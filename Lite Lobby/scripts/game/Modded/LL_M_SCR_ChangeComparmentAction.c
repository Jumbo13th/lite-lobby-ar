// Qualification gate on the seat-switch action. Stock seat switching consults no lock
// at all, so gating entry alone leaves the cargo-bay-then-driver-seat route open. Gated in
// CanBePerformed rather than CanBeShown so the player is told why. The class name repeats
// vanilla's spelling.
modded class SCR_ChangeComparmentAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		LL_VehicleAccess access = LL_VehicleAccess.GetInstance();
		if (!access)
			return true;

		// The seat this action moves into, resolved by CanBeShownScript, which runs first.
		LocalizedString reason;
		if (access.CanUseCompartment(user, m_NonOccupiedSlot, reason))
			return true;

		SetCannotPerformReason(reason);
		return false;
	}
}