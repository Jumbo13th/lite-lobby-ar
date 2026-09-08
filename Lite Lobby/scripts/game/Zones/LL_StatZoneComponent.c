// Marks a map area as a statistics objective zone: kills whose victim fell inside earn
// the config frag multiplier. The victim's position, not the killer's, so defenders
// shooting out and attackers shooting in both qualify. Zero-config: add it to the
// polygon shape a zone trigger or restriction zone already uses. Server-only and
// event-driven.

class LL_StatZoneComponentClass : ScriptComponentClass
{
}

class LL_StatZoneComponent : ScriptComponent
{
	protected ref array<float> m_aPolygon2D = {};
	protected bool m_bPolygonBuilt;

	// Server-side registry; world-guarded because statics survive a scenario restart.
	protected static ref array<LL_StatZoneComponent> s_aZones = {};
	protected static BaseWorld s_RegistryWorld;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		BaseWorld world = owner.GetWorld();
		if (world != s_RegistryWorld)
		{
			s_aZones.Clear();
			s_RegistryWorld = world;
		}

		if (!s_aZones.Contains(this))
			s_aZones.Insert(this);
	}

	void ~LL_StatZoneComponent()
	{
		if (s_aZones)
			s_aZones.RemoveItem(this);
	}

	static float GetZoneMultiplierAt(vector pos, float configMultiplier, out string zoneName)
	{
		zoneName = "";

		foreach (LL_StatZoneComponent zone : s_aZones)
		{
			if (!zone || !zone.IsInside(pos))
				continue;

			zoneName = zone.GetZoneDisplayName();
			return configMultiplier;
		}

		return 0;
	}

	protected bool IsInside(vector pos)
	{
		// Built lazily on the first kill, so init order is irrelevant.
		if (!m_bPolygonBuilt)
		{
			m_bPolygonBuilt = true;
			LL_TriggerComponent.BuildPolygon("", GetOwner(), m_aPolygon2D);
			if (m_aPolygon2D.Count() < 6)
				Print("[LL_Lobby] Stats: stat zone owner is not a polygon shape (or has under 3 points) — zone inert.", LogLevel.WARNING);
		}

		if (m_aPolygon2D.Count() < 6)
			return false;

		return LL_TriggerComponent.PointInPolygon(pos, m_aPolygon2D);
	}

	protected string GetZoneDisplayName()
	{
		string ownName = GetOwner().GetName();
		if (ownName != "")
			return ownName;
		return "zone";
	}
}