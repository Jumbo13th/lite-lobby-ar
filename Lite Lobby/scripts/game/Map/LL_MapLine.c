// One map line as both sides hold it. Points are whole decimetres of world X and Z, x then
// y, so the wire and the save carry plain ints and neither side depends on the map's scale.

//! Sent as an int in RpcDo_OwnerMapLineRefused; the values are the wire contract.
enum LL_EMapLineRefusal
{
	NONE = 0,
	NOT_ALLOWED = 1,
	PLAYER_LIMIT = 2,
	FACTION_LIMIT = 3,
	TOO_SOON = 4,
	INVALID = 5,
	POINT_LIMIT = 6
}

class LL_MapLine
{
	static const int MAX_POINTS = 64;
	static const int MAX_LINES_PER_PLAYER = 20;
	static const int MAX_LINES_PER_FACTION = 150;
	static const float MIN_INTERVAL_S = 2;
	static const float MIN_WIDTH_M = 0.1;
	static const float MAX_WIDTH_FRACTION = 0.05;

	int m_iId;
	int m_iColor;
	float m_fWidth;
	ref array<int> m_aPoints = {};
	WorldTimestamp m_CreatedAt;
	string m_sAuthorName;

	// Server only: never sent to a client.
	FactionKey m_sFactionKey;
	string m_sAuthorKey;

	// Client only: decided by the server for this recipient.
	bool m_bOwn;

	// Client only: ground bounding box for culling and hit tests.
	vector m_vMin;
	vector m_vMax;

	//! Client side, in the player's language. POINT_LIMIT is only raised while drawing.
	static string TranslateRefusal(LL_EMapLineRefusal reason)
	{
		switch (reason)
		{
			case LL_EMapLineRefusal.PLAYER_LIMIT:
				return WidgetManager.Translate("#LL-MapLines_RefusedPlayerLimit", MAX_LINES_PER_PLAYER.ToString());
			case LL_EMapLineRefusal.FACTION_LIMIT:
				return WidgetManager.Translate("#LL-MapLines_RefusedFactionLimit", MAX_LINES_PER_FACTION.ToString());
			case LL_EMapLineRefusal.TOO_SOON:
				return WidgetManager.Translate("#LL-MapLines_RefusedTooSoon");
			case LL_EMapLineRefusal.INVALID:
				return WidgetManager.Translate("#LL-MapLines_RefusedInvalid");
			case LL_EMapLineRefusal.POINT_LIMIT:
				return WidgetManager.Translate("#LL-MapLines_PointLimit", MAX_POINTS.ToString());
		}

		return WidgetManager.Translate("#LL-MapLines_RefusedNotAllowed");
	}

	static int ToDecimetres(float metres)
	{
		return Math.Round(metres * 10);
	}

	static float ToMetres(int decimetres)
	{
		return decimetres * 0.1;
	}

	int GetPointCount()
	{
		return m_aPoints.Count() / 2;
	}

	void GetPoint(int index, out float x, out float y)
	{
		x = ToMetres(m_aPoints[index * 2]);
		y = ToMetres(m_aPoints[index * 2 + 1]);
	}

	void ComputeBounds()
	{
		int count = GetPointCount();
		if (count == 0)
			return;

		float minX = float.MAX;
		float minY = float.MAX;
		float maxX = -float.MAX;
		float maxY = -float.MAX;
		float x, y;
		for (int i = 0; i < count; i++)
		{
			GetPoint(i, x, y);
			minX = Math.Min(minX, x);
			minY = Math.Min(minY, y);
			maxX = Math.Max(maxX, x);
			maxY = Math.Max(maxY, y);
		}

		// The start dot is wider than the line.
		float pad = m_fWidth;
		m_vMin = Vector(minX - pad, 0, minY - pad);
		m_vMax = Vector(maxX + pad, 0, maxY + pad);
	}
}
