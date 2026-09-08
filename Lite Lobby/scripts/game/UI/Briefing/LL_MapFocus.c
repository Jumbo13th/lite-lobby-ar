// Pans and zooms the open map to a world entity or position. Driven by clickable rows:
// the in-game RichTextWidget does not support clickable links.

class LL_MapFocus
{
	// Pixels per metre the map eases to; 2.5 gives a tactical view. Clamped to the map's range.
	protected static const float FOCUS_ZOOM_PPU = 2.5;
	protected static const float FOCUS_TIME_S = 0.4;

	// Uses the entity's current position; ToPos takes a fixed one.
	static void To(string entityName)
	{
		if (entityName == "")
			return;

		IEntity ent = GetGame().GetWorld().FindEntityByName(entityName);
		if (!ent)
		{
			Print(string.Format("[LL_MapFocus] No world entity named '%1'.", entityName), LogLevel.WARNING);
			return;
		}

		vector pos = ent.GetOrigin();
		ToPos(pos[0], pos[2]);
	}

	// The squad roster links each squad to its spawn snapshot, not its live position.
	static void ToPos(float worldX, float worldZ)
	{
		SCR_MapEntity mapEnt = SCR_MapEntity.GetMapInstance();
		if (!mapEnt || !mapEnt.IsOpen())
			return;

		float zoom = Math.Clamp(FOCUS_ZOOM_PPU, mapEnt.GetMinZoom(), mapEnt.GetMaxZoom());
		mapEnt.ZoomPanSmooth(zoom, worldX, worldZ, FOCUS_TIME_S);
	}
}