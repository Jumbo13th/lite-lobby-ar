//! Markers, mission-maker markers, map areas and trigger zones. Markers are found by
//! comparing the server's marker lists once a second, which also catches expiries and
//! server-side removals that pass through no hook.
class LL_ReplayMap : Managed
{
	protected LL_ReplayWriter m_Writer;
	protected LL_ReplayEntities m_Entities;

	protected ref map<int, string> m_mMarkerLines = new map<int, string>();
	protected ref array<string> m_aManualLines = {};
	protected ref array<LL_MapZoneComponent> m_aAreas = {};
	protected ref array<string> m_aAreaLines = {};
	protected ref array<bool> m_aAreaShown = {};
	protected ref array<LL_TriggerZoneBase> m_aZones = {};
	protected ref array<string> m_aZoneLines = {};
	protected ref array<string> m_aZoneHolders = {};

	void Init(LL_ReplayWriter writer, LL_ReplayEntities entities)
	{
		m_Writer = writer;
		m_Entities = entities;
	}

	//! Everything found here goes out with the first snapshot. A mission-maker marker hidden
	//! during the game phase is left out, since the recording covers only that phase.
	void Start_S()
	{
		m_aManualLines.Clear();
		array<LL_ManualMarker> manualMarkers = LL_ManualMarker.GetAll();
		if (manualMarkers)
		{
			int manualIndex = 1;
			foreach (LL_ManualMarker manual : manualMarkers)
			{
				if (!manual || manual.IsHiddenInState(SCR_EGameModeState.GAME))
					continue;

				m_aManualLines.Insert(ManualMarkerLine(manual, manualIndex));
				manualIndex++;
			}
		}

		m_aAreas.Clear();
		m_aAreaLines.Clear();
		m_aAreaShown.Clear();
		array<LL_MapZoneComponent> areas = LL_MapZoneComponent.GetAll();
		if (areas)
		{
			foreach (LL_MapZoneComponent area : areas)
			{
				if (!area)
					continue;

				m_aAreas.Insert(area);
				m_aAreaLines.Insert(AreaLine(area, m_aAreas.Count()));
				m_aAreaShown.Insert(area.IsShownNow());
			}
		}

		m_aZones.Clear();
		m_aZoneLines.Clear();
		m_aZoneHolders.Clear();
		array<LL_TriggerComponent> triggers = LL_TriggerComponent.GetAll();
		if (triggers)
		{
			foreach (LL_TriggerComponent trigger : triggers)
			{
				LL_TriggerZoneBase zone = LL_TriggerZoneBase.Cast(trigger);
				if (!zone)
					continue;

				m_aZones.Insert(zone);
				m_aZoneLines.Insert(ZoneLine(zone, m_aZones.Count()));
				m_aZoneHolders.Insert(zone.GetZoneHolderFaction());
			}
		}

		m_mMarkerLines.Clear();
		CollectMarkers_S(null);
	}

	void SecondStep_S()
	{
		CollectMarkers_S(m_Writer);

		int count = m_aAreas.Count();
		for (int i = 0; i < count; i++)
		{
			LL_MapZoneComponent area = m_aAreas[i];
			if (!area)
				continue;

			bool shown = area.IsShownNow();
			if (shown == m_aAreaShown[i])
				continue;

			m_aAreaShown[i] = shown;
			m_Writer.Add(AreaVisibilityLine(i + 1, shown));
		}
	}

	void WriteSnapshot_S()
	{
		foreach (int markerId, string markerLine : m_mMarkerLines)
		{
			m_Writer.AddSnapshotLine(markerLine);
		}

		foreach (string manualLine : m_aManualLines)
		{
			m_Writer.AddSnapshotLine(manualLine);
		}

		int areaCount = m_aAreaLines.Count();
		for (int i = 0; i < areaCount; i++)
		{
			m_Writer.AddSnapshotLine(m_aAreaLines[i]);
			m_Writer.AddSnapshotLine(AreaVisibilityLine(i + 1, m_aAreaShown[i]));
		}

		int zoneCount = m_aZoneLines.Count();
		for (int z = 0; z < zoneCount; z++)
		{
			m_Writer.AddSnapshotLine(m_aZoneLines[z]);
			m_Writer.AddSnapshotLine(string.Format("ZH %1 %2", z + 1, LL_ReplayWriter.Tok(m_aZoneHolders[z])));
		}
	}

	void RecordZoneCaptured_S(LL_TriggerZoneBase zone, string side)
	{
		int index = m_aZones.Find(zone);
		if (index < 0)
			return;

		m_Writer.Add(string.Format("ZC %1 %2", index + 1, LL_ReplayWriter.Tok(side)));
	}

	void RecordZoneHolder_S(LL_TriggerZoneBase zone, string holder)
	{
		int index = m_aZones.Find(zone);
		if (index < 0)
			return;

		m_aZoneHolders[index] = holder;
		m_Writer.Add(string.Format("ZH %1 %2", index + 1, LL_ReplayWriter.Tok(holder)));
	}

	//! With a writer, new markers are written as placed and missing ones as removed; without
	//! one, the current markers are only remembered.
	protected void CollectMarkers_S(LL_ReplayWriter writer)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
			return;

		SCR_MapMarkerEntryPlaced placed;
		SCR_MapMarkerConfig config = manager.GetMarkerConfig();
		if (config)
			placed = SCR_MapMarkerEntryPlaced.Cast(config.GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));

		set<int> present = new set<int>();
		CollectFrom(manager.GetStaticMarkers(), placed, present, writer);
		CollectFrom(manager.GetDisabledMarkers(), placed, present, writer);

		array<int> gone = {};
		foreach (int markerId, string markerLine : m_mMarkerLines)
		{
			if (!present.Contains(markerId))
				gone.Insert(markerId);
		}

		foreach (int goneId : gone)
		{
			m_mMarkerLines.Remove(goneId);
			if (writer)
				writer.Add(string.Format("M- g%1", goneId));
		}
	}

	protected void CollectFrom(array<SCR_MapMarkerBase> markers, SCR_MapMarkerEntryPlaced placed, notnull set<int> present, LL_ReplayWriter writer)
	{
		if (!markers)
			return;

		foreach (SCR_MapMarkerBase marker : markers)
		{
			if (!marker)
				continue;

			int markerId = marker.GetMarkerID();
			if (markerId == -1)
				continue;

			present.Insert(markerId);
			if (m_mMarkerLines.Contains(markerId))
				continue;

			string markerLine = MarkerLine(marker, placed);
			m_mMarkerLines.Set(markerId, markerLine);
			if (writer)
				writer.Add(markerLine);
		}
	}

	protected string MarkerLine(notnull SCR_MapMarkerBase marker, SCR_MapMarkerEntryPlaced placed)
	{
		int position[2];
		marker.GetWorldPos(position);

		string type = typename.EnumToString(SCR_EMapMarkerType, marker.GetType());
		type.ToLower();

		int author = m_Entities.PrefOfPlayer(marker.GetMarkerOwnerID());

		string icon = "icon" + marker.GetIconEntry();
		string colour = "-";
		if (placed && marker.GetType() == SCR_EMapMarkerType.PLACED_CUSTOM)
		{
			ResourceName imageset;
			ResourceName imagesetGlow;
			string quad;
			if (placed.GetIconEntry(marker.GetIconEntry(), imageset, imagesetGlow, quad))
				icon = quad;

			Color color = placed.GetColorEntry(marker.GetColorEntry());
			if (color)
				colour = LL_ReplayWriter.ColorHex(color, false);
		}

		string line = string.Format("M+ g%1 %2 %3 %4 %5 %6 %7 %8 %9", marker.GetMarkerID(), LL_ReplayWriter.Tok(type), position[0], position[1],
			marker.GetRotation(), LL_ReplayWriter.IntOr(author, author > 0), SidesFromFlags(marker.GetMarkerFactionFlags()),
			LL_ReplayWriter.Str(icon), colour);
		return line + " " + LL_ReplayWriter.Str(marker.GetCustomText());
	}

	protected static string ManualMarkerLine(notnull LL_ManualMarker manual, int index)
	{
		vector origin = manual.GetOrigin();
		vector angles = manual.GetYawPitchRoll();
		int rotation = Math.Round(angles[0]) - 90;
		rotation = ((rotation % 360) + 360) % 360;
		int x = Math.Round(origin[0]);
		int z = Math.Round(origin[2]);

		string line = string.Format("M+ n%1 manual %2 %3 %4 - %5 %6 %7", index, x, z, rotation,
			SidesFromList(manual.IsShownForAnyFaction(), manual.GetVisibleFactions()), LL_ReplayWriter.Str(manual.GetQuadName()),
			LL_ReplayWriter.ColorHex(manual.GetMarkerColor(), false));
		return line + " " + LL_ReplayWriter.Str(manual.GetDescription());
	}

	protected static string AreaLine(notnull LL_MapZoneComponent area, int aid)
	{
		int width = Math.Round(area.GetOutlineWidth());
		int lineMode = 0;
		if (area.IsLineMode())
			lineMode = 1;

		string line = string.Format("A %1 %2 %3 %4 %5 %6 %7", aid, SidesFromList(area.IsShownForAnyFaction(), area.GetVisibleFactions()),
			LL_ReplayWriter.ColorHex(area.GetFillColor(), true), LL_ReplayWriter.ColorHex(area.GetOutlineColor(), true), width, lineMode,
			LL_ReplayWriter.Str(area.GetLabel()));

		array<vector> points = {};
		area.GetDrawnPolygon(points);
		foreach (vector point : points)
		{
			int x = Math.Round(point[0]);
			int z = Math.Round(point[2]);
			line += string.Format(" %1 %2", x, z);
		}

		return line;
	}

	protected static string AreaVisibilityLine(int aid, bool shown)
	{
		if (shown)
			return string.Format("AV %1 1", aid);

		return string.Format("AV %1 0", aid);
	}

	protected static string ZoneLine(notnull LL_TriggerZoneBase zone, int zid)
	{
		string line = string.Format("Z %1 %2 %3 %4", zid, LL_ReplayWriter.Tok(zone.GetAttackerFaction()),
			LL_ReplayWriter.Tok(zone.GetDefenderFaction()), LL_ReplayWriter.Str(zone.GetZoneStatLabel()));

		array<float> polygon = {};
		zone.GetZoneOutline(polygon);
		int count = polygon.Count();
		for (int i = 0; i + 1 < count; i += 2)
		{
			int x = Math.Round(polygon[i]);
			int z = Math.Round(polygon[i + 1]);
			line += string.Format(" %1 %2", x, z);
		}

		return line;
	}

	protected static string SidesFromList(bool anyFaction, array<FactionKey> factions)
	{
		if (anyFaction || !factions || factions.IsEmpty())
			return "*";

		string sides;
		foreach (FactionKey faction : factions)
		{
			if (faction == "")
				continue;

			if (sides != "")
				sides += ",";

			sides += faction;
		}

		if (sides == "")
			return "*";

		return LL_ReplayWriter.Tok(sides);
	}

	protected static string SidesFromFlags(int flags)
	{
		if (flags == 0)
			return "*";

		FactionManager factions = GetGame().GetFactionManager();
		if (!factions)
			return "*";

		string sides;
		for (int i = 0; i < 31; i++)
		{
			if ((flags & (1 << i)) == 0)
				continue;

			Faction faction = factions.GetFactionByIndex(i);
			if (!faction)
				continue;

			if (sides != "")
				sides += ",";

			sides += faction.GetFactionKey();
		}

		if (sides == "")
			return "*";

		return LL_ReplayWriter.Tok(sides);
	}
}
