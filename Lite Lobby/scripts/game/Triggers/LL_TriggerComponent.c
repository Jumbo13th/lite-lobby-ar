// Base for mission triggers: a server-side condition whose only effect is a one-off
// HUD notice (everyone, or a faction and/or squads). Zero replication beyond the
// existing broadcast. Subclasses override OnActivate (poll, subscribe or schedule),
// call Fire once, and OnDisarm tears detection down. Detection starts at GAME.

class LL_TriggerComponentClass : ScriptComponentClass
{
}

class LL_TriggerComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Broadcast the message below when this trigger fires.", category: "Lite Lobby")]
	protected bool m_bBroadcastMessage;

	[Attribute("#LL-AdminMessage_Announcement", UIWidgets.EditBox, "Banner header shown above the message. Plain text or a localization key (#LL-...) — keys are translated on each player's client in their own language.", category: "Lite Lobby")]
	protected string m_sTitle;

	[Attribute("", UIWidgets.EditBoxMultiline, "Message flashed on every player's HUD when this trigger fires (if broadcasting is on). Plain text or a localization key (#LL-...).", category: "Lite Lobby")]
	protected string m_sMessage;

	[Attribute("", UIWidgets.EditBox, "Show the fired message only to players slotted in this faction (e.g. US, USSR). Empty = both sides.", category: "Lite Lobby")]
	protected FactionKey m_sMessageFaction;

	[Attribute("", UIWidgets.EditBox, "Editor names of squad group entities (SCR_AIGroup) — the message is shown only to players slotted in these squads. Empty = no squad filter.", category: "Lite Lobby")]
	protected ref array<string> m_aMessageGroups;

	[Attribute("1", UIWidgets.CheckBox, "List this trigger's auto-generated objective in the map's Objectives panel.", category: "Lite Lobby")]
	protected bool m_bShowInBriefing;

	// 1 Hz.
	protected const int TICK_MS = 1000;

	protected bool m_bFired;
	protected bool m_bActivated;

	// Stable key for the statistics snapshot: the owner's entity name, else a per-load
	// sequence id. Website-facing.
	protected static int s_iNextStatKey;
	protected string m_sStatKey;

	// Kept on all machines: the Objectives description is built client-side. Flushed on
	// a world change like LL_MissionDescription's registry.
	protected static ref array<LL_TriggerComponent> s_aBriefingTriggers = {};
	protected static BaseWorld s_RegistryWorld;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		RegisterForBriefing(owner);

		if (!Replication.IsServer())
			return;

		// Triggers live on world entities the game mode does not know about, so they pull
		// the state instead of being pushed it.
		GetGame().GetCallqueue().CallLater(TryActivate, TICK_MS, true);
	}

	protected void RegisterForBriefing(IEntity owner)
	{
		// Statics survive a scenario restart.
		BaseWorld world = owner.GetWorld();
		if (world != s_RegistryWorld)
		{
			s_aBriefingTriggers.Clear();
			s_iNextStatKey = 0;
			s_RegistryWorld = world;
		}

		if (!s_aBriefingTriggers.Contains(this))
			s_aBriefingTriggers.Insert(this);
	}

	void ~LL_TriggerComponent()
	{
		if (s_aBriefingTriggers)
			s_aBriefingTriggers.RemoveItem(this);
	}

	protected void TryActivate()
	{
		if (m_bActivated)
			return;
		if (!IsEvaluationAllowed())
			return;

		m_bActivated = true;
		GetGame().GetCallqueue().Remove(TryActivate);
		OnActivate();
	}

	// Called once, on authority, after the gate opens.
	protected void OnActivate()
	{
	}

	// Called on fire.
	protected void OnDisarm()
	{
	}

	// Evaluation only in GAME (freeze included). The hard freeze folds in here so every
	// trigger suspends at once; a one-shot CallLater already in flight is not stopped, so
	// absolute deadlines subtract held time themselves (LL_TriggerMissionEndTimer).
	protected bool IsEvaluationAllowed()
	{
		LL_GameModeCoop gm = LL_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gm)
			return false;
		if (LL_GameModeCoop.IsHardFreezeActive())
			return false;

		return gm.GetState() == SCR_EGameModeState.GAME;
	}

	// Once per mission; the latch guards a second call before detection is torn down.
	protected void Fire()
	{
		if (m_bFired)
			return;
		m_bFired = true;

		if (m_bBroadcastMessage && m_sMessage != "")
			BroadcastFiredMessage();

		// Triggers that record their own attributed stats skip the generic timeline entry.
		if (!ReportsOwnStats())
		{
			LL_StatsManager stats = LL_StatsManager.GetInstance();
			if (stats)
			{
				string label = m_sMessage;
				if (label == "")
					label = m_sTitle;
				stats.NotifyTriggerFired(this, label);
			}
		}

		OnDisarm();
	}

	protected bool ReportsOwnStats()
	{
		return false;
	}

	// Squad names resolve to group RplIds at fire time, the ids the squad's slots
	// registered under. Fail closed: configured squads that all fail to resolve drop the
	// message rather than widen the audience.
	protected void BroadcastFiredMessage()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		bool groupsConfigured = false;
		array<int> groupIds = {};
		if (m_aMessageGroups)
		{
			foreach (string groupName : m_aMessageGroups)
			{
				if (groupName == "")
					continue;
				groupsConfigured = true;

				int groupId = ResolveGroupRplId(groupName);
				if (groupId != -1)
					groupIds.Insert(groupId);
				else
					Print(string.Format("[LL_Trigger] Message squad '%1' did not resolve to a replicated SCR_AIGroup — check the entity name", groupName), LogLevel.WARNING);
			}
		}

		if (groupsConfigured && groupIds.IsEmpty())
		{
			Print("[LL_Trigger] All message squads failed to resolve — message NOT sent: " + m_sMessage, LogLevel.WARNING);
			return;
		}

		mgr.BroadcastTargetedMessage_S(m_sMessage, m_sTitle, m_sMessageFaction, groupIds);
	}

	// Client-side audience test from replicated slot data. Unslotted viewers pass every
	// filter: the gate cuts noise, not intel.
	protected bool MessageTargetsLocalPlayer()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return false;

		LL_SlotData slot = mgr.FindSlotByPlayerId(pc.GetPlayerId());
		if (!slot)
			return true;

		if (m_sMessageFaction != "" && slot.m_sFactionKey != m_sMessageFaction)
			return false;

		if (m_aMessageGroups)
		{
			bool groupsConfigured = false;
			bool inTargetGroup = false;
			foreach (string groupName : m_aMessageGroups)
			{
				if (groupName == "")
					continue;
				groupsConfigured = true;
				if (ResolveGroupRplId(groupName) == slot.m_iGroupId)
				{
					inTargetGroup = true;
					break;
				}
			}
			if (groupsConfigured && !inTargetGroup)
				return false;
		}

		return true;
	}

	// Works on every machine: entity names are loadtime data and RplIds are identical
	// across server and clients.
	static int ResolveGroupRplId(string groupEntityName)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByName(groupEntityName));
		if (!group)
			return -1;

		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		if (!rpl)
			return -1;

		RplId rawId = rpl.Id();
		if (!rawId.IsValid())
			return -1;

		return rawId;
	}

	// Overridden by the triggers that can attribute work to a unit.
	float GetStatPoints()
	{
		return 0;
	}

	float GetStatMaxPerPlayer()
	{
		return 0;
	}

	string GetStatKey()
	{
		if (m_sStatKey != "")
			return m_sStatKey;

		m_sStatKey = GetOwner().GetName();
		if (m_sStatKey == "")
		{
			s_iNextStatKey++;
			m_sStatKey = string.Format("trigger-%1", s_iNextStatKey);
		}
		return m_sStatKey;
	}

	// Objective markup for the map's Objectives panel, rendered by LL_RichTextUI and
	// built client-side from loadtime config: <h>Header</h> plus body, with one
	// <link=EntityName>label</link> for the clickable object.

	string GetObjectiveMarkup()
	{
		return "";
	}

	// <h> is block-level in LL_RichTextUI. A #key title is translated here because it
	// lands inside composed markup.
	static string Header(string title)
	{
		return string.Format("<h>%1</h>", WidgetManager.Translate(title));
	}

	static string FormatDuration(int seconds)
	{
		if (seconds < 0)
			seconds = 0;
		int mins = seconds / 60;
		int secs = seconds - mins * 60;
		return string.Format("%1:%2", mins.ToString(), secs.ToString(2));
	}

	static string BuildObjectivesMarkup()
	{
		string text = "";
		foreach (LL_TriggerComponent trig : s_aBriefingTriggers)
		{
			if (!trig || !trig.m_bShowInBriefing)
				continue;
			string line = trig.GetObjectiveMarkup();
			if (line == "")
				continue;
			if (text != "")
				text += "<hr/>";
			text += line;
		}
		if (text == "")
			return WidgetManager.Translate("#LL-Trigger_NoConditions");
		return text;
	}

	static bool HasObjectives()
	{
		foreach (LL_TriggerComponent trig : s_aBriefingTriggers)
		{
			if (trig && trig.m_bShowInBriefing && trig.GetObjectiveMarkup() != "")
				return true;
		}
		return false;
	}

	// Clickable map-focus link for a named world entity; an empty label falls back to the
	// editable-entity name.
	static string ObjectLink(string entityName, string label = "")
	{
		if (entityName == "")
			return "";
		if (label == "")
			label = EntityDisplayName(entityName);
		return string.Format("<link=%1>%2</link>", entityName, label);
	}

	static string FactionName(FactionKey key)
	{
		if (key == "")
			return "";
		FactionManager fm = GetGame().GetFactionManager();
		if (fm)
		{
			Faction f = fm.GetFactionByKey(key);
			if (f)
				return f.GetFactionName();
		}
		return key;
	}

	// The designer-set editable-entity name, else the raw entity name.
	static string EntityDisplayName(string entityName)
	{
		if (entityName == "")
			return "";

		IEntity ent = GetGame().GetWorld().FindEntityByName(entityName);
		if (!ent)
			return entityName;

		SCR_EditableEntityComponent ec = SCR_EditableEntityComponent.Cast(ent.FindComponent(SCR_EditableEntityComponent));
		if (ec)
		{
			SCR_UIInfo info = ec.GetInfo();
			if (info)
			{
				string n = info.GetName();
				if (n != "")
					return n;
			}
		}
		return entityName;
	}

	static FactionKey GetEntityFactionKey(IEntity ent)
	{
		if (!ent)
			return "";
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return "";
		Faction f = fac.GetAffiliatedFaction();
		if (!f)
			return "";
		return f.GetFactionKey();
	}

	static bool IsCharacterAlive(IEntity ent)
	{
		if (!ent)
			return false;
		DamageManagerComponent dmg = DamageManagerComponent.Cast(ent.FindComponent(DamageManagerComponent));
		if (dmg && dmg.GetState() == EDamageState.DESTROYED)
			return false;
		return true;
	}

	// Alive playable characters, human- or bot-held: bots must count toward capture and
	// loss thresholds like human-held slots. Pure AI groups are not slots.
	static void CollectAlivePlayableCharacters(notnull array<IEntity> outChars)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			IEntity ent = mgr.ResolveSlotEntity(slot.m_iRplId);
			if (IsCharacterAlive(ent))
				outChars.Insert(ent);
		}
	}

	// Flat x,z polygon from a named ShapeEntity, or the owner when the name is empty.
	static void BuildPolygon(string shapeName, IEntity ownerFallback, notnull array<float> outPoly)
	{
		outPoly.Clear();

		ShapeEntity shape;
		if (shapeName != "")
			shape = ShapeEntity.Cast(GetGame().GetWorld().FindEntityByName(shapeName));
		else
			shape = ShapeEntity.Cast(ownerFallback);

		if (!shape)
			return;

		array<vector> pts = {};
		shape.GetPointsPositions(pts);
		if (pts.Count() < 3)
			return;

		// CoordToParent applies the full transform; origin + local misplaces a rotated shape.
		foreach (vector p : pts)
		{
			vector world = shape.CoordToParent(p);
			outPoly.Insert(world[0]);
			outPoly.Insert(world[2]);
		}
	}

	static bool PointInPolygon(vector pos, notnull array<float> poly)
	{
		float px = pos[0];
		float pz = pos[2];
		int n = poly.Count() / 2;
		bool inside = false;
		int j = n - 1;
		for (int i = 0; i < n; j = i++)
		{
			float xi = poly[i * 2];
			float zi = poly[i * 2 + 1];
			float xj = poly[j * 2];
			float zj = poly[j * 2 + 1];
			if ((zi > pz) != (zj > pz) && px < (xj - xi) * (pz - zi) / (zj - zi) + xi)
				inside = !inside;
		}
		return inside;
	}
}