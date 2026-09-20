// Mission triggers as they stood at the snapshot: one typed state per trigger, keyed by
// its stat key, polymorphic through the context's type discriminator.

class LL_TriggerState
{
	string key;
	bool started;
	bool fired;
}

class LL_TriggerTimedState : LL_TriggerState
{
	// Mission-clock reading the countdown began at.
	float startedAt;
}

class LL_TriggerZoneCaptureState : LL_TriggerState
{
	float held;
	bool captured;
}

class LL_TriggerZoneContestState : LL_TriggerState
{
	float progress;
	string holder;
}

class LL_TriggerSupremacyState : LL_TriggerState
{
	bool bothSeen;
}

class LL_TriggerData : PersistentState
{
}

class LL_TriggerSerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	override static typename GetTargetType()
	{
		return LL_TriggerData;
	}

	override ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		LL_GameModeCoop gm = LL_GameModeCoop.GetInstance();
		if (!gm || gm.GetState() != SCR_EGameModeState.GAME)
			return ESerializeResult.DEFAULT;

		array<ref LL_TriggerState> states = {};
		foreach (LL_TriggerComponent trigger : LL_TriggerComponent.GetAll())
		{
			if (!trigger)
				continue;

			LL_TriggerState state = trigger.CaptureState();
			if (state)
				states.Insert(state);
		}

		if (states.IsEmpty())
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", VERSION);
		bool prev = context.EnableTypeDiscriminator(true);
		context.WriteValue("triggers", states);
		context.EnableTypeDiscriminator(prev);
		return ESerializeResult.OK;
	}

	override bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		int version;
		context.ReadValue("version", version);
		if (version != VERSION)
		{
			Print(string.Format("[LL_Lobby] Resume: trigger record version %1 unknown, triggers start fresh", version), LogLevel.WARNING);
			return true;
		}

		array<ref LL_TriggerState> states = {};
		bool prev = context.EnableTypeDiscriminator(true);
		context.ReadValue("triggers", states);
		context.EnableTypeDiscriminator(prev);

		int restored = 0;
		foreach (LL_TriggerState state : states)
		{
			if (!state)
				continue;

			LL_TriggerComponent trigger = LL_TriggerComponent.FindByStatKey(state.key);
			if (!trigger)
			{
				Print(string.Format("[LL_Lobby] Resume: no trigger named '%1' in this world, its record is dropped", state.key), LogLevel.WARNING);
				continue;
			}

			trigger.ApplyState(state);
			restored++;
		}

		Print(string.Format("[LL_Lobby] Resume: %1 trigger record(s) applied", restored), LogLevel.NORMAL);
		return true;
	}
}
