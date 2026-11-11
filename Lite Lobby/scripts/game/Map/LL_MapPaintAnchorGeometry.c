//! Flat caps and bounded miter joins for translucent anchor chains.
class LL_MapPaintAnchorGeometry
{
	protected static vector AngularOffset(array<vector> points, int index, vector fallbackNormal, float halfWidth)
	{
		if (index == 0 || index == points.Count() - 1)
			return fallbackNormal * halfWidth;
		vector before = points[index] - points[index - 1];
		vector after = points[index + 1] - points[index];
		before = before / before.Length();
		after = after / after.Length();
		vector firstNormal = Vector(-before[1], before[0], 0);
		vector secondNormal = Vector(-after[1], after[0], 0);
		float denominator = 1 + vector.Dot(firstNormal, secondNormal);
		if (denominator < 0.001)
			return fallbackNormal * halfWidth;
		vector offset = (firstNormal + secondNormal) * (halfWidth / denominator);
		float length = offset.Length();
		float limit = halfWidth * 4;
		if (length > limit)
			offset = offset * (limit / length);
		return offset;
	}

	static void AddLines(array<float> vertices, float width, int color, array<ref CanvasWidgetCommand> commands)
	{
		if (width <= 0)
			return;
		array<vector> points = {};
		for (int i = 0; i + 1 < vertices.Count(); i += 2)
		{
			vector point = Vector(vertices[i], vertices[i + 1], 0);
			if (points.IsEmpty() || vector.DistanceSq(points[points.Count() - 1], point) > 0.000001)
				points.Insert(point);
		}
		for (int i = 1; i < points.Count(); i++)
		{
			vector start = points[i - 1];
			vector end = points[i];
			vector direction = end - start;
			vector normal = Vector(-direction[1], direction[0], 0) / direction.Length();
			vector startOffset = AngularOffset(points, i - 1, normal, width * 0.5);
			vector endOffset = AngularOffset(points, i, normal, width * 0.5);
			vector a = start - startOffset;
			vector b = end - endOffset;
			vector c = end + endOffset;
			vector d = start + startOffset;
			PolygonDrawCommand segment = new PolygonDrawCommand();
			segment.m_iColor = color;
			segment.m_Vertices = {a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1]};
			commands.Insert(segment);
		}
	}
}
