//! Keep the reference override in Game: methods are sealed when that module links.
#ifdef WORKBENCH
class LL_MapPaintFullScanDocument : LL_MapPaintDocument
{
	//! Reference broad phase: run the same exact cutting against every segment.
	override void QuerySpatialPath(array<vector> path, float radius, array<ref LL_MapPaintSpatialSegment> results)
	{
		results.Clear();
		foreach (LL_MapPaintStroke stroke : m_Strokes)
		{
			int first = 1;
			if (stroke.m_Points.Count() == 1)
				first = 0;
			for (int i = first; i < stroke.m_Points.Count(); i++)
			{
				LL_MapPaintSpatialSegment segment = new LL_MapPaintSpatialSegment();
				segment.m_Stroke = stroke;
				segment.m_iSegmentIndex = i;
				results.Insert(segment);
			}
		}
	}
}

class LL_MapPaintIndexProbe : LL_MapPaintDocument
{
	LL_MapPaintSpatialSegment GetIndexedSegment(LL_MapPaintStroke stroke, int index)
	{
		LL_MapPaintSpatialStroke indexed = m_SpatialStrokes.Get(stroke.m_iSpatialId);
		if (!indexed)
			return null;
		return indexed.m_Segments.Get(index);
	}

	int GetIndexedStrokeCount() { return m_SpatialStrokes.Count(); }
	int GetSpatialSectorCount() { return m_SpatialSectors.Count(); }
	int GetSpatialCellCount()
	{
		int count;
		foreach (string key, LL_MapPaintSpatialSector sector : m_SpatialSectors)
			count += sector.m_Cells.Count();
		return count;
	}
}

class LL_MapPaintTooltipProbe : LL_MapPaintAuthorTooltip
{
	LL_MapPaintStroke Pick(LL_MapPaintDocument document, vector position, float tolerance = 0.1)
	{
		return FindStrokeAtPosition(document, position, tolerance);
	}
}
#endif
