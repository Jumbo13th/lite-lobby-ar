// A zone that can be taken and retaken all game; the pool follows whoever holds it at
// publish. Progress is a signed count of seconds of control from -capture (defender's
// end) through 0 (neutral) to +capture (attacker's end). The dominating side pushes it;
// with nobody dominating it slides back to the owner's end. A side owns the zone at its
// own end and loses it to nobody at zero, so converting a held zone is two equal pieces
// of work. Domination = at least Min Presence inside and more than Domination Ratio
// times the other side. The flag follows the bar, not ownership: height = |progress|,
// flag = the side the bar is on.

class LL_TriggerZoneContestClass : LL_TriggerZoneBaseClass
{
}

class LL_TriggerZoneContest : LL_TriggerZoneBase
{
	[Attribute("0", UIWidgets.CheckBox, "Nobody owns the zone at mission start (the pole flies its own white flag, and no side is credited if the zone is never taken). Unticked, the Defender Faction starts holding it.", category: "Lite Lobby")]
	protected bool m_bStartsNeutral;

	[Attribute("180", UIWidgets.EditBox, "Seconds of unbroken domination needed to take a NEUTRAL zone. Taking one the other side holds costs this twice: once to push it back to neutral, once to claim it.", category: "Lite Lobby")]
	protected float m_fCaptureSeconds;

	[Attribute("1", UIWidgets.EditBox, "Minimum alive playables a side needs inside before it can push the zone at all. Keeps an empty zone from drifting.", category: "Lite Lobby")]
	protected int m_iMinPresence;

	[Attribute("1", UIWidgets.EditBox, "A side must have MORE than this many times the other side's count inside to dominate. 1 = simple majority; 2 = must more than double them.", category: "Lite Lobby")]
	protected float m_fDominationRatio;

	[Attribute("30", UIWidgets.EditBox, "Minimum seconds between two announced changes of ownership. Ownership still flips — only the HUD banner is held back.", category: "Lite Lobby")]
	protected float m_fAnnounceCooldown;

	// Seconds, not a 0..1 fraction: adding 1/180 to a float 180 times lands under 1.0
	// and delays every capture by a tick.
	protected float m_fProgress;

	protected FactionKey m_sHolder;
	protected float m_fLastAnnounceTime = -99999;

	// A capture time near the tick rate would flip ownership every tick when the sides
	// alternate, and each flip writes a statistics timeline entry.
	protected const float MIN_CAPTURE_SECONDS = 30;

	override string GetObjectiveMarkup()
	{
		// The zone is named once, in the opening sentence; threading it into the rules
		// sentence breaks the word order in other languages.
		string zone = ZoneBriefingLink();

		string start;
		if (m_bStartsNeutral)
			start = WidgetManager.Translate("#LL-Trigger_ContestStartNeutral", zone);
		else
			start = WidgetManager.Translate("#LL-Trigger_ContestStartOwned", zone,
				WidgetManager.Translate(FactionName(GetDefenderFaction())));

		string body = WidgetManager.Translate("#LL-Trigger_ZoneContestBody",
			DescribeDomination(),
			FormatDuration(m_fCaptureSeconds));

		return Header("#LL-Trigger_HeaderZoneContest") + start + " " + body;
	}

	protected string DescribeDomination()
	{
		// x2 and x3 get their own wording: Russian counts do not agree with a bare numeral.
		string phrase;
		if (m_fDominationRatio <= 1)
			phrase = WidgetManager.Translate("#LL-Trigger_ContestDomMajority");
		else if (m_fDominationRatio == 2)
			phrase = WidgetManager.Translate("#LL-Trigger_ContestDomTwice");
		else if (m_fDominationRatio == 3)
			phrase = WidgetManager.Translate("#LL-Trigger_ContestDomThrice");
		else
		{
			int whole = m_fDominationRatio;
			string ratio = m_fDominationRatio.ToString();
			if (whole == m_fDominationRatio)
				ratio = whole.ToString();

			phrase = WidgetManager.Translate("#LL-Trigger_ContestDomRatio", ratio);
		}

		if (m_iMinPresence > 1)
			phrase = WidgetManager.Translate("#LL-Trigger_ContestDomMin", phrase, m_iMinPresence.ToString());

		return phrase;
	}

	override protected void OnActivate()
	{
		if (!SetupZone())
		{
			Print("[LL_Trigger] ZoneContest: zone shape not found or too few points — trigger inert.", LogLevel.WARNING);
			return;
		}

		if (m_fCaptureSeconds < MIN_CAPTURE_SECONDS)
		{
			Print(string.Format("[LL_Trigger] ZoneContest '%1': capture seconds %2 is too short — raised to %3.",
				GetZoneStatLabel(), m_fCaptureSeconds.ToString(), MIN_CAPTURE_SECONDS.ToString()), LogLevel.WARNING);
			m_fCaptureSeconds = MIN_CAPTURE_SECONDS;
		}

		if (m_iMinPresence < 1)
			m_iMinPresence = 1;
		if (m_fDominationRatio < 1)
			m_fDominationRatio = 1;

		// Both leave a zone that can never change hands, which looks like a broken trigger.
		if (GetAttackerFaction() == "")
			Print(string.Format("[LL_Trigger] ZoneContest '%1': no attacker faction — nobody can ever take this zone.", GetZoneStatLabel()), LogLevel.WARNING);
		else if (GetAttackerFaction() == GetDefenderFaction())
			Print(string.Format("[LL_Trigger] ZoneContest '%1': attacker and defender are the same faction — the zone can never move.", GetZoneStatLabel()), LogLevel.WARNING);

		// No defender means nothing starts owning it.
		if (GetDefenderFaction() == "")
			m_bStartsNeutral = true;

		if (m_bStartsNeutral)
		{
			m_sHolder = "";
			m_fProgress = 0;
		}
		else
		{
			m_sHolder = GetDefenderFaction();
			m_fProgress = -m_fCaptureSeconds;
		}

		SetFlagOwner(FlagFaction());
		SetFlagRaise(RaiseLevel());

		GetGame().GetCallqueue().CallLater(PollTick, TICK_MS, true);
	}

	// Never disarmed by a condition; the base's one-shot Fire latch is unused.
	override protected void OnDisarm()
	{
		GetGame().GetCallqueue().Remove(PollTick);
	}

	void ~LL_TriggerZoneContest()
	{
		GetGame().GetCallqueue().Remove(PollTick);
	}

	protected void PollTick()
	{
		// Leaving GAME stops the whole tick, so the bar stays where the fight left it.
		if (!IsEvaluationAllowed())
			return;

		array<IEntity> inside = {};
		CollectInsideCharacters(inside);

		// Presence for both sides: which side is paid is unknown until publish.
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			stats.RecordZonePresence(this, inside, TICK_MS / 1000.0);

		int attackers, defenders;
		CountSides(inside, attackers, defenders);

		// Each half of the bar costs m_fCaptureSeconds.
		float dt = TICK_MS / 1000.0;

		if (Dominates(attackers, defenders))
			m_fProgress += dt;
		else if (Dominates(defenders, attackers))
			m_fProgress -= dt;
		else
			DecayTowardRest(dt);

		m_fProgress = Math.Clamp(m_fProgress, -m_fCaptureSeconds, m_fCaptureSeconds);

		UpdateOwnership();

		// Every tick: the flag follows the bar, and the setters drop no-op writes.
		SetFlagOwner(FlagFaction());
		SetFlagRaise(RaiseLevel());
	}

	// The one place seconds become the setter's 0..1 fraction.
	protected float RaiseLevel()
	{
		return Math.AbsFloat(m_fProgress) / m_fCaptureSeconds;
	}

	// The side the bar is on, deliberately not the owner.
	protected FactionKey FlagFaction()
	{
		if (m_fProgress > 0)
			return GetAttackerFaction();
		if (m_fProgress < 0)
			return GetDefenderFaction();

		return "";
	}

	// The owner's end, or neutral.
	protected float RestPoint()
	{
		if (m_sHolder == "")
			return 0;
		if (m_sHolder == GetAttackerFaction())
			return m_fCaptureSeconds;
		return -m_fCaptureSeconds;
	}

	// Rest is always on the owner's side of zero, so decay only restores the status quo.
	protected void DecayTowardRest(float dt)
	{
		float rest = RestPoint();
		if (m_fProgress < rest)
			m_fProgress = Math.Min(m_fProgress + dt, rest);
		else if (m_fProgress > rest)
			m_fProgress = Math.Max(m_fProgress - dt, rest);
	}

	// Counted directly: the base's faction histogram hashes every faction inside.
	protected void CountSides(notnull array<IEntity> chars, out int attackers, out int defenders)
	{
		attackers = 0;
		defenders = 0;

		FactionKey att = GetAttackerFaction();
		FactionKey def = GetDefenderFaction();

		foreach (IEntity ent : chars)
		{
			FactionKey fk = GetEntityFactionKey(ent);
			if (fk == "")
				continue;

			// Two separate ifs: a trigger with the same key on both sides then deadlocks
			// instead of handing that side a free capture.
			if (fk == att)
				attackers++;
			if (fk == def)
				defenders++;
		}
	}

	// Strict > means a tie is nobody's.
	protected bool Dominates(int own, int other)
	{
		return own >= m_iMinPresence && own > other * m_fDominationRatio;
	}

	// Tested against the rails rather than a sign flip: a tick can step over zero.
	protected void UpdateOwnership()
	{
		FactionKey owner = m_sHolder;

		if (m_fProgress >= m_fCaptureSeconds)
			owner = GetAttackerFaction();
		else if (m_fProgress <= -m_fCaptureSeconds)
			owner = GetDefenderFaction();
		else if (m_sHolder == GetAttackerFaction() && m_fProgress <= 0)
			owner = "";
		else if (m_sHolder == GetDefenderFaction() && m_fProgress >= 0)
			owner = "";

		if (owner == m_sHolder)
			return;

		m_sHolder = owner;

		// Every change of hands goes on the timeline; MIN_CAPTURE_SECONDS bounds the rate.
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
			stats.NotifyZoneOwnerChanged(this, m_sHolder);

		AnnounceFlip();
	}

	// Rate-limited HUD banner.
	protected void AnnounceFlip()
	{
		if (!m_bBroadcastMessage)
			return;

		// GetTickCount is a 32-bit millisecond counter and wraps after ~25 days.
		float now = System.GetTickCount() / 1000.0;
		if (now < m_fLastAnnounceTime)
			m_fLastAnnounceTime = now - m_fAnnounceCooldown;

		if (now - m_fLastAnnounceTime < m_fAnnounceCooldown)
			return;
		m_fLastAnnounceTime = now;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		// Losing a zone to nobody is its own announcement.
		string template;
		if (m_sHolder == "")
		{
			template = "#LL-Trigger_ZoneNeutralMessage";
		}
		else
		{
			// The base Message overrides the wording: %1 = zone, %2 = the new owner.
			template = m_sMessage;
			if (template == "")
				template = "#LL-Trigger_ZoneFlipMessage";
		}

		// Key + parts: each client renders zone and faction names in its own language.
		mgr.BroadcastZoneFlip_S(template, GetZoneStatLabel(), m_sHolder, m_sTitle);
	}

	// Never sealed: the pool follows the holder on every publish.
	override bool IsContestedZone()
	{
		return true;
	}

	override FactionKey GetZoneHolderFaction()
	{
		return m_sHolder;
	}
}