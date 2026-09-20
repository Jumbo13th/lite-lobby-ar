// Static map markers as the marker manager replicates them to a joining client. Restored
// markers are server markers: the session-scoped owner id cannot be re-linked, so the
// faction gate and the admin removal keep working while the placer cannot delete their own.

class LL_MarkerRecord
{
	int posX;
	int posY;
	int flags;
	int configId;
	int factionFlags;
	int rotation;
	int type;
	int colorEntry;
	int iconEntry;
	string text;
	bool timestampVisible;
	// Seconds before the snapshot: a world timestamp does not survive a restart.
	float timestampAge;
}

class LL_MarkerData : PersistentState
{
	protected ref array<ref LL_MarkerRecord> m_aPending;

	void SetPending(array<ref LL_MarkerRecord> records)
	{
		m_aPending = records;
	}

	// Inserted once the system is active and the marker manager's own state is settled.
	void HandleStateChange(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		if (newState != EPersistenceSystemState.ACTIVE || !m_aPending)
			return;

		SCR_PersistenceSystem scripted = SCR_PersistenceSystem.GetScriptedInstance();
		if (scripted)
			scripted.GetOnStateChanged().Remove(HandleStateChange);

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
			return;

		WorldTimestamp now = GetGame().GetWorld().GetTimestamp();
		foreach (LL_MarkerRecord record : m_aPending)
		{
			SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
			marker.SetType(record.type);
			marker.SetWorldPos(record.posX, record.posY);
			marker.SetFlags(record.flags);
			marker.SetMarkerConfigID(record.configId);
			marker.SetMarkerFactionFlags(record.factionFlags);
			marker.SetRotation(record.rotation);
			marker.SetColorEntry(record.colorEntry);
			marker.SetIconEntry(record.iconEntry);
			marker.SetCustomText(record.text);
			marker.SetTimestampVisibility(record.timestampVisible);
			if (record.timestampVisible)
				marker.SetTimestamp(now.PlusSeconds(-record.timestampAge));

			manager.InsertStaticMarker(marker, false, true);
		}

		Print(string.Format("[LL_Lobby] Resume: %1 map marker(s) restored", m_aPending.Count()), LogLevel.NORMAL);
		m_aPending = null;
	}
}

class LL_MarkerSerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	override static typename GetTargetType()
	{
		return LL_MarkerData;
	}

	override ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
			return ESerializeResult.DEFAULT;

		array<SCR_MapMarkerBase> markers = manager.GetStaticMarkers();
		foreach (SCR_MapMarkerBase disabled : manager.GetDisabledMarkers())
			markers.Insert(disabled);

		WorldTimestamp now = GetGame().GetWorld().GetTimestamp();
		array<ref LL_MarkerRecord> records = {};
		foreach (SCR_MapMarkerBase marker : markers)
		{
			if (marker.GetMarkerID() == -1)
				continue;

			LL_MarkerRecord record = new LL_MarkerRecord();
			int pos[2];
			marker.GetWorldPos(pos);
			record.posX = pos[0];
			record.posY = pos[1];
			record.flags = marker.GetFlags();
			record.configId = marker.GetMarkerConfigID();
			record.factionFlags = marker.GetMarkerFactionFlags();
			record.rotation = marker.GetRotation();
			record.type = marker.GetType();
			record.colorEntry = marker.GetColorEntry();
			record.iconEntry = marker.GetIconEntry();
			record.text = marker.GetCustomText();
			record.timestampVisible = marker.IsTimestampVisible();
			if (record.timestampVisible)
				record.timestampAge = now.DiffSeconds(marker.GetTimestamp());

			records.Insert(record);
		}

		if (records.IsEmpty())
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", VERSION);
		bool prev = context.EnableTypeDiscriminator(false);
		context.WriteValue("markers", records);
		context.EnableTypeDiscriminator(prev);
		return ESerializeResult.OK;
	}

	override bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		LL_MarkerData state = LL_MarkerData.Cast(instance);
		if (!state)
			return true;

		int version;
		context.ReadValue("version", version);
		if (version != VERSION)
		{
			Print(string.Format("[LL_Lobby] Resume: marker record version %1 unknown, markers are dropped", version), LogLevel.WARNING);
			return true;
		}

		array<ref LL_MarkerRecord> records = {};
		bool prev = context.EnableTypeDiscriminator(false);
		context.ReadValue("markers", records);
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
