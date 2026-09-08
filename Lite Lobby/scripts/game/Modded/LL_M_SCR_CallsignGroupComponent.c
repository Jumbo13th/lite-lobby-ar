// Applies a designer-fixed callsign (LL_GroupCallsign) at the vanilla assign point, so
// the name is right from the moment it exists: the replicated slot group name reads it
// once and the freed-on-destroy bookkeeping stays vanilla. The indices are reserved from
// the pool first so other squads auto-assign around them.
modded class SCR_CallsignGroupComponent
{
	override protected void AssignGroupCallsign()
	{
		if (m_Group && m_Faction && m_CallsignManager)
		{
			LL_GroupCallsign designer = LL_GroupCallsign.Cast(m_Group.FindComponent(LL_GroupCallsign));
			if (designer && designer.HasCallsign())
			{
				int company, platoon, squad;
				designer.GetCallsign(company, platoon, squad);

				m_CallsignManager.LL_ReserveGroupCallsign(m_Faction, company, platoon, squad);
				DoAssignCallsign(company, platoon, squad);
				return;
			}
		}

		super.AssignGroupCallsign();
	}
}