// The game deletes every saved player character whose owner is not back within its
// reconnect timeout after a load; the lobby holds a slot for the whole game and its own
// session record reserves bodies for absent holders. Writing nothing also keeps the
// game from scheduling that deletion. The base class is repeated because a bare modded
// config-container class loads as "Unknown class".

modded class SCR_PlayerReconnectDataSerializer : ScriptedStateSerializer
{
	override ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		return ESerializeResult.DEFAULT;
	}

	override bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		return true;
	}
}
