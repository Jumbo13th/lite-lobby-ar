// Blocks damage while the protected freeze period is active. ComputeEffectiveDamage is
// the authority-side funnel every hit passes through, character hit zones included.
// EDamageType.TRUE passes: the zone-exit kill routes through this method too, and TRUE
// is reserved for scripted death. Pairs with the client-side trigger lock in
// LL_M_SCR_CharacterControllerComponent as the authoritative backstop.
modded class SCR_HitZone
{
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		if (mode && mode.IsFreezeSafetyActive() && damageContext.damageType != EDamageType.TRUE)
			return 0;

		return super.ComputeEffectiveDamage(damageContext, isDOT);
	}
}