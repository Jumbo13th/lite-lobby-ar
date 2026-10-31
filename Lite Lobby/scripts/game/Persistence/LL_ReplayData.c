class LL_ReplayEntityPair
{
	string persistentId;
	int eid;
}

class LL_ReplayPlayerPair
{
	string identity;
	int pref;
}

class LL_ReplayWeaponEntry
{
	string name;
	string type;
	int wid;
}

//! What a crash-resumed game needs to continue its recording in the next segment with the
//! same recording ids.
class LL_ReplayContinuation
{
	string session;
	int segment;
	int snapshotT;
	int nextEid;
	int nextPref;
	int nextWid;
	ref array<ref LL_ReplayEntityPair> entities = {};
	ref array<ref LL_ReplayPlayerPair> players = {};
	ref array<ref LL_ReplayWeaponEntry> weapons = {};
}

//! Kept apart from the session record, whose strict version check would refuse every older
//! snapshot if replay fields were added to it.
class LL_ReplayData : PersistentState
{
}

class LL_ReplaySerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	override static typename GetTargetType()
	{
		return LL_ReplayData;
	}

	override ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		LL_ReplayRecorderComponent recorder = LL_ReplayRecorderComponent.GetInstance();
		if (!recorder)
			return ESerializeResult.DEFAULT;

		LL_ReplayContinuation continuation = recorder.CaptureContinuation_S();
		if (!continuation)
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", VERSION);
		bool prev = context.EnableTypeDiscriminator(false);
		context.WriteValue("replay", continuation);
		context.EnableTypeDiscriminator(prev);
		return ESerializeResult.OK;
	}

	//! An unreadable record starts a new recording on resume; it never refuses the resume.
	override bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		int version;
		context.ReadValue("version", version);
		if (version != VERSION)
		{
			Print(string.Format("[LL_Lobby] Resume: replay record version %1 unknown, a new recording starts", version), LogLevel.WARNING);
			return true;
		}

		LL_ReplayContinuation continuation = new LL_ReplayContinuation();
		bool prev = context.EnableTypeDiscriminator(false);
		bool read = context.ReadValue("replay", continuation);
		context.EnableTypeDiscriminator(prev);

		if (read)
			LL_ReplayRecorderComponent.SetPendingContinuation_S(continuation);

		return true;
	}
}
