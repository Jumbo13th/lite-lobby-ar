//! Deterministic map-space circle shared by drag previews and replicated operations.
class LL_MapPaintCircleGeometry
{
	static const int MIN_SEGMENTS = 12;
	static const int MAX_SEGMENTS = 64;
	static const float TARGET_SEGMENT_LENGTH = 25;

	static int GetSegmentCount(float radius)
	{
		// Round up to complete quadrants and use world units for identical replicas.
		int segments = (int)Math.Ceil(2 * Math.PI * radius / (4 * TARGET_SEGMENT_LENGTH)) * 4;
		return Math.ClampInt(segments, MIN_SEGMENTS, MAX_SEGMENTS);
	}

	static float GetRadius(vector center, vector edge)
	{
		return vector.Distance(center, edge);
	}

	static bool IsValid(vector center, vector edge)
	{
		float radius = GetRadius(center, edge);
		float limit = LL_MapPaintProtocol.MAX_COORDINATE;
		return center[1] == 0 && edge[1] == 0 && radius > 0
			&& center[0] - radius >= -limit && center[0] + radius <= limit
			&& center[2] - radius >= -limit && center[2] + radius <= limit;
	}

	static bool BuildPoints(vector center, vector edge, array<vector> points)
	{
		points.Clear();
		if (!IsValid(center, edge))
			return false;
		float radius = GetRadius(center, edge);
		int segments = GetSegmentCount(radius);
		for (int i = 0; i < segments; i++)
		{
			float angle = i * (2 * Math.PI / segments);
			points.Insert(center + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius));
		}
		// Reuse the exact first point so there is no floating-point seam.
		points.Insert(points[0]);
		return true;
	}
}
