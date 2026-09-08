// The radial and the spectator's "jump camera here" share the map context click; a
// spectator owns no slot the server would accept a marker from, so the radial stays off.

modded class SCR_MapRadialUI
{
	override protected void OnInputMenuOpen(float value, EActionTrigger reason)
	{
		if (LL_SpectatorMenu.IsMapOpen())
			return;

		super.OnInputMenuOpen(value, reason);
	}
}