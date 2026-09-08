// Confines players to (or out of) a polygon zone, entirely on each client for its own
// player: a HUD warning counts down on the wrong side, then the client asks the server
// to kill it. The shape is baked into the world; the only message is one kill request.
// Subclasses override IsEnforcementActive to time-box it.

class LL_ZoneRestrictionComponentClass : ScriptComponentClass
{
}

class LL_ZoneRestrictionComponent : ScriptComponent
{
	[Attribute("{1D11B2C3D4E5F6A7}UI/HUD/ZoneExitWarning.layout", UIWidgets.ResourceNamePicker, "HUD warning shown while the player is on the wrong side.", params: "layout", category: "Lite Lobby")]
	protected ResourceName m_sWarningLayout;

	[Attribute("10", UIWidgets.EditBox, "Seconds to return before the player is killed.", category: "Lite Lobby")]
	protected float m_fReturnSeconds;

	[Attribute("0", UIWidgets.CheckBox, "Lethal to ENTER (on) instead of lethal to LEAVE (off).", category: "Lite Lobby")]
	protected bool m_bLethalToEnter;

	[Attribute("", UIWidgets.Auto, "Factions affected (empty = all).", category: "Lite Lobby")]
	protected ref array<FactionKey> m_aTargetFactions;

	protected ShapeEntity m_Shape;
	protected ref array<float> m_aPolygon2D = {};
	protected Widget m_wWarning;
	protected TextWidget m_wWarningCounter;
	protected bool m_bTimerActive;
	protected float m_fTimerRemaining = -1;
	protected bool m_bHasEnteredZone;
	protected IEntity m_PrevControlledEntity;

	// The freeze-time HUD hides itself while a return-to-zone warning is up.
	protected static int s_iActiveWarnings;

	static bool IsAnyWarningActive()
	{
		return s_iActiveWarnings > 0;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Shape = ShapeEntity.Cast(owner);
		if (!m_Shape)
			return;
		if (!m_aTargetFactions)
			m_aTargetFactions = {};

		// The shape's points are not reliably queryable at OnPostInit.
		GetGame().GetCallqueue().CallLater(BuildPolygon, 0, false);

		SetEventMask(owner, EntityEvent.FRAME);
	}

	protected void BuildPolygon()
	{
		if (!m_Shape)
			return;
		array<vector> pts = {};
		m_Shape.GetPointsPositions(pts);
		if (pts.Count() < 3)
			return;

		m_aPolygon2D.Clear();
		vector origin = m_Shape.GetOrigin();
		foreach (vector p : pts)
		{
			vector wp = p + origin;
			m_aPolygon2D.Insert(wp[0]);
			m_aPolygon2D.Insert(wp[2]);
		}
	}

	// Subclasses gate enforcement (e.g. only during freeze time). Default: always.
	protected bool IsEnforcementActive()
	{
		return true;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;

		IEntity controlled = pc.GetControlledEntity();
		if (controlled != m_PrevControlledEntity)
		{
			// Possession changed: start fresh.
			ResetState();
			m_PrevControlledEntity = controlled;
		}

		if (!controlled || m_aPolygon2D.Count() < 6)
			return;

		if (!IsEnforcementActive())
		{
			if (m_bTimerActive || m_bHasEnteredZone)
				ResetState();
			return;
		}

		DamageManagerComponent dmg = DamageManagerComponent.Cast(controlled.FindComponent(DamageManagerComponent));
		if (dmg && dmg.GetState() == EDamageState.DESTROYED)
		{
			ResetState();
			return;
		}

		if (!IsTarget(controlled))
			return;

		bool onSafeSide = IsInside(controlled.GetOrigin());
		if (m_bLethalToEnter)
			onSafeSide = !onSafeSide;

		if (onSafeSide)
		{
			m_bHasEnteredZone = true;
			if (m_bTimerActive)
				StopWarning();
			return;
		}

		// Armed only once the player has been on the safe side, so spawning outside does
		// not kill instantly.
		if (!m_bHasEnteredZone)
			return;

		if (!m_bTimerActive)
		{
			m_bTimerActive = true;
			m_fTimerRemaining = m_fReturnSeconds;
			ShowWarning();
		}
		else
		{
			m_fTimerRemaining -= timeSlice;
			UpdateWarning();
			if (m_fTimerRemaining <= 0)
			{
				RequestKill();
				ResetState();
			}
		}
	}

	protected bool IsTarget(IEntity ent)
	{
		if (m_aTargetFactions.IsEmpty())
			return true;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return false;
		Faction f = fac.GetAffiliatedFaction();
		if (!f)
			return false;
		return m_aTargetFactions.Contains(f.GetFactionKey());
	}

	protected bool IsInside(vector pos)
	{
		float px = pos[0];
		float pz = pos[2];
		int n = m_aPolygon2D.Count() / 2;
		bool inside = false;
		int j = n - 1;
		for (int i = 0; i < n; j = i++)
		{
			float xi = m_aPolygon2D[i * 2];
			float zi = m_aPolygon2D[i * 2 + 1];
			float xj = m_aPolygon2D[j * 2];
			float zj = m_aPolygon2D[j * 2 + 1];
			if ((zi > pz) != (zj > pz) && px < (xj - xi) * (pz - zi) / (zj - zi) + xi)
				inside = !inside;
		}
		return inside;
	}

	protected void RequestKill()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;
		LL_LobbyPlayerComponent comp = LL_LobbyPlayerComponent.Cast(pc.FindComponent(LL_LobbyPlayerComponent));
		if (comp)
			comp.RequestKillOutsideZone();
	}

	protected void ShowWarning()
	{
		if (m_sWarningLayout == "")
			return;
		m_wWarning = GetGame().GetWorkspace().CreateWidgets(m_sWarningLayout, null);
		if (m_wWarning)
		{
			m_wWarningCounter = TextWidget.Cast(m_wWarning.FindAnyWidget("Counter"));
			s_iActiveWarnings++;
		}
		UpdateWarning();
	}

	protected void UpdateWarning()
	{
		if (!m_wWarningCounter)
			return;
		int total = Math.Ceil(m_fTimerRemaining);
		if (total < 0)
			total = 0;
		int minutes = total / 60;
		int seconds = total % 60;
		m_wWarningCounter.SetTextFormat("%1:%2", minutes.ToString(2), seconds.ToString(2));
	}

	protected void StopWarning()
	{
		m_bTimerActive = false;
		m_fTimerRemaining = -1;
		if (m_wWarning)
		{
			m_wWarning.RemoveFromHierarchy();
			m_wWarning = null;
			m_wWarningCounter = null;
			s_iActiveWarnings--;
		}
	}

	protected void ResetState()
	{
		StopWarning();
		m_bHasEnteredZone = false;
	}

	void ~LL_ZoneRestrictionComponent()
	{
		if (m_wWarning)
		{
			s_iActiveWarnings--;
			m_wWarning.RemoveFromHierarchy();
		}
	}
}