class LL_MapPaintStroke
{
	ref array<vector> m_Points = {};
	float m_fWidth;
	bool m_bAnchorLine;
	int m_iColor;
	int m_iOwner = -1;
	int m_iSpatialId;
	int m_iDrawOrder;

	float GetScreenWidth(float zoom, float brushScreenWidth)
	{
		if (m_bAnchorLine)
			return m_fWidth * Math.Max(zoom, 0); // Width stays fixed in map metres.
		return brushScreenWidth;
	}

	LL_MapPaintStroke MakeFragment()
	{
		LL_MapPaintStroke fragment = new LL_MapPaintStroke();
		fragment.m_fWidth = m_fWidth;
		fragment.m_bAnchorLine = m_bAnchorLine;
		fragment.m_iColor = m_iColor;
		fragment.m_iOwner = m_iOwner;
		fragment.m_iDrawOrder = m_iDrawOrder;
		return fragment;
	}
}

class LL_MapPaintSpatialSegment
{
	ref LL_MapPaintStroke m_Stroke;
	int m_iSegmentIndex;
	int m_iQueryStamp;
	ref map<string, bool> m_SpatialCells = new map<string, bool>();
	ref array<int> m_CellCoordinates = {}; // X/Z pairs for direct removal from cells.
}

class LL_MapPaintSpatialStroke
{
	ref map<int, ref LL_MapPaintSpatialSegment> m_Segments = new map<int, ref LL_MapPaintSpatialSegment>();
}

class LL_MapPaintStrokeBucket
{
	ref array<LL_MapPaintStroke> m_Strokes = {};
}

class LL_MapPaintSpatialCell
{
	ref array<ref LL_MapPaintSpatialSegment> m_Segments = {};
}

class LL_MapPaintSpatialSector
{
	int m_iX;
	int m_iZ;
	ref map<int, ref LL_MapPaintSpatialCell> m_Cells = new map<int, ref LL_MapPaintSpatialCell>();
}

class LL_MapPaintEraseCandidate
{
	ref map<int, bool> m_Segments = new map<int, bool>();
}

//! Annotation model. The game mode's marker manager owns the shared server copy.
class LL_MapPaintDocument
{
	static const int MAX_POINTS = 8192;
	static const int MAX_STORED_POINTS = 16384; // Erasing can introduce cut endpoints.
	static const int SPATIAL_SECTOR_COUNT = 10;
	static const int SPATIAL_SUBDIVISIONS = 5;
	static const int SPATIAL_LEAF_COUNT = SPATIAL_SECTOR_COUNT * SPATIAL_SUBDIVISIONS;
	static const float DEFAULT_SPATIAL_LEAF_SIZE = 256;
	ref array<ref LL_MapPaintStroke> m_Strokes = {};
	protected int m_iPointCount;
	protected int m_iVersion;
	protected int m_iNextSpatialId = 1;
	protected int m_iNextDrawOrder = 1;
	protected int m_iQueryStamp = 1;
	protected ref map<string, ref LL_MapPaintSpatialSector> m_SpatialSectors = new map<string, ref LL_MapPaintSpatialSector>();
	protected ref map<int, ref LL_MapPaintSpatialStroke> m_SpatialStrokes = new map<int, ref LL_MapPaintSpatialStroke>();
	protected bool m_bSpatialBoundsConfigured;
	protected float m_fSpatialOriginX;
	protected float m_fSpatialOriginZ;
	protected float m_fSpatialWidth;
	protected float m_fSpatialHeight;
	protected float m_fSpatialLeafWidth;
	protected float m_fSpatialLeafHeight;

	int GetVersion() { return m_iVersion; }
	int GetPointCount() { return m_iPointCount; }

	void LL_MapPaintDocument()
	{
		// Documents also receive edits on dedicated servers and clients with closed maps.
		// Use a sparse world-space grid until the map entity supplies terrain bounds.
		float size = DEFAULT_SPATIAL_LEAF_SIZE * SPATIAL_LEAF_COUNT;
		ConfigureSpatialGrid(vector.Zero, Vector(size, 0, size));
	}

	//! Configure a 10x10 sector grid with 5x5 leaf cells in each sector.
	void ConfigureSpatialGrid(vector origin, vector size)
	{
		if (size[0] <= 0 || size[2] <= 0)
			return;
		if (m_bSpatialBoundsConfigured
			&& m_fSpatialOriginX == origin[0] && m_fSpatialOriginZ == origin[2]
			&& m_fSpatialWidth == size[0] && m_fSpatialHeight == size[2])
			return;

		m_fSpatialOriginX = origin[0];
		m_fSpatialOriginZ = origin[2];
		m_fSpatialWidth = size[0];
		m_fSpatialHeight = size[2];
		m_fSpatialLeafWidth = m_fSpatialWidth / SPATIAL_LEAF_COUNT;
		m_fSpatialLeafHeight = m_fSpatialHeight / SPATIAL_LEAF_COUNT;
		m_bSpatialBoundsConfigured = true;
		RebuildSpatialGrid();
	}

	void ReplaceFrom(LL_MapPaintDocument source)
	{
		if (!source || source == this)
			return;
		// Snapshot objects are new, but unchanged geometry can retain its existing
		// stroke and segment records. Verify full geometry after the bucket lookup.
		map<string, ref LL_MapPaintStrokeBucket> buckets = new map<string, ref LL_MapPaintStrokeBucket>();
		foreach (LL_MapPaintStroke existing : m_Strokes)
		{
			string key = StrokeLookupKey(existing);
			LL_MapPaintStrokeBucket bucket = buckets.Get(key);
			if (!bucket)
			{
				bucket = new LL_MapPaintStrokeBucket();
				buckets.Set(key, bucket);
			}
			bucket.m_Strokes.Insert(existing);
		}
		array<ref LL_MapPaintStroke> restored = {};
		map<int, bool> retained = new map<int, bool>();
		m_iPointCount = 0;
		m_iNextDrawOrder = 1;
		foreach (LL_MapPaintStroke incoming : source.m_Strokes)
		{
			LL_MapPaintStroke stroke;
			LL_MapPaintStrokeBucket bucket = buckets.Get(StrokeLookupKey(incoming));
			if (bucket)
			{
				for (int i = bucket.m_Strokes.Count() - 1; i >= 0; i--)
				{
					LL_MapPaintStroke candidate = bucket.m_Strokes[i];
					if (!SameStrokeGeometry(candidate, incoming))
						continue;
					stroke = candidate;
					bucket.m_Strokes.Remove(i);
					retained.Set(stroke.m_iSpatialId, true);
					break;
				}
			}
			if (!stroke)
			{
				stroke = incoming.MakeFragment();
				stroke.m_Points.Copy(incoming.m_Points);
			}
			stroke.m_iDrawOrder = m_iNextDrawOrder++;
			restored.Insert(stroke);
			m_iPointCount += stroke.m_Points.Count();
		}
		foreach (LL_MapPaintStroke previous : m_Strokes)
		{
			if (!retained.Get(previous.m_iSpatialId))
				UnindexStroke(previous);
		}
		m_Strokes = restored;
		foreach (LL_MapPaintStroke stroke : m_Strokes)
		{
			if (!m_SpatialStrokes.Get(stroke.m_iSpatialId))
				IndexStroke(stroke);
		}
		m_iVersion++;
	}

	protected string StrokeLookupKey(LL_MapPaintStroke stroke)
	{
		int count = stroke.m_Points.Count();
		if (count == 0)
			return string.Empty;
		return string.Format("%1|%2|%3|%4|%5|%6|%7", count, stroke.m_fWidth, stroke.m_iColor, stroke.m_iOwner, stroke.m_bAnchorLine, stroke.m_Points[0], stroke.m_Points[count - 1]);
	}

	protected bool SameStrokeGeometry(LL_MapPaintStroke a, LL_MapPaintStroke b)
	{
		if (a.m_iColor != b.m_iColor || a.m_iOwner != b.m_iOwner || a.m_fWidth != b.m_fWidth
			|| a.m_bAnchorLine != b.m_bAnchorLine || a.m_Points.Count() != b.m_Points.Count())
			return false;
		foreach (int i, vector point : a.m_Points)
		{
			if (point != b.m_Points[i])
				return false;
		}
		return true;
	}

	LL_MapPaintStroke AddStroke(array<vector> points, float width, int color, int owner = -1, bool anchorLine = false)
	{
		if (!points || points.IsEmpty() || m_iPointCount + points.Count() > MAX_POINTS)
			return null;
		LL_MapPaintStroke stroke = new LL_MapPaintStroke();
		stroke.m_fWidth = width;
		stroke.m_bAnchorLine = anchorLine;
		stroke.m_iColor = color;
		if (anchorLine)
			stroke.m_iColor = LL_MapPaintAnchorStyle.GetColor(color);
		stroke.m_iOwner = owner;
		stroke.m_Points.Copy(points);
		m_Strokes.Insert(stroke);
		m_iPointCount += points.Count();
		IndexStroke(stroke);
		m_iVersion++;
		return stroke;
	}

	LL_MapPaintStroke AddAnchorSegment(array<vector> points, float width, int color, int owner, bool continuation)
	{
		color = LL_MapPaintAnchorStyle.GetColor(color);
		if (continuation && points && points.Count() == 2)
		{
			for (int i = m_Strokes.Count() - 1; i >= 0; i--)
			{
				LL_MapPaintStroke stroke = m_Strokes[i];
				if (!stroke.m_bAnchorLine || stroke.m_iOwner != owner)
					continue;
				int count = stroke.m_Points.Count();
				if (count > 0 && stroke.m_fWidth == width && LL_MapPaintAnchorStyle.GetColor(stroke.m_iColor) == color && stroke.m_Points[count - 1] == points[0])
				{
					stroke.m_iColor = color;
					if (points[0] == points[1] || AppendPoint(stroke, points[1], 0))
						return stroke;
					return null;
				}
				break;
			}
		}
		return AddStroke(points, width, color, owner, true);
	}

	//! Pending anchor segments can extend a render copy without changing confirmed data.
	LL_MapPaintDocument CopyForPreview()
	{
		LL_MapPaintDocument copy = new LL_MapPaintDocument();
		foreach (LL_MapPaintStroke stroke : m_Strokes)
		{
			LL_MapPaintStroke cloned = stroke.MakeFragment();
			cloned.m_Points.Copy(stroke.m_Points);
			copy.m_Strokes.Insert(cloned);
		}
		copy.m_iPointCount = m_iPointCount;
		copy.m_iNextDrawOrder = m_iNextDrawOrder;
		return copy;
	}

	LL_MapPaintStroke BeginStroke(vector point, float width, int color, int owner = -1)
	{
		if (m_iPointCount >= MAX_POINTS)
			return null;

		LL_MapPaintStroke stroke = new LL_MapPaintStroke();
		stroke.m_fWidth = width;
		stroke.m_iColor = color;
		stroke.m_iOwner = owner;
		stroke.m_Points.Insert(point);
		m_Strokes.Insert(stroke);
		m_iPointCount++;
		IndexStroke(stroke);
		m_iVersion++;
		return stroke;
	}

	bool AppendPoint(LL_MapPaintStroke stroke, vector point, float minimumDistance)
	{
		if (!stroke || m_iPointCount >= MAX_POINTS)
			return false;

		vector previous = stroke.m_Points[stroke.m_Points.Count() - 1];
		if (vector.DistanceSq(previous, point) <= minimumDistance * minimumDistance)
			return false;

		stroke.m_Points.Insert(point);
		m_iPointCount++;
		if (stroke.m_Points.Count() == 2)
		{
			// A single point becomes an edge; remove its obsolete dot entry.
			LL_MapPaintSpatialStroke indexed = m_SpatialStrokes.Get(stroke.m_iSpatialId);
			if (indexed)
			{
				UnindexSegment(indexed.m_Segments.Get(0));
				indexed.m_Segments.Remove(0);
			}
		}
		IndexSegment(stroke, stroke.m_Points.Count() - 1, previous, point);
		m_iVersion++;
		return true;
	}

	//! Map the cursor to its leaf cells and inspect only those cells and neighbors
	//! touched by the hover radius. Sparse 5x5 leaf sectors also support coordinates
	//! outside the terrain bounds, where the map widget can show empty margins.
	void QuerySpatialGrid(vector position, float radius, array<ref LL_MapPaintSpatialSegment> results)
	{
		if (!results)
			return;
		results.Clear();
		if (!m_bSpatialBoundsConfigured || m_SpatialSectors.Count() == 0)
			return;

		float searchRadius = Math.Max(0, radius);
		int minX = WorldToLeafX(position[0] - searchRadius);
		int maxX = WorldToLeafX(position[0] + searchRadius);
		int minZ = WorldToLeafZ(position[2] - searchRadius);
		int maxZ = WorldToLeafZ(position[2] + searchRadius);

		BeginSpatialQuery();
		for (int cellX = minX; cellX <= maxX; cellX++)
		{
			for (int cellZ = minZ; cellZ <= maxZ; cellZ++)
			{
				LL_MapPaintSpatialCell cell = GetSpatialCell(cellX, cellZ, false);
				if (cell)
					AddCellSegments(cell, results);
			}
		}
	}

	//! Conservative broad phase for the entire swept eraser, not just its samples.
	//! Visit populated sectors/cells so long paths outside the terrain never scan
	//! millions of empty cells. The exact capsule intersection remains in ErasePath.
	void QuerySpatialPath(array<vector> path, float radius, array<ref LL_MapPaintSpatialSegment> results)
	{
		if (!results)
			return;
		results.Clear();
		if (!path || path.IsEmpty() || !m_bSpatialBoundsConfigured)
			return;
		BeginSpatialQuery();
		float searchRadius = Math.Max(0, radius);
		foreach (string sectorKey, LL_MapPaintSpatialSector sector : m_SpatialSectors)
		{
			float left = m_fSpatialOriginX + sector.m_iX * SPATIAL_SUBDIVISIONS * m_fSpatialLeafWidth;
			float top = m_fSpatialOriginZ + sector.m_iZ * SPATIAL_SUBDIVISIONS * m_fSpatialLeafHeight;
			float right = left + SPATIAL_SUBDIVISIONS * m_fSpatialLeafWidth;
			float bottom = top + SPATIAL_SUBDIVISIONS * m_fSpatialLeafHeight;
			if (!SpatialBoundsTouchPath(path, searchRadius, left, top, right, bottom))
				continue;
			foreach (int cellKey, LL_MapPaintSpatialCell cell : sector.m_Cells)
			{
				int subX = cellKey / SPATIAL_SUBDIVISIONS;
				int subZ = cellKey % SPATIAL_SUBDIVISIONS;
				float cellLeft = left + subX * m_fSpatialLeafWidth;
				float cellTop = top + subZ * m_fSpatialLeafHeight;
				if (SpatialBoundsTouchPath(path, searchRadius, cellLeft, cellTop, cellLeft + m_fSpatialLeafWidth, cellTop + m_fSpatialLeafHeight))
					AddCellSegments(cell, results);
			}
		}
	}

	protected bool SpatialBoundsTouchPath(array<vector> path, float radius, float left, float top, float right, float bottom)
	{
		int sweepCount = Math.Max(1, path.Count() - 1);
		for (int i = 0; i < sweepCount; i++)
		{
			vector start = path[i];
			vector end = path[Math.Min(i + 1, path.Count() - 1)];
			float first = 0;
			float last = 1;
			if (LL_MapPaintGeometry.ClipAxis(start[0], end[0] - start[0], left - radius, right + radius, first, last)
				&& LL_MapPaintGeometry.ClipAxis(start[2], end[2] - start[2], top - radius, bottom + radius, first, last))
				return true;
		}
		return false;
	}

	protected void BeginSpatialQuery()
	{
		m_iQueryStamp++;
		if (m_iQueryStamp <= 0)
		{
			foreach (string sectorKey, LL_MapPaintSpatialSector sector : m_SpatialSectors)
			{
				foreach (int cellKey, LL_MapPaintSpatialCell cell : sector.m_Cells)
				{
					foreach (LL_MapPaintSpatialSegment resetSegment : cell.m_Segments)
						resetSegment.m_iQueryStamp = 0;
				}
			}
			m_iQueryStamp = 1;
		}
	}

	protected void AddCellSegments(LL_MapPaintSpatialCell cell, array<ref LL_MapPaintSpatialSegment> results)
	{
		foreach (LL_MapPaintSpatialSegment segment : cell.m_Segments)
		{
			if (segment.m_iQueryStamp == m_iQueryStamp)
				continue;
			segment.m_iQueryStamp = m_iQueryStamp;
			results.Insert(segment);
		}
	}

	protected int WorldToLeafX(float coordinate)
	{
		return (int)Math.Floor((coordinate - m_fSpatialOriginX) / m_fSpatialLeafWidth);
	}

	protected int WorldToLeafZ(float coordinate)
	{
		return (int)Math.Floor((coordinate - m_fSpatialOriginZ) / m_fSpatialLeafHeight);
	}

	protected int FloorDivide(int value, int divisor)
	{
		int quotient = value / divisor;
		if (value < 0 && value % divisor != 0)
			quotient--;
		return quotient;
	}

	protected LL_MapPaintSpatialCell GetSpatialCell(int leafX, int leafZ, bool create)
	{
		int sectorX = FloorDivide(leafX, SPATIAL_SUBDIVISIONS);
		int sectorZ = FloorDivide(leafZ, SPATIAL_SUBDIVISIONS);
		string sectorKey = string.Format("%1:%2", sectorX, sectorZ);
		LL_MapPaintSpatialSector sector = m_SpatialSectors.Get(sectorKey);
		if (!sector && create)
		{
			sector = new LL_MapPaintSpatialSector();
			sector.m_iX = sectorX;
			sector.m_iZ = sectorZ;
			m_SpatialSectors.Set(sectorKey, sector);
		}
		if (!sector)
			return null;

		int subX = leafX - sectorX * SPATIAL_SUBDIVISIONS;
		int subZ = leafZ - sectorZ * SPATIAL_SUBDIVISIONS;
		int cellKey = subX * SPATIAL_SUBDIVISIONS + subZ;
		LL_MapPaintSpatialCell cell = sector.m_Cells.Get(cellKey);
		if (!cell && create)
		{
			cell = new LL_MapPaintSpatialCell();
			sector.m_Cells.Set(cellKey, cell);
		}
		return cell;
	}

	protected void IndexStroke(LL_MapPaintStroke stroke)
	{
		if (!stroke || stroke.m_Points.IsEmpty())
			return;
		UnindexStroke(stroke);
		EnsureSpatialStroke(stroke);
		if (stroke.m_Points.Count() == 1)
		{
			IndexSegment(stroke, 0, stroke.m_Points[0], stroke.m_Points[0]);
			return;
		}
		for (int pointIndex = 1; pointIndex < stroke.m_Points.Count(); pointIndex++)
			IndexSegment(stroke, pointIndex, stroke.m_Points[pointIndex - 1], stroke.m_Points[pointIndex]);
	}

	protected void IndexSegment(LL_MapPaintStroke stroke, int segmentIndex, vector start, vector end)
	{
		if (!m_bSpatialBoundsConfigured)
			return;

		LL_MapPaintSpatialStroke indexed = EnsureSpatialStroke(stroke);
		UnindexSegment(indexed.m_Segments.Get(segmentIndex));
		LL_MapPaintSpatialSegment segment = new LL_MapPaintSpatialSegment();
		segment.m_Stroke = stroke;
		segment.m_iSegmentIndex = segmentIndex;
		indexed.m_Segments.Set(segmentIndex, segment);
		float dx = end[0] - start[0];
		float dz = end[2] - start[2];
		float length = Math.Sqrt(dx * dx + dz * dz);
		float stepSize = Math.Min(m_fSpatialLeafWidth, m_fSpatialLeafHeight) * 0.5;
		if (stepSize <= 0)
			return;
		int steps = Math.Max(1, (int)Math.Ceil(length / stepSize));
		for (int step = 0; step <= steps; step++)
		{
			float fraction = step / (float)steps;
			float x = start[0] + dx * fraction;
			float z = start[2] + dz * fraction;
			IndexPointCells(segment, x, z);
		}
	}

	protected void IndexPointCells(LL_MapPaintSpatialSegment segment, float x, float z)
	{
		// Sampling at half-leaf spacing and adding the remaining half-step plus
		// stroke radius includes every leaf touched by the segment's visible width.
		float margin = segment.m_Stroke.m_fWidth * 0.5 + Math.Min(m_fSpatialLeafWidth, m_fSpatialLeafHeight) * 0.25;
		int minX = WorldToLeafX(x - margin);
		int maxX = WorldToLeafX(x + margin);
		int minZ = WorldToLeafZ(z - margin);
		int maxZ = WorldToLeafZ(z + margin);
		for (int cellX = minX; cellX <= maxX; cellX++)
		{
			for (int cellZ = minZ; cellZ <= maxZ; cellZ++)
			{
				string key = string.Format("%1:%2", cellX, cellZ);
				if (segment.m_SpatialCells.Get(key))
					continue;

				LL_MapPaintSpatialCell cell = GetSpatialCell(cellX, cellZ, true);
				if (!cell)
					continue;
				cell.m_Segments.Insert(segment);
				segment.m_SpatialCells.Set(key, true);
				segment.m_CellCoordinates.Insert(cellX);
				segment.m_CellCoordinates.Insert(cellZ);
			}
		}
	}

	protected LL_MapPaintSpatialStroke EnsureSpatialStroke(LL_MapPaintStroke stroke)
	{
		if (stroke.m_iSpatialId == 0)
			stroke.m_iSpatialId = m_iNextSpatialId++;
		if (stroke.m_iDrawOrder == 0)
			stroke.m_iDrawOrder = m_iNextDrawOrder++;
		LL_MapPaintSpatialStroke indexed = m_SpatialStrokes.Get(stroke.m_iSpatialId);
		if (!indexed)
		{
			indexed = new LL_MapPaintSpatialStroke();
			m_SpatialStrokes.Set(stroke.m_iSpatialId, indexed);
		}
		return indexed;
	}

	protected void UnindexStroke(LL_MapPaintStroke stroke)
	{
		LL_MapPaintSpatialStroke indexed = m_SpatialStrokes.Get(stroke.m_iSpatialId);
		if (!indexed)
			return;
		foreach (int index, LL_MapPaintSpatialSegment segment : indexed.m_Segments)
			UnindexSegment(segment);
		m_SpatialStrokes.Remove(stroke.m_iSpatialId);
	}

	protected void UnindexSegment(LL_MapPaintSpatialSegment segment)
	{
		if (!segment)
			return;
		// Only visit cells recorded when this segment was inserted.
		for (int i = 0; i < segment.m_CellCoordinates.Count(); i += 2)
		{
			int leafX = segment.m_CellCoordinates[i];
			int leafZ = segment.m_CellCoordinates[i + 1];
			int sectorX = FloorDivide(leafX, SPATIAL_SUBDIVISIONS);
			int sectorZ = FloorDivide(leafZ, SPATIAL_SUBDIVISIONS);
			string sectorKey = string.Format("%1:%2", sectorX, sectorZ);
			LL_MapPaintSpatialSector sector = m_SpatialSectors.Get(sectorKey);
			if (!sector)
				continue;
			int cellKey = (leafX - sectorX * SPATIAL_SUBDIVISIONS) * SPATIAL_SUBDIVISIONS + leafZ - sectorZ * SPATIAL_SUBDIVISIONS;
			LL_MapPaintSpatialCell cell = sector.m_Cells.Get(cellKey);
			if (!cell)
				continue;
			cell.m_Segments.RemoveItem(segment);
			if (cell.m_Segments.IsEmpty())
			{
				sector.m_Cells.Remove(cellKey);
				if (sector.m_Cells.IsEmpty())
					m_SpatialSectors.Remove(sectorKey);
			}
		}
		segment.m_CellCoordinates.Clear();
		segment.m_SpatialCells.Clear();
	}

	protected void RebuildSpatialGrid()
	{
		m_SpatialSectors.Clear();
		m_SpatialStrokes.Clear();
		foreach (LL_MapPaintStroke stroke : m_Strokes)
			IndexStroke(stroke);
	}

	//! Split polylines at the union of the eraser path capsules. Commit index
	//! removals/insertions only for changed strokes, after the point budget check.
	//! Expanding by half the pen width also erases the visible edges and round caps.
	bool ErasePath(array<vector> path, float radius, array<int> erasedOwners = null)
	{
		if (erasedOwners)
			erasedOwners.Clear();
		if (!path || path.IsEmpty())
			return false;

		array<ref LL_MapPaintSpatialSegment> nearby = {};
		QuerySpatialPath(path, radius, nearby);
		if (nearby.IsEmpty())
			return false;
		map<int, ref LL_MapPaintEraseCandidate> candidates = new map<int, ref LL_MapPaintEraseCandidate>();
		foreach (LL_MapPaintSpatialSegment segment : nearby)
		{
			int strokeId = segment.m_Stroke.m_iSpatialId;
			LL_MapPaintEraseCandidate candidate = candidates.Get(strokeId);
			if (!candidate)
			{
				candidate = new LL_MapPaintEraseCandidate();
				candidates.Set(strokeId, candidate);
			}
			candidate.m_Segments.Set(segment.m_iSegmentIndex, true);
		}

		array<ref LL_MapPaintStroke> survivors = {};
		array<ref LL_MapPaintStroke> removed = {};
		array<ref LL_MapPaintStroke> added = {};
		array<vector> cutIntervals = {};
		array<vector> mergedIntervals = {};
		bool changed;
		foreach (LL_MapPaintStroke stroke : m_Strokes)
		{
			LL_MapPaintEraseCandidate candidate = candidates.Get(stroke.m_iSpatialId);
			if (!candidate)
			{
				survivors.Insert(stroke);
				continue;
			}
			float hitRadius = radius + stroke.m_fWidth * 0.5;
			int count = stroke.m_Points.Count();
			if (count == 1)
			{
				bool erased;
				int sweepCount = path.Count() - 1;
				if (sweepCount < 1)
					sweepCount = 1;
				for (int sweepIndex = 0; sweepIndex < sweepCount; sweepIndex++)
				{
					vector sweepStart = path[0];
					vector sweepEnd = path[0];
					if (path.Count() > 1)
					{
						sweepStart = path[sweepIndex];
						sweepEnd = path[sweepIndex + 1];
					}
					float first, last;
					if (LL_MapPaintGeometry.CapsuleInterval(stroke.m_Points[0], stroke.m_Points[0], sweepStart, sweepEnd, hitRadius, first, last))
					{
						erased = true;
						break;
					}
				}
				if (erased)
				{
					changed = true;
					removed.Insert(stroke);
				}
				else
					survivors.Insert(stroke);
				continue;
			}

			int survivorStart = survivors.Count();
			bool strokeChanged;
			LL_MapPaintStroke fragment = stroke.MakeFragment();
			for (int i = 1; i < count; i++)
			{
				vector a = stroke.m_Points[i - 1];
				vector b = stroke.m_Points[i];
				vector direction = b - a;
				cutIntervals.Clear();
				mergedIntervals.Clear();
				bool checkSegment = candidate.m_Segments.Get(i);
				int sweepCount = path.Count() - 1;
				if (sweepCount < 1)
					sweepCount = 1;
				for (int sweepIndex = 0; checkSegment && sweepIndex < sweepCount; sweepIndex++)
				{
					vector sweepStart = path[0];
					vector sweepEnd = path[0];
					if (path.Count() > 1)
					{
						sweepStart = path[sweepIndex];
						sweepEnd = path[sweepIndex + 1];
					}
					float first, last;
					if (LL_MapPaintGeometry.CapsuleInterval(a, b, sweepStart, sweepEnd, hitRadius, first, last))
						cutIntervals.Insert(Vector(first, last, 0));
				}

				if (cutIntervals.IsEmpty())
				{
					if (fragment.m_Points.IsEmpty())
						fragment.m_Points.Insert(a);
					fragment.m_Points.Insert(b);
					continue;
				}

				changed = true;
				strokeChanged = true;
				// Sort the small per-edge interval list by repeatedly selecting its
				// earliest entry. A network packet contains at most 32 points.
				float pendingFirst, pendingLast;
				bool hasPending;
				while (!cutIntervals.IsEmpty())
				{
					int earliestIndex;
					for (int intervalIndex = 1; intervalIndex < cutIntervals.Count(); intervalIndex++)
					{
						if (cutIntervals[intervalIndex][0] < cutIntervals[earliestIndex][0])
							earliestIndex = intervalIndex;
					}
					vector interval = cutIntervals[earliestIndex];
					cutIntervals.Remove(earliestIndex);

					if (!hasPending)
					{
						pendingFirst = interval[0];
						pendingLast = interval[1];
						hasPending = true;
					}
					else if (interval[0] <= pendingLast + LL_MapPaintGeometry.EPSILON)
						pendingLast = Math.Max(pendingLast, interval[1]);
					else
					{
						mergedIntervals.Insert(Vector(pendingFirst, pendingLast, 0));
						pendingFirst = interval[0];
						pendingLast = interval[1];
					}
				}
				if (hasPending)
					mergedIntervals.Insert(Vector(pendingFirst, pendingLast, 0));

				float cursor;
				foreach (vector interval : mergedIntervals)
				{
					float cutStart = interval[0];
					float cutEnd = interval[1];
					if (cutStart > cursor + LL_MapPaintGeometry.EPSILON)
					{
						if (fragment.m_Points.IsEmpty())
							fragment.m_Points.Insert(a + direction * cursor);
						fragment.m_Points.Insert(a + direction * cutStart);
						survivors.Insert(fragment);
						fragment = stroke.MakeFragment();
					}
					else
					{
						if (fragment.m_Points.Count() > 1)
							survivors.Insert(fragment);
						fragment = stroke.MakeFragment();
					}
					cursor = Math.Max(cursor, cutEnd);
				}

				if (cursor < 1 - LL_MapPaintGeometry.EPSILON)
				{
					if (fragment.m_Points.IsEmpty())
						fragment.m_Points.Insert(a + direction * cursor);
					fragment.m_Points.Insert(b);
				}
				else if (fragment.m_Points.Count() > 1)
				{
					survivors.Insert(fragment);
					fragment = stroke.MakeFragment();
				}
			}

			if (fragment.m_Points.Count() > 1)
				survivors.Insert(fragment);
			if (!strokeChanged)
			{
				while (survivors.Count() > survivorStart)
					survivors.Remove(survivors.Count() - 1);
				survivors.Insert(stroke);
				continue;
			}
			removed.Insert(stroke);
			for (int fragmentIndex = survivorStart; fragmentIndex < survivors.Count(); fragmentIndex++)
				added.Insert(survivors[fragmentIndex]);
		}

		if (!changed)
			return false;

		int pointCount;
		foreach (LL_MapPaintStroke remaining : survivors)
			pointCount += remaining.m_Points.Count();
		if (pointCount > MAX_STORED_POINTS)
			return false;
		foreach (LL_MapPaintStroke oldStroke : removed)
		{
			UnindexStroke(oldStroke);
			// Report only committed changes, after geometry and point-budget checks.
			if (erasedOwners && !erasedOwners.Contains(oldStroke.m_iOwner))
				erasedOwners.Insert(oldStroke.m_iOwner);
		}
		m_Strokes = survivors;
		m_iPointCount = pointCount;
		foreach (LL_MapPaintStroke newStroke : added)
			IndexStroke(newStroke);
		m_iVersion++;
		return true;
	}

	//! Compatibility helper for callers that erase a single swept segment.
	bool Erase(vector from, vector to, float radius)
	{
		array<vector> path = {from, to};
		return ErasePath(path, radius);
	}
}
