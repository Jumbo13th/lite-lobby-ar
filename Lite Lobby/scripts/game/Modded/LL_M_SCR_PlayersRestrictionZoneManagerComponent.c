// Read access to the vanilla restriction-zone set, which is protected. Returns a copy so
// the game mode can unregister zones while iterating when freeze time ends.

modded class SCR_PlayersRestrictionZoneManagerComponent
{
	array<SCR_EditorRestrictionZoneEntity> LL_GetZones()
	{
		array<SCR_EditorRestrictionZoneEntity> result = {};
		foreach (SCR_EditorRestrictionZoneEntity zone : m_aRestrictionZones)
			result.Insert(zone);
		return result;
	}
}