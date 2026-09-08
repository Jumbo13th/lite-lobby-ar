// Mission-side customisation of the insert-marker dialog's icons and colours, as
// attributes on the game-mode prefab. The modded marker manager swaps the config's
// PLACED_CUSTOM entry for the one built here; an absent component or an empty list
// keeps Configs/Map/LL_MapMarkerConfig.conf's part. Every machine builds the identical
// entry, so replicated marker indices stay valid.

class LL_PlacedMarkersClass : SCR_BaseGameModeComponentClass
{
}

class LL_PlacedMarkers : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.Object, "Marker TYPES players can choose when placing a map marker (double-click on the map). Each entry = one icon in the Type dropdown. Leave the list EMPTY to use the addon's default set.", category: "Placed Markers")]
	protected ref array<ref LL_MarkerIconEntry> m_aMarkerIcons;

	[Attribute("", UIWidgets.Object, "COLORS players can choose when placing a map marker. Each entry = one row in the Color dropdown; any icon can be placed in any of these colors. Leave the list EMPTY to use the addon's default palette.", category: "Placed Markers")]
	protected ref array<ref LL_MarkerColorEntry> m_aMarkerColors;

	//! Injects the attributes into the conf-loaded entry and returns it, or null when
	//! there is nothing to override. The conf instance is mutated rather than a new one
	//! created: script `new` does not apply [Attribute] defaults, so a fresh entry has no
	//! marker layout and the marker update loop crashes.
	SCR_MapMarkerEntryPlaced BuildPlacedEntry(SCR_MapMarkerEntryPlaced confEntry)
	{
		LL_MapMarkerEntryPlaced entry = LL_MapMarkerEntryPlaced.Cast(confEntry);
		if (!entry)
			return null;

		bool changed;

		if (m_aMarkerIcons && !m_aMarkerIcons.IsEmpty())
		{
			array<ref SCR_MarkerColorEntry> colors;
			if (m_aMarkerColors && !m_aMarkerColors.IsEmpty())
			{
				colors = {};
				foreach (LL_MarkerColorEntry color : m_aMarkerColors)
					colors.Insert(color);
			}
			else
			{
				colors = entry.GetColorEntries();
			}

			entry.LL_Init(m_aMarkerIcons, colors);
			changed = true;
		}
		else if (m_aMarkerColors && !m_aMarkerColors.IsEmpty())
		{
			array<ref LL_MarkerIconEntry> confIcons = entry.GetLLIcons();
			if (confIcons)
			{
				array<ref SCR_MarkerColorEntry> colors = {};
				foreach (LL_MarkerColorEntry color : m_aMarkerColors)
					colors.Insert(color);

				entry.LL_Init(confIcons, colors);
				changed = true;
			}
		}

		if (!changed)
			return null;

		return entry;
	}
}

//! Icons come from its own lean LL_MarkerIconEntry list, served to every stock consumer
//! through the virtual GetIconEntry. The inherited icon list stays empty; its only
//! callers are the stock radial and editor, both replaced here, and all null-guard.
[BaseContainerProps(), SCR_MapMarkerTitle()]
class LL_MapMarkerEntryPlaced : SCR_MapMarkerEntryPlaced
{
	[Attribute("", UIWidgets.Object, desc: "Icons players can pick in the insert-marker dialog (used instead of the stock icon list).")]
	protected ref array<ref LL_MarkerIconEntry> m_aIcons;

	array<ref LL_MarkerIconEntry> GetLLIcons()
	{
		return m_aIcons;
	}

	void LL_Init(notnull array<ref LL_MarkerIconEntry> icons, notnull array<ref SCR_MarkerColorEntry> colors)
	{
		m_aIcons = icons;
		m_aPlacedMarkerColors = colors;
	}

	//! No glow: the icon set has no glow variants.
	override bool GetIconEntry(int i, out ResourceName imageset, out ResourceName imagesetGlow, out string imageQuad)
	{
		if (!m_aIcons || !m_aIcons.IsIndexValid(i))
			return false;

		imagesetGlow = ResourceName.Empty;
		m_aIcons[i].GetIconResource(imageset, imageQuad);
		return true;
	}

	override void InitClientSettings(SCR_MapMarkerBase marker, SCR_MapMarkerWidgetComponent widgetComp, bool skipProfanityFilter = false)
	{
		super.InitClientSettings(marker, widgetComp, skipProfanityFilter);

		// Each icon carries its own pixel size and ink fraction, which decide where its label sits.
		if (m_aIcons && m_aIcons.IsIndexValid(marker.GetIconEntry()))
		{
			LL_MarkerIconEntry icon = m_aIcons[marker.GetIconEntry()];
			widgetComp.LL_SetIconGeometry(icon.GetIconSize(), icon.GetLabelInk());
		}
	}
}