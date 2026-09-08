// Client-side trigger lock during the protected freeze period. OnControlledByPlayer
// fires with controlled == true only on the owning machine, where firing is predicted,
// so the shot stops at the input. A periodic tick keeps SetWeaponNoFireTime topped up
// (0.5 s window over a 0.25 s tick) and re-evaluates the freeze, whose length is
// dynamic. Authoritative damage immunity is LL_M_SCR_HitZone.
modded class SCR_CharacterControllerComponent
{
	protected bool m_bLLFireLocked;

	override protected void OnControlledByPlayer(IEntity owner, bool controlled)
	{
		super.OnControlledByPlayer(owner, controlled);

		// `controlled` is true for every player-controlled character on every machine,
		// the dedicated server included; act only for the local player's own character.
		bool isLocal = controlled && owner && owner == SCR_PlayerController.GetLocalControlledEntity();

		// Repossession must never stack duplicate repeating timers.
		GetGame().GetCallqueue().Remove(LL_FreezeFireTick);
		m_bLLFireLocked = false;

		if (isLocal)
			GetGame().GetCallqueue().CallLater(LL_FreezeFireTick, 250, true);
	}

	protected void LL_FreezeFireTick()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		bool active = mode && mode.IsFreezeSafetyActive();

		if (active)
		{
			SetWeaponNoFireTime(0.5);
			m_bLLFireLocked = true;
		}
		else if (m_bLLFireLocked)
		{
			// Drop the lock now instead of waiting the window out.
			SetWeaponNoFireTime(0);
			m_bLLFireLocked = false;
		}
	}
}