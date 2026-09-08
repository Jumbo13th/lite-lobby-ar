// Marks a per-player VoN proxy entity and resolves playerId → proxy on every machine.
// Menu voice transmits through a component that is not on the controlled entity, and
// the engine only routes such audio when the sender's entity and radio exist on the
// receiving machine; PlayerControllers replicate owner↔server only. The single RplProp
// int arrives with the entity stream, so JIP clients register with no extra bookkeeping.

class LL_VoNProxyComponentClass : ScriptComponentClass
{
}

class LL_VoNProxyComponent : ScriptComponent
{
	[RplProp(onRplName: "OnPlayerIdChanged")]
	protected int m_iPlayerId = -1;

	protected static ref map<int, LL_VoNProxyComponent> s_mByPlayerId = new map<int, LL_VoNProxyComponent>();

	static LL_VoNProxyComponent GetByPlayerId(int playerId)
	{
		LL_VoNProxyComponent comp;
		if (s_mByPlayerId.Find(playerId, comp))
			return comp;
		return null;
	}

	static IEntity GetProxyEntity(int playerId)
	{
		LL_VoNProxyComponent comp = GetByPlayerId(playerId);
		if (comp)
			return comp.GetOwner();
		return null;
	}

	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	void SetPlayerId_S(int playerId)
	{
		m_iPlayerId = playerId;
		Replication.BumpMe();
		Register();
	}

	protected void OnPlayerIdChanged()
	{
		Register();
	}

	protected void Register()
	{
		if (m_iPlayerId <= 0)
			return;

		s_mByPlayerId.Set(m_iPlayerId, this);

		// On clients the proxy can stream in after the channel data arrived (JIP);
		// registration is the one reliable "this radio exists now" hook.
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
			vonMgr.ApplyRadioKey(m_iPlayerId);
	}

	override void OnDelete(IEntity owner)
	{
		if (m_iPlayerId > 0)
		{
			LL_VoNProxyComponent current;
			if (s_mByPlayerId.Find(m_iPlayerId, current) && current == this)
				s_mByPlayerId.Remove(m_iPlayerId);
		}

		super.OnDelete(owner);
	}
}