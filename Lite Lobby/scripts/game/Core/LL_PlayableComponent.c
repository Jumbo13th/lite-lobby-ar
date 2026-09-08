// On every playable character. Zero replication: per-character traffic multiplies by
// the playable count. Registers the character with LL_LobbyManager on the server and
// bridges entity → slot data locally.

class LL_PlayableComponentClass : ScriptComponentClass
{
}

class LL_PlayableComponent : ScriptComponent
{
	// Injected into the character base prefab override, so every character carries it.
	[Attribute("1", UIWidgets.CheckBox, "Register this character as a playable lobby slot.")]
	protected bool m_bIsPlayable;

	protected int m_iRplId = -1;

	protected AIAgent m_AIAgent;

	// System-spawned template characters are never in an AI group; this keeps them out.
	protected bool m_bHasGroup;

	// Workbench offline mode has no valid RplIds.
	protected static int s_iFallbackIdCounter = 100000;

	protected int m_iRegisterRetries;

	protected bool m_bRegistered;

	// Possession only ever targets runtime bodies: since 1.7.0.54 a possessed loadtime
	// body no longer streams to clients. See LL_LobbyManager.PossessSlot_S.
	protected bool m_bRuntimeSpawned;

	void MarkRuntimeSpawned()	{ m_bRuntimeSpawned = true; }
	bool IsRuntimeSpawned()		{ return m_bRuntimeSpawned; }

	// Sort key inherited from the slot a respawned body replaces, so it keeps its
	// in-squad position. Unset = this body's own RplId.
	protected bool m_bInheritSortKey;
	protected int m_iInheritSortKey;
	void SetSortOrder(int sortKey)
	{
		m_iInheritSortKey = sortKey;
		m_bInheritSortKey = true;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		if (!m_bIsPlayable)
			return;

		if (Replication.IsServer())
		{
			// The RplId is not assigned yet at OnPostInit.
			GetGame().GetCallqueue().CallLater(RegisterWithManager, 500, false);
		}
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);

		GetGame().GetCallqueue().Remove(ForceDeactivateAI_S);

		// A flag rather than an id sign test: valid RplIds can be negative as ints.
		if (Replication.IsServer() && m_bRegistered)
		{
			LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
			if (mgr)
				mgr.UnregisterSlot_S(m_iRplId);
		}
	}

	protected void RegisterWithManager()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Preview/template characters can be spawned into another world context.
		if (owner.GetWorld() != GetGame().GetWorld())
			return;

		// Typed validity check: real RplIds can convert to negative ints, and a sign test
		// stamped fallback ids on whole servers.
		bool hasValidId = false;
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (rpl)
		{
			RplId rawId = rpl.Id();
			if (rawId.IsValid())
			{
				m_iRplId = rawId;
				hasValidId = true;
			}
		}

		if (!hasValidId)
		{
			// The id can still be unassigned this early; a fallback id would break spectator
			// labels and follow on every client. Retried even when Mode reads None, because
			// world-placed squads register before the session reports its mode.
			bool networked = RplSession.Mode() != RplMode.None;
			if (m_iRegisterRetries < 4 || (networked && m_iRegisterRetries < 20))
			{
				m_iRegisterRetries++;
				GetGame().GetCallqueue().CallLater(RegisterWithManager, 500, false);
				return;
			}

			// Offline play: local resolution goes through the manager's entity map.
			m_iRplId = s_iFallbackIdCounter;
			s_iFallbackIdCounter++;

			// On a networked session this means clients cannot resolve the character.
			if (networked)
			{
				Print(string.Format("[LL_Lobby] Slot got FALLBACK id %1 on a networked session (mode=%2, retries=%3) — clients cannot resolve this character: prefab=%4",
					m_iRplId, typename.EnumToString(RplMode, RplSession.Mode()), m_iRegisterRetries, GetPrefabName(owner)), LogLevel.WARNING);
			}
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
		{
			Print(string.Format("[LL_Lobby] PlayableComponent: manager not found, cannot register id=%1", m_iRplId), LogLevel.WARNING);
			return;
		}

		LL_SlotData slot = BuildSlotData();
		if (!slot)
			return;

		if (slot.m_sFactionKey == "")
		{
			Print(string.Format("[LL_Lobby] Skipping faction-less character: prefab=%1 pos=%2",
				GetPrefabName(owner), owner.GetOrigin().ToString()), LogLevel.NORMAL);
			return;
		}

		// Loose characters with no AI group are system template spawns.
		if (!m_bHasGroup)
		{
			Print(string.Format("[LL_Lobby] Skipping ungrouped character (template/preview spawn?): prefab=%1 pos=%2",
				GetPrefabName(owner), owner.GetOrigin().ToString()), LogLevel.NORMAL);
			return;
		}

		// The entity is kept server-side for possession.
		mgr.RegisterSlot_S(slot, owner);
		m_bRegistered = true;

		Print(string.Format("[LL_Lobby] PlayableComponent registered: id=%1 name=%2 faction=%3 group=%4",
			slot.m_iRplId, slot.m_sName, slot.m_sFactionKey, slot.m_sGroupName), LogLevel.NORMAL);

		// Fresh playables are alive; the slot turns KIA on the event.
		SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(
			owner.FindComponent(SCR_CharacterDamageManagerComponent));
		if (dmgMgr)
			dmgMgr.GetOnDamageStateChanged().Insert(OnDamageStateChanged_S);

		// Loadtime bodies stand inert as slot definitions; the engine re-activates their
		// AI on its own, so this repeats. They are never possessed directly.
		GetGame().GetCallqueue().CallLater(ForceDeactivateAI_S, 500, true);
	}

	// Server-only repeater: the engine toggles activation on its own.
	protected void ForceDeactivateAI_S()
	{
		if (!m_AIAgent)
		{
			GetGame().GetCallqueue().Remove(ForceDeactivateAI_S);
			return;
		}

		if (m_AIAgent.IsAIActivated())
			m_AIAgent.DeactivateAI();
	}

	protected void OnDamageStateChanged_S(EDamageState state)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.SetSlotDamageState_S(m_iRplId, state);
	}

	// Read once at registration; the manager owns the data afterwards.
	protected LL_SlotData BuildSlotData()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		LL_SlotData slot = new LL_SlotData();
		slot.m_iRplId = m_iRplId;

		// Source for the client-side 3D slot preview; the live entity is server-side only.
		EntityPrefabData prefabData = owner.GetPrefabData();
		if (prefabData)
			slot.m_sPrefabName = prefabData.GetPrefabName();

		// GetAffiliatedFactionKey reads the stored key; GetAffiliatedFaction queries the
		// faction manager, which may not have registered the entity yet.
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(
			owner.FindComponent(FactionAffiliationComponent));
		if (factionComp)
		{
			slot.m_sFactionKey = factionComp.GetAffiliatedFactionKey();

			// Loose characters have no affiliated faction at init, only the prefab default.
			if (slot.m_sFactionKey == "")
				slot.m_sFactionKey = factionComp.GetDefaultFactionKey();
		}

		SCR_EditableEntityComponent editableComp = SCR_EditableEntityComponent.Cast(
			owner.FindComponent(SCR_EditableEntityComponent));
		if (editableComp)
		{
			SCR_EditableEntityUIInfo uiInfo = SCR_EditableEntityUIInfo.Cast(editableComp.GetInfo());
			if (uiInfo)
			{
				slot.m_sName = uiInfo.GetName();

				// Captured as strings: the entity only exists server-side during the lobby.
				if (uiInfo.GetIconSetName() == "")
				{
					slot.m_sIconPath = uiInfo.GetIconPath();
				}
				else
				{
					slot.m_sIconPath = uiInfo.GetImageSetPath();
					slot.m_sIconName = uiInfo.GetIconSetName();
				}
			}
		}

		if (slot.m_sName == "")
			slot.m_sName = "Unknown";

		SCR_AIGroup group = null;
		AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
		if (aiControl)
		{
			AIAgent agent = aiControl.GetControlAIAgent();
			if (agent)
			{
				// ForceDeactivateAI_S needs the agent every tick.
				m_AIAgent = agent;
				group = SCR_AIGroup.Cast(agent.GetParentGroup());
			}
		}

		m_bHasGroup = group != null;

		if (group)
		{
			RplComponent groupRpl = RplComponent.Cast(group.FindComponent(RplComponent));
			if (groupRpl)
				slot.m_iGroupId = groupRpl.Id();

			// The callsign names are already translated strings, safe on a dedicated server.
			SCR_CallsignGroupComponent callsignComp = SCR_CallsignGroupComponent.Cast(
				group.FindComponent(SCR_CallsignGroupComponent));
			if (callsignComp)
			{
				// Squads are ordered by this name (number-aware); the numeric callsign key is
				// not always ready at this point.
				string company, platoon, squad, character, format;
				if (callsignComp.GetCallsignNames(company, platoon, squad, character, format))
				{
					// Company + platoon + squad, e.g. "Alpha Red 2".
					string groupName = "";
					if (company != "")
						groupName = company;
					if (platoon != "")
					{
						if (groupName != "")
							groupName += " ";
						groupName += platoon;
					}
					if (squad != "")
					{
						if (groupName != "")
							groupName += " ";
						groupName += squad;
					}
					slot.m_sGroupName = groupName;
				}
			}

			if (slot.m_sGroupName == "")
			{
				string entityName = group.GetName();
				if (entityName != "")
					slot.m_sGroupName = entityName;
			}
		}

		// Own RplId = editor placement order for loadtime bodies; a respawned body inherits
		// the replaced slot's key.
		if (m_bInheritSortKey)
			slot.m_iSortKey = m_iInheritSortKey;
		else
			slot.m_iSortKey = m_iRplId;

		// No initial damage read: GetState at init reported false KIA for whole squads.

		return slot;
	}

	protected static string GetPrefabName(IEntity entity)
	{
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return "(no prefab)";

		ResourceName prefabName = prefabData.GetPrefabName();
		if (prefabName == "")
			return "(unnamed)";

		return prefabName;
	}

	int GetRplId()
	{
		return m_iRplId;
	}

	LL_SlotData GetSlotData()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return null;

		return mgr.FindSlotByRplId(m_iRplId);
	}

	int GetAssignedPlayerId()
	{
		LL_SlotData slot = GetSlotData();
		if (!slot)
			return -1;

		return slot.m_iPlayerId;
	}

	bool IsAvailable()
	{
		LL_SlotData slot = GetSlotData();
		if (!slot)
			return false;

		return slot.IsAvailable();
	}
}