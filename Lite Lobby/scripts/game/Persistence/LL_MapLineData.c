// Map lines as the server stored them, for a session resumed after a crash. The author key
// is the reconnect identity, so authors keep their lines and limits after the resume.

class LL_MapLineRecord
{
	int id;
	string factionKey;
	string authorKey;
	string authorName;
	int color;
	float width;
	ref array<int> points = {};
	// Seconds before the snapshot: a world timestamp does not survive a restart.
	float age;
}

class LL_MapLineData : PersistentState
{
	protected ref array<ref LL_MapLineRecord> m_aPending;

	void SetPending(array<ref LL_MapLineRecord> records)
	{
		m_aPending = records;
	}

	// Inserted once the system is active, as markers are, so the lobby manager exists.
	void HandleStateChange(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		if (newState != EPersistenceSystemState.ACTIVE || !m_aPending)
			return;

		SCR_PersistenceSystem scripted = SCR_PersistenceSystem.GetScriptedInstance();
		if (scripted)
			scripted.GetOnStateChanged().Remove(HandleStateChange);

		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		ChimeraWorld world = GetGame().GetWorld();
		if (!lobbyManager || !lobbyManager.GetMapLineStore() || !world)
			return;

		WorldTimestamp now = world.GetServerTimestamp();
		array<ref LL_MapLine> lines = {};
		foreach (LL_MapLineRecord record : m_aPending)
		{
			LL_MapLine line = new LL_MapLine();
			line.m_iId = record.id;
			line.m_sFactionKey = record.factionKey;
			line.m_sAuthorKey = record.authorKey;
			line.m_sAuthorName = record.authorName;
			line.m_iColor = record.color;
			line.m_fWidth = record.width;
			line.m_aPoints.Copy(record.points);
			line.m_CreatedAt = now.PlusSeconds(-record.age);
			lines.Insert(line);
		}

		int restored = lobbyManager.GetMapLineStore().Restore_S(lines);

		Print(string.Format("[LL_Lobby] Resume: %1 map line(s) restored", restored), LogLevel.NORMAL);
		m_aPending = null;
	}
}

class LL_MapLineSerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	override static typename GetTargetType()
	{
		return LL_MapLineData;
	}

	override ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		LL_LobbyManager lobbyManager = LL_LobbyManager.GetInstance();
		ChimeraWorld world = GetGame().GetWorld();
		if (!lobbyManager || !lobbyManager.GetMapLineStore() || !world)
			return ESerializeResult.DEFAULT;

		WorldTimestamp now = world.GetServerTimestamp();
		array<ref LL_MapLineRecord> records = {};
		foreach (LL_MapLine line : lobbyManager.GetMapLineStore().GetLines())
		{
			LL_MapLineRecord record = new LL_MapLineRecord();
			record.id = line.m_iId;
			record.factionKey = line.m_sFactionKey;
			record.authorKey = line.m_sAuthorKey;
			record.authorName = line.m_sAuthorName;
			record.color = line.m_iColor;
			record.width = line.m_fWidth;
			record.points.Copy(line.m_aPoints);
			record.age = now.DiffSeconds(line.m_CreatedAt);
			records.Insert(record);
		}

		if (records.IsEmpty())
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", VERSION);
		bool prev = context.EnableTypeDiscriminator(false);
		context.WriteValue("lines", records);
		context.EnableTypeDiscriminator(prev);
		return ESerializeResult.OK;
	}

	override bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		LL_MapLineData state = LL_MapLineData.Cast(instance);
		if (!state)
			return true;

		int version;
		context.ReadValue("version", version);
		if (version != VERSION)
		{
			Print(string.Format("[LL_Lobby] Resume: map line record version %1 unknown, lines are dropped", version), LogLevel.WARNING);
			return true;
		}

		array<ref LL_MapLineRecord> records = {};
		bool prev = context.EnableTypeDiscriminator(false);
		context.ReadValue("lines", records);
		context.EnableTypeDiscriminator(prev);
		if (records.IsEmpty())
			return true;

		state.SetPending(records);
		SCR_PersistenceSystem scripted = SCR_PersistenceSystem.Cast(GetSystem());
		if (scripted)
			scripted.GetOnStateChanged().Insert(state.HandleStateChange);

		return true;
	}
}
