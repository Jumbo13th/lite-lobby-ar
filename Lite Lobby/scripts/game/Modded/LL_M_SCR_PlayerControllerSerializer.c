// The controller record feeds a respawn system the lobby stubs to null; the lobby's slot
// record carries the body a player gets back. The base class is repeated because a bare
// modded config-container class loads as "Unknown class".

modded class SCR_PlayerControllerSerializer : ScriptedEntitySerializer
{
	override protected ESerializeResult Serialize(notnull IEntity entity, notnull SaveContext context)
	{
		return ESerializeResult.DEFAULT;
	}

	override protected bool Deserialize(notnull IEntity entity, notnull LoadContext context)
	{
		return true;
	}
}
