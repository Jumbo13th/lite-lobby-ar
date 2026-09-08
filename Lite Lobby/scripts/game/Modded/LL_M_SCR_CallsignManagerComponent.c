// Exposes the protected reserve so a designer-fixed callsign (LL_GroupCallsign) is
// removed from the pool and auto-assigned squads cannot land on it.
modded class SCR_CallsignManagerComponent
{
	void LL_ReserveGroupCallsign(Faction faction, int companyIndex, int platoonIndex, int squadIndex)
	{
		RemoveAvailableGroupCallsign(faction, companyIndex, platoonIndex, squadIndex);
	}
}