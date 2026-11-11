enum LL_EMapPaintOperation
{
	BRUSH,
	ERASE,
	ANCHOR,
	ANCHOR_CONTINUE,
	LINE,
	CIRCLE
}

class LL_MapPaintOperation
{
	int m_iRevision;
	int m_iOwner;
	int m_iToken;
	LL_EMapPaintOperation m_Kind;
	float m_fSize;
	int m_iColor;
	ref array<vector> m_Points = {};
}

class LL_MapPaintProtocol
{
	static const int BATCH_POINTS = 32;
	static const float SEND_INTERVAL = 0.1;
	static const float MAX_COORDINATE = 1000000;

	static bool IsStroke(int kind)
	{
		return kind == LL_EMapPaintOperation.BRUSH || kind == LL_EMapPaintOperation.LINE || kind == LL_EMapPaintOperation.CIRCLE || IsAnchor(kind);
	}

	static bool IsAnchor(int kind)
	{
		return kind == LL_EMapPaintOperation.ANCHOR || kind == LL_EMapPaintOperation.ANCHOR_CONTINUE;
	}

	static int NormalizeColor(int kind, int color)
	{
		if (IsAnchor(kind))
			return LL_MapPaintAnchorStyle.GetColor(color);
		return color | 0xff000000;
	}

	//! Called on the server before accepting any client geometry.
	static bool Validate(int kind, float size, array<vector> points)
	{
		if ((!IsStroke(kind) && kind != LL_EMapPaintOperation.ERASE)
			|| !points || points.IsEmpty() || points.Count() > BATCH_POINTS)
			return false;
		if ((kind == LL_EMapPaintOperation.LINE || kind == LL_EMapPaintOperation.CIRCLE) && points.Count() != 2)
			return false;
		float maximum = 4096;
		if (kind == LL_EMapPaintOperation.ERASE)
			maximum = 8192;
		// Range comparisons also reject NaN and infinity.
		if (!(size >= 0.01 && size <= maximum))
			return false;
		foreach (vector point : points)
		{
			if (!(point[0] >= -MAX_COORDINATE && point[0] <= MAX_COORDINATE
				&& point[1] == 0 && point[2] >= -MAX_COORDINATE && point[2] <= MAX_COORDINATE))
				return false;
		}
		if (kind == LL_EMapPaintOperation.CIRCLE)
			return LL_MapPaintCircleGeometry.IsValid(points[0], points[1]);
		return true;
	}

	static bool Apply(LL_MapPaintDocument document, int kind, float size, int color, array<vector> points, int owner = -1, array<int> erasedOwners = null)
	{
		color = NormalizeColor(kind, color);
		if (IsAnchor(kind))
			return document.AddAnchorSegment(points, size, color, owner, kind == LL_EMapPaintOperation.ANCHOR_CONTINUE) != null;
		if (kind == LL_EMapPaintOperation.CIRCLE)
		{
			array<vector> circle = {};
			if (points.Count() != 2 || !LL_MapPaintCircleGeometry.BuildPoints(points[0], points[1], circle))
				return false;
			return document.AddStroke(circle, size, color, owner) != null;
		}
		if (kind == LL_EMapPaintOperation.BRUSH || kind == LL_EMapPaintOperation.LINE)
			return document.AddStroke(points, size, color, owner) != null;
		return document.ErasePath(points, size, erasedOwners);
	}
}

//! Keeps the visible document stable while snapshots and live operations arrive
//! on different replicated entities. Only contiguous server revisions are applied.
class LL_MapPaintReplica
{
	ref LL_MapPaintDocument m_Document = new LL_MapPaintDocument();
	protected ref LL_MapPaintDocument m_Snapshot;
	protected ref LL_MapPaintStroke m_SnapshotStroke;
	protected ref map<int, ref LL_MapPaintOperation> m_Buffer = new map<int, ref LL_MapPaintOperation>();
	protected int m_iRevision;
	protected int m_iSnapshotRevision;
	protected int m_iSnapshotRequest;
	protected int m_iSnapshotPointCount;
	protected bool m_bReady;

	bool IsReady() { return m_bReady; }
	int GetRevision() { return m_iRevision; }
	bool HasGap() { return m_bReady && !m_Buffer.IsEmpty(); }

	void SetAuthority()
	{
		m_bReady = true;
	}

	void AdvanceAuthority()
	{
		m_iRevision++;
	}

	void AwaitSnapshot(int request)
	{
		m_iSnapshotRequest = request;
		m_bReady = false;
		m_Snapshot = null;
		m_SnapshotStroke = null;
	}

	void BeginSnapshot(int request, int revision)
	{
		if (request != m_iSnapshotRequest)
			return;
		m_Snapshot = new LL_MapPaintDocument();
		m_SnapshotStroke = null;
		m_iSnapshotRevision = revision;
		m_iSnapshotPointCount = 0;
	}

	void SnapshotChunk(int request, bool first, float width, int color, array<vector> points, int owner = -1, bool anchorLine = false)
	{
		if (request != m_iSnapshotRequest || !m_Snapshot)
			return;
		if (!LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.BRUSH, width, points)
			|| m_iSnapshotPointCount + points.Count() > LL_MapPaintDocument.MAX_STORED_POINTS)
		{
			m_Snapshot = null;
			return;
		}
		if (first)
		{
			m_SnapshotStroke = new LL_MapPaintStroke();
			m_SnapshotStroke.m_fWidth = width;
			m_SnapshotStroke.m_bAnchorLine = anchorLine;
			m_SnapshotStroke.m_iColor = color;
			if (anchorLine)
				m_SnapshotStroke.m_iColor = LL_MapPaintAnchorStyle.GetColor(color);
			m_SnapshotStroke.m_iOwner = owner;
			m_Snapshot.m_Strokes.Insert(m_SnapshotStroke);
		}
		if (!m_SnapshotStroke)
			return;
		foreach (vector point : points)
			m_SnapshotStroke.m_Points.Insert(point);
		m_iSnapshotPointCount += points.Count();
	}

	bool EndSnapshot(int request)
	{
		if (request != m_iSnapshotRequest || !m_Snapshot)
			return false;
		m_Document.ReplaceFrom(m_Snapshot);
		m_Snapshot = null;
		m_SnapshotStroke = null;
		m_iRevision = m_iSnapshotRevision;
		m_bReady = true;
		array<int> obsolete = {};
		foreach (int revision, LL_MapPaintOperation operation : m_Buffer)
		{
			if (revision <= m_iRevision)
				obsolete.Insert(revision);
		}
		foreach (int old : obsolete)
			m_Buffer.Remove(old);
		ApplyBuffered();
		return m_bReady;
	}

	void Receive(LL_MapPaintOperation operation)
	{
		if (operation.m_iRevision <= m_iRevision)
			return;
		if (m_Buffer.Count() >= 512)
		{
			m_Buffer.Clear();
			m_bReady = false;
			return;
		}
		m_Buffer.Set(operation.m_iRevision, operation);
		ApplyBuffered();
	}

	protected void ApplyBuffered()
	{
		while (m_bReady)
		{
			LL_MapPaintOperation operation = m_Buffer.Get(m_iRevision + 1);
			if (!operation)
				return;
			if (!LL_MapPaintProtocol.Apply(m_Document, operation.m_Kind, operation.m_fSize, operation.m_iColor, operation.m_Points, operation.m_iOwner))
			{
				m_bReady = false;
				return;
			}
			m_Buffer.Remove(m_iRevision + 1);
			m_iRevision++;
		}
	}
}
