// Vehicle row under a squad: faction colour, icon and name, display only. A button base
// because plain widgets do not reliably receive mouse enter/leave for the hover preview.
class LL_VehicleSelector : SCR_ButtonBaseComponent
{
	protected ImageWidget m_wFactionColor;
	protected ImageWidget m_wIcon;
	protected TextWidget m_wName;

	protected int m_iVehicleRplId = -1;
	protected LL_CoopLobby m_CoopLobby;

	void Init(LL_VehicleData vehicle, LL_CoopLobby lobby)
	{
		if (!vehicle)
			return;

		m_iVehicleRplId = vehicle.m_iRplId;
		m_CoopLobby = lobby;

		Widget root = GetRootWidget();
		if (!root)
			return;

		m_wFactionColor = ImageWidget.Cast(root.FindAnyWidget("VehicleFactionColor"));
		m_wIcon = ImageWidget.Cast(root.FindAnyWidget("VehicleIcon"));
		m_wName = TextWidget.Cast(root.FindAnyWidget("VehicleClassName"));

		if (m_wName)
			m_wName.SetText(vehicle.m_sName);

		if (m_wIcon)
		{
			if (vehicle.m_sIconPath != "")
				m_wIcon.LoadImageTexture(0, vehicle.m_sIconPath);
			else
				m_wIcon.SetVisible(false);
		}

		if (m_wFactionColor)
		{
			SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (fm)
			{
				SCR_Faction faction = SCR_Faction.Cast(fm.GetFactionByKey(vehicle.m_sFactionKey));
				if (faction)
					m_wFactionColor.SetColor(faction.GetFactionColor());
			}
		}
	}

	// Hover drives the shared preview pane, as the character rows do.
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		super.OnMouseEnter(w, x, y);

		if (m_CoopLobby)
			m_CoopLobby.SetPreviewVehicle(m_iVehicleRplId);

		return false;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		super.OnMouseLeave(w, enterW, x, y);

		if (m_CoopLobby)
			m_CoopLobby.ClearPreview();

		return false;
	}
}