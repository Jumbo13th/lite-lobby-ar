// OnCreateMarker creates a dynamic marker's widget on every map open and OnMapClosed only
// unsubscribes; vanilla gets away with it because closing the map destroys the widget
// tree, but the spectator map is a layer of a menu that only hides, so copies pile up.
// Nulling m_wRoot also lets OnUpdateVisibility rebuild a marker vanilla's OnDelete left
// hidden. LL_MapMarkersUI.LL_DropMarkerWidgets does the same for static markers, whose
// class carries RPC codec methods and must not be modded.

modded class SCR_MapMarkerEntity
{
	override void OnCreateMarker()
	{
		LL_DropWidget();
		super.OnCreateMarker();
	}

	override protected void OnMapClosed(MapConfiguration config)
	{
		super.OnMapClosed(config);
		LL_DropWidget();
	}

	override void OnDelete()
	{
		super.OnDelete();
		LL_DropWidget();
	}

	protected void LL_DropWidget()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		m_MarkerWidgetComp = null;
	}
}