//! Geometry in map world coordinates (XY stored in vector XZ).
class LL_MapPaintGeometry
{
	static const float EPSILON = 0.000001;

	//! Intersect [0, 1] with the interval where min <= origin + delta * t <= max.
	static bool ClipAxis(float origin, float delta, float minimum, float maximum, inout float first, inout float last)
	{
		if (Math.AbsFloat(delta) < EPSILON)
			return origin >= minimum && origin <= maximum;

		float a = (minimum - origin) / delta;
		float b = (maximum - origin) / delta;
		first = Math.Max(first, Math.Min(a, b));
		last = Math.Min(last, Math.Max(a, b));
		return first <= last;
	}

	static bool CircleInterval(vector start, vector end, vector center, float radius, out float first, out float last)
	{
		vector direction = end - start;
		vector offset = start - center;
		float lengthSq = direction.LengthSq();
		if (lengthSq < EPSILON)
		{
			first = 0;
			last = 1;
			return offset.LengthSq() <= radius * radius;
		}

		float projection = vector.Dot(offset, direction);
		float discriminant = projection * projection - lengthSq * (offset.LengthSq() - radius * radius);
		if (discriminant < 0)
			return false;

		float root = Math.Sqrt(discriminant);
		first = Math.Max(0, (-projection - root) / lengthSq);
		last = Math.Min(1, (-projection + root) / lengthSq);
		return first <= last;
	}

	//! Exact swept-circle eraser: union of two disks and their connecting rectangle.
	//! A capsule is convex, so the union of the hit intervals is one interval.
	static bool CapsuleInterval(vector start, vector end, vector sweepStart, vector sweepEnd, float radius, out float first, out float last)
	{
		first = 1;
		last = 0;
		float a, b;
		bool hit;
		if (CircleInterval(start, end, sweepStart, radius, a, b))
		{
			first = a;
			last = b;
			hit = true;
		}

		vector axis = sweepEnd - sweepStart;
		float length = axis.Length();
		if (length < EPSILON)
			return hit;

		if (CircleInterval(start, end, sweepEnd, radius, a, b))
		{
			first = Math.Min(first, a);
			last = Math.Max(last, b);
			hit = true;
		}

		axis = axis / length;
		vector normal = Vector(-axis[2], 0, axis[0]);
		vector offset = start - sweepStart;
		vector direction = end - start;
		a = 0;
		b = 1;
		if (ClipAxis(vector.Dot(offset, axis), vector.Dot(direction, axis), 0, length, a, b)
			&& ClipAxis(vector.Dot(offset, normal), vector.Dot(direction, normal), -radius, radius, a, b))
		{
			first = Math.Min(first, a);
			last = Math.Max(last, b);
			hit = true;
		}

		return hit;
	}
}
