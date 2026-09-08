// Clickable squad header: left-click folds, right-click opens the admin group menu.
// SCR_ButtonBaseComponent raises m_OnClicked for the left button only, so the right
// click is caught in OnClick.

class LL_GroupHeaderButton : SCR_ButtonBaseComponent
{
	protected LL_RolesGroup m_RolesGroup;

	void SetRolesGroup(LL_RolesGroup group)
	{
		m_RolesGroup = group;
	}

	// super raises m_OnClicked for the left button.
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);

		if (button == 1 && m_RolesGroup)
			m_RolesGroup.OpenGroupContextMenu();

		return false;
	}
}