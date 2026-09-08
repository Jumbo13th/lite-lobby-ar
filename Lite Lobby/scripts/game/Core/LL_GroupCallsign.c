// Designer-set squad callsign: company/platoon/squad indices into the faction's callsign
// name lists. Applied server-side by LL_M_SCR_CallsignGroupComponent and reserved from
// the pool; the lobby reads the resulting name through the replicated slot group name.
[ComponentEditorProps(category: "Lite Lobby", description: "Fixed squad callsign (company/platoon/squad indices) shown in the lobby. -1 = auto.")]
class LL_GroupCallsignClass : ScriptComponentClass
{
}

class LL_GroupCallsign : ScriptComponent
{
	[Attribute("-1", UIWidgets.EditBox, "Company callsign index (-1 = auto-assign this group).", category: "Lite Lobby")]
	protected int m_iCompany;

	[Attribute("-1", UIWidgets.EditBox, "Platoon callsign index (-1 = auto-assign this group).", category: "Lite Lobby")]
	protected int m_iPlatoon;

	[Attribute("-1", UIWidgets.EditBox, "Squad callsign index (-1 = auto-assign this group).", category: "Lite Lobby")]
	protected int m_iSquad;

	// A partial set would produce a malformed name, so it falls back to auto.
	bool HasCallsign()
	{
		return m_iCompany >= 0 && m_iPlatoon >= 0 && m_iSquad >= 0;
	}

	void GetCallsign(out int company, out int platoon, out int squad)
	{
		company = m_iCompany;
		platoon = m_iPlatoon;
		squad = m_iSquad;
	}
}