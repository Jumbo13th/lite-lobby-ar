//! Author names are captured when the change is accepted, including disconnected players.
class LL_MapPaintHistoryEntry
{
	int m_iOwner;
	int m_iTool;
	int m_iCreatedAt;
	string m_sAuthor;
	string m_sErasedAuthor;

	LL_MapPaintHistoryEntry Copy()
	{
		LL_MapPaintHistoryEntry entry = new LL_MapPaintHistoryEntry();
		entry.m_iOwner = m_iOwner;
		entry.m_iTool = m_iTool;
		entry.m_iCreatedAt = m_iCreatedAt;
		entry.m_sAuthor = m_sAuthor;
		entry.m_sErasedAuthor = m_sErasedAuthor;
		return entry;
	}

	string GetText()
	{
		string action = "#LL-MapPaint_HistoryBrush";
		switch (m_iTool)
		{
			case LL_EMapPaintMode.ERASER: action = "#LL-MapPaint_HistoryErase"; break;
			case LL_EMapPaintMode.LINE: action = "#LL-MapPaint_HistoryLine"; break;
			case LL_EMapPaintMode.ANCHOR: action = "#LL-MapPaint_HistoryAnchor"; break;
			case LL_EMapPaintMode.CIRCLE: action = "#LL-MapPaint_HistoryCircle"; break;
		}
		string author = LL_MapPaintHistory.GetDisplayName(m_sAuthor);
		if (m_iTool == LL_EMapPaintMode.ERASER && !m_sErasedAuthor.IsEmpty())
			return WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", author, LL_MapPaintHistory.GetDisplayName(m_sErasedAuthor));
		string text = WidgetManager.Translate(action, author);
		return text;
	}
}

//! Current action group for one player, including simultaneous eraser targets.
class LL_MapPaintHistoryGroup
{
	int m_iTool;
	int m_iStartedAt;
	ref array<string> m_ErasedTargets = {};
}

//! Server-side rolling log for one faction. Times are monotonic milliseconds.
class LL_MapPaintHistory
{
	static const int GROUP_INTERVAL = 10000;
	static const int RETENTION = 300000;
	protected ref array<ref LL_MapPaintHistoryEntry> m_Entries = {};
	protected ref map<int, ref LL_MapPaintHistoryGroup> m_Groups = new map<int, ref LL_MapPaintHistoryGroup>();

	static int GetTool(int kind)
	{
		if (LL_MapPaintProtocol.IsAnchor(kind))
			return LL_EMapPaintMode.ANCHOR;
		if (kind == LL_EMapPaintOperation.LINE)
			return LL_EMapPaintMode.LINE;
		if (kind == LL_EMapPaintOperation.ERASE)
			return LL_EMapPaintMode.ERASER;
		if (kind == LL_EMapPaintOperation.CIRCLE)
			return LL_EMapPaintMode.CIRCLE;
		return LL_EMapPaintMode.BRUSH;
	}

	static string GetPlayerName(int owner)
	{
		string name;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			name = playerManager.GetPlayerName(owner);
		if (!name.IsEmpty())
			return name;
		if (owner < 0)
			return "#LL-MapPaint_UnknownPlayer";
		return string.Format("#LL-MapPaint_Player:%1", owner);
	}

	static string GetDisplayName(string name)
	{
		string prefix = "#LL-MapPaint_Player:";
		if (name.IndexOf(prefix) == 0)
			return WidgetManager.Translate("#LL-MapPaint_Player", name.Substring(prefix.Length(), name.Length() - prefix.Length()));
		if (name == "#LL-MapPaint_UnknownPlayer" || name == "#LL-MapPaint_LocalPlayer")
			return WidgetManager.Translate(name);
		return name;
	}

	static array<string> GetPlayerNames(array<int> owners)
	{
		array<string> names = {};
		if (owners)
		{
			foreach (int owner : owners)
				names.Insert(GetPlayerName(owner));
		}
		return names;
	}

	bool Record(int owner, string author, int kind, int now, array<string> erasedAuthors = null, array<int> erasedOwners = null)
	{
		Prune(now);
		int tool = GetTool(kind);
		array<string> targetKeys = {};
		array<string> targetNames = {};
		if (tool == LL_EMapPaintMode.ERASER)
			CollectErasedTargets(erasedAuthors, erasedOwners, targetKeys, targetNames);

		LL_MapPaintHistoryGroup group = m_Groups.Get(owner);
		bool withinInterval = group && group.m_iTool == tool && now - group.m_iStartedAt < GROUP_INTERVAL;
		if (withinInterval && tool != LL_EMapPaintMode.ERASER)
			return false;

		int previousCount = m_Entries.Count();
		if (tool == LL_EMapPaintMode.ERASER)
		{
			foreach (int targetIndex, string key : targetKeys)
			{
				if (withinInterval && group.m_ErasedTargets.Contains(key))
					continue;
				AddEntry(owner, author, tool, now, targetNames[targetIndex]);
			}
		}
		else
			AddEntry(owner, author, tool, now);

		if (!group)
		{
			group = new LL_MapPaintHistoryGroup();
			m_Groups.Set(owner, group);
		}
		// Track the actual targets even when no row was added: continuing on one
		// owner and then switching back must start a new record. Repeated packets
		// with the same simultaneous targets must not restart the interval.
		group.m_ErasedTargets.Copy(targetKeys);
		if (m_Entries.Count() == previousCount)
			return false;
		group.m_iTool = tool;
		group.m_iStartedAt = now;
		return true;
	}

	protected void CollectErasedTargets(array<string> authors, array<int> owners, array<string> keys, array<string> names)
	{
		if (authors)
		{
			foreach (int index, string name : authors)
			{
				string key = "name:" + name;
				if (owners && index < owners.Count())
					key = "player:" + owners[index].ToString();
				if (keys.Contains(key))
					continue;
				keys.Insert(key);
				names.Insert(name);
			}
		}
		if (keys.IsEmpty())
		{
			keys.Insert("unknown");
			names.Insert(string.Empty);
		}
	}

	protected void AddEntry(int owner, string author, int tool, int now, string erasedAuthor = string.Empty)
	{
		LL_MapPaintHistoryEntry entry = new LL_MapPaintHistoryEntry();
		entry.m_iOwner = owner;
		entry.m_sAuthor = author;
		entry.m_iTool = tool;
		entry.m_iCreatedAt = now;
		entry.m_sErasedAuthor = erasedAuthor;
		m_Entries.Insert(entry);
	}

	void Prune(int now)
	{
		for (int i = m_Entries.Count() - 1; i >= 0; i--)
		{
			if (now - m_Entries[i].m_iCreatedAt >= RETENTION)
				m_Entries.RemoveOrdered(i);
		}
		array<int> expiredGroups = {};
		foreach (int owner, LL_MapPaintHistoryGroup group : m_Groups)
		{
			if (now - group.m_iStartedAt >= RETENTION)
				expiredGroups.Insert(owner);
		}
		foreach (int expiredOwner : expiredGroups)
			m_Groups.Remove(expiredOwner);
	}

	bool IsEmpty()
	{
		return m_Entries.IsEmpty();
	}

	//! Copies preserve the open window even when the live log is pruned or extended.
	array<ref LL_MapPaintHistoryEntry> Snapshot(int now)
	{
		Prune(now);
		array<ref LL_MapPaintHistoryEntry> snapshot = {};
		for (int i = m_Entries.Count() - 1; i >= 0; i--)
			snapshot.Insert(m_Entries[i].Copy());
		return snapshot;
	}
}

//! One explicitly requested snapshot, never a subscription to live changes.
class LL_MapPaintHistorySnapshot
{
	static const int BATCH_SIZE = 50;
	int m_iRequest;
	string m_sFactionKey;
	bool m_bReady;
	bool m_bAvailable = true;
	ref array<ref LL_MapPaintHistoryEntry> m_Entries = {};

	//! Parallel arrays keep the RPC payload limited to primitive, supported types.
	bool AppendBatch(array<string> authors, array<int> tools, array<string> erasedAuthors)
	{
		if (m_bReady || !m_bAvailable)
			return false;
		if (!authors || !tools || !erasedAuthors || authors.IsEmpty() || authors.Count() > BATCH_SIZE
			|| tools.Count() != authors.Count() || erasedAuthors.Count() != authors.Count())
		{
			m_Entries.Clear();
			m_bAvailable = false;
			return false;
		}
		foreach (int index, string author : authors)
		{
			LL_MapPaintHistoryEntry entry = new LL_MapPaintHistoryEntry();
			entry.m_sAuthor = author;
			entry.m_iTool = tools[index];
			entry.m_sErasedAuthor = erasedAuthors[index];
			m_Entries.Insert(entry);
		}
		return true;
	}
}
