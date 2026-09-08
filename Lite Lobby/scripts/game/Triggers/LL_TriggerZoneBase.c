// Shared base of the zone triggers: the shape, the two sides, the flags, the
// statistics pool and the per-tick headcount inside. Declares the contract
// LL_StatsManager reads off a zone trigger. Subclasses drive the tick.

class LL_TriggerZoneBaseClass : LL_TriggerComponentClass
{
}

class LL_TriggerZoneBase : LL_TriggerComponent
{
	[Attribute("", UIWidgets.EditBox, "Name of the ShapeEntity zone. Empty = this component's own owner.", category: "Lite Lobby")]
	protected string m_sZoneEntityName;

	[Attribute("", UIWidgets.EditBox, "Friendly zone name shown in the briefing text (e.g. 'the Bridge'). Empty = the zone's editable/entity name. The map link still uses the entity name.", category: "Lite Lobby")]
	protected string m_sZoneDisplayName;

	[Attribute("USSR", UIWidgets.EditBox, "Attacking faction — does not own the zone at mission start.", category: "Lite Lobby")]
	protected FactionKey m_sAttackerFaction;

	[Attribute("US", UIWidgets.EditBox, "Defending faction — starts owning the zone and its flags.", category: "Lite Lobby")]
	protected FactionKey m_sDefenderFaction;

	[Attribute("", UIWidgets.EditBox, "Flag entity names showing who owns the zone (each needs an SCR_FlagComponent).", category: "Lite Lobby")]
	protected ref array<string> m_aFlagNames;

	[Attribute("0", UIWidgets.EditBox, "Statistics: point pool for this zone. Goes to the units of the side that OWNS it when the game is scored, split by seconds their players spent inside (dying keeps seconds already earned). 0 = no stat award.", category: "Lite Lobby")]
	protected float m_fStatPoints;

	[Attribute("0", UIWidgets.EditBox, "Statistics: per-player cap — a unit's share of the pool never exceeds this × its contributing players. 0 = uncapped.", category: "Lite Lobby")]
	protected float m_fStatMaxPerPlayer;

	protected ref array<float> m_aPolygon2D = {};
	protected ref array<SCR_FlagComponent> m_aFlags = {};

	// False = the shape is unusable and the subclass stays inert.
	protected bool SetupZone()
	{
		BuildPolygon(m_sZoneEntityName, GetOwner(), m_aPolygon2D);
		if (m_aPolygon2D.Count() < 6)
			return false;

		ResolveFlags();
		return true;
	}

	protected void ResolveFlags()
	{
		if (!m_aFlagNames)
			return;

		foreach (string name : m_aFlagNames)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByName(name);
			if (!ent)
				continue;

			SCR_FlagComponent flag = SCR_FlagComponent.Cast(ent.FindComponent(SCR_FlagComponent));
			if (flag)
				m_aFlags.Insert(flag);
			else
				Print(string.Format("[LL_Trigger] Zone: '%1' has no SCR_FlagComponent.", name), LogLevel.WARNING);
		}
	}

	// Null guards: the array holds weak refs and a flag deleted mid-game must not abort the zone.

	protected void SetFlagOwner(FactionKey owner)
	{
		foreach (SCR_FlagComponent flag : m_aFlags)
		{
			if (flag)
				flag.LL_SetOwnerFaction_S(owner);
		}
	}

	// The setter drops writes that change nothing.
	protected void SetFlagRaise(float raise)
	{
		foreach (SCR_FlagComponent flag : m_aFlags)
		{
			if (flag)
				flag.LL_SetRaiseLevel_S(raise);
		}
	}

	// Collected once per tick, shared by the ownership check and the presence feed.
	protected void CollectInsideCharacters(notnull array<IEntity> outChars)
	{
		array<IEntity> chars = {};
		CollectAlivePlayableCharacters(chars);
		foreach (IEntity ent : chars)
		{
			if (PointInPolygon(ent.GetOrigin(), m_aPolygon2D))
				outChars.Insert(ent);
		}
	}

	protected void CountByFaction(notnull map<string, int> outCounts, notnull array<IEntity> insideChars)
	{
		outCounts.Clear();

		foreach (IEntity ent : insideChars)
		{
			FactionKey fk = GetEntityFactionKey(ent);
			if (fk == "")
				continue;

			int cur = 0;
			outCounts.Find(fk, cur);
			outCounts.Set(fk, cur + 1);
		}
	}

	override float GetStatPoints()
	{
		return m_fStatPoints;
	}

	override float GetStatMaxPerPlayer()
	{
		return m_fStatMaxPerPlayer;
	}

	FactionKey GetAttackerFaction()
	{
		return m_sAttackerFaction;
	}

	FactionKey GetDefenderFaction()
	{
		return m_sDefenderFaction;
	}

	FactionKey GetZoneHolderFaction()
	{
		return m_sDefenderFaction;
	}

	// True = ownership can still change: the statistics record is never sealed.
	bool IsContestedZone()
	{
		return false;
	}

	string GetZoneStatLabel()
	{
		if (m_sZoneDisplayName != "")
			return m_sZoneDisplayName;
		if (m_sZoneEntityName != "")
			return EntityDisplayName(m_sZoneEntityName);

		string ownName = GetOwner().GetName();
		if (ownName != "")
			return ownName;
		return "zone";
	}

	// The zone as the one clickable object in an objective line.
	protected string ZoneBriefingLink()
	{
		string zone = ObjectLink(m_sZoneEntityName, m_sZoneDisplayName);
		if (zone == "")
			zone = WidgetManager.Translate("#LL-Trigger_ZoneFallback");
		return zone;
	}
}