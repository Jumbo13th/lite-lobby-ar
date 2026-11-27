// The local player's map lines as the server delivered them for their faction, ordered by
// server id, which is the stacking order. A line can arrive twice: live and in a faction sync.

class LL_MapLineCache
{
	protected ref array<ref LL_MapLine> m_aLines = {};
	protected ref ScriptInvokerVoid m_OnChanged = new ScriptInvokerVoid();

	static LL_MapLineCache GetLocal()
	{
		LL_LobbyPlayerComponent lobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (!lobbyPlayer)
			return null;

		return lobbyPlayer.GetMapLineCache();
	}

	array<ref LL_MapLine> GetLines()
	{
		return m_aLines;
	}

	ScriptInvokerVoid GetOnChanged()
	{
		return m_OnChanged;
	}

	void Add(notnull LL_MapLine line)
	{
		int insertAt = m_aLines.Count();
		for (int i = m_aLines.Count() - 1; i >= 0; i--)
		{
			int id = m_aLines[i].m_iId;
			if (id == line.m_iId)
				return;

			if (id < line.m_iId)
				break;

			insertAt = i;
		}

		line.ComputeBounds();
		m_aLines.InsertAt(line, insertAt);
		m_OnChanged.Invoke();
	}

	void Remove(int id)
	{
		for (int i = 0; i < m_aLines.Count(); i++)
		{
			if (m_aLines[i].m_iId != id)
				continue;

			m_aLines.RemoveOrdered(i);
			m_OnChanged.Invoke();
			return;
		}
	}

	void Reset()
	{
		if (m_aLines.IsEmpty())
			return;

		m_aLines.Clear();
		m_OnChanged.Invoke();
	}

	int CountOwn()
	{
		int count;
		foreach (LL_MapLine line : m_aLines)
		{
			if (line.m_bOwn)
				count++;
		}

		return count;
	}

	int CountAll()
	{
		return m_aLines.Count();
	}
}
