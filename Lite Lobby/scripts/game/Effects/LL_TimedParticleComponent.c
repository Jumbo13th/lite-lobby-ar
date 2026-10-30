// Plays a particle effect the mission maker placed for a set time, from the end of freeze or from
// the start of the game phase. Every machine that draws particles decides on its own from the game
// state, the freeze countdown and the mission clock it already receives: no replication.

class LL_TimedParticleComponentClass : ScriptComponentClass
{
}

class LL_TimedParticleComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Start when freeze time ends. Off = when the game starts, before freeze.", category: "Lite Lobby")]
	protected bool m_bAtFreezeEnd;

	[Attribute("900", UIWidgets.EditBox, "Seconds the effect plays. The particle must play until it is stopped, as the addon's Particles/LL_SignalSmoke_Red.ptc does.", category: "Lite Lobby")]
	protected float m_fBurnSeconds;

	protected static const int TICK_MS = 1000;

	protected bool m_bStopped;
	protected bool m_bLit;
	protected float m_fStartedAt;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);

		super.OnDelete(owner);
	}

	protected void Tick()
	{
		ParticleEffectEntity effect = ParticleEffectEntity.Cast(GetOwner());
		if (!effect)
		{
			Print(string.Format("[LL_TimedParticle] %1 is not a particle effect; nothing is timed", GetOwner().GetName()), LogLevel.WARNING);
			GetGame().GetCallqueue().Remove(Tick);
			return;
		}

		// The game plays a placed particle from the moment the world loads.
		if (!m_bStopped)
		{
			effect.Stop();
			m_bStopped = true;
		}

		LL_GameModeCoop gm = LL_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gm)
			return;

		if (!m_bLit)
		{
			if (gm.GetState() == SCR_EGameModeState.DEBRIEFING)
			{
				GetGame().GetCallqueue().Remove(Tick);
				return;
			}

			if (!ShouldLight(gm))
				return;

			m_bLit = true;
			effect.Play();
		}

		if (gm.GetState() == SCR_EGameModeState.GAME && gm.GetElapsedTime() - m_fStartedAt < m_fBurnSeconds)
			return;

		// Only the emitters stop: the smoke already in the air thins out instead of vanishing.
		effect.StopEmission();
		GetGame().GetCallqueue().Remove(Tick);
	}

	protected bool ShouldLight(LL_GameModeCoop gm)
	{
		if (gm.GetState() != SCR_EGameModeState.GAME)
			return false;

		if (!m_bAtFreezeEnd)
		{
			m_fStartedAt = 0;
			return true;
		}

		if (gm.GetFreezeTimeRemaining() > 0)
			return false;

		m_fStartedAt = gm.GetFreezeEndedAt();
		return true;
	}
}
