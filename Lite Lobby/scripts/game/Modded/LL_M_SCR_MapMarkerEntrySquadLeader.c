// Drops the "Focus Current Squad" toolbar button. Vanilla registers it in OnMapInit and
// nothing else; the squad-leader marker logic lives elsewhere and keeps working. No
// super call: it is the registration being dropped. The decorator and base are repeated
// because a bare modded BaseContainerProps class loads as "Unknown class" and truncates
// the marker list.
[BaseContainerProps(), SCR_MapMarkerTitle()]
modded class SCR_MapMarkerEntrySquadLeader : SCR_MapMarkerEntryDynamic
{
	override void OnMapInit(notnull SCR_MapEntity mapEnt, notnull SCR_MapMarkersUI markerUIComp)
	{
	}
}