// World Editor arrow along a spawn point's forward axis, the direction a line drop runs in.
// Workbench builds only: nothing of it exists in the game.

class LL_SpawnPointArrowComponentClass : ScriptComponentClass
{
}

class LL_SpawnPointArrowComponent : ScriptComponent
{
	[Attribute("250", UIWidgets.EditBox, "Arrow length (m) in the World Editor.", category: "Lite Lobby")]
	protected float m_fLength;

#ifdef WORKBENCH
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		vector origin = owner.GetOrigin();
		vector direction = owner.GetTransformAxis(2);
		direction[1] = 0;
		direction.Normalize();
		Shape.CreateArrow(origin, origin + direction * m_fLength, 3, Color.ORANGE, ShapeFlags.ONCE);
	}
#endif
}
