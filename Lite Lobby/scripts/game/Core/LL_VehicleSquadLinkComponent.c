// Links a vehicle to a squad so it shows under that squad in the lobby: set the editor
// name of the squad's SCR_AIGroup. Zero per-entity replication: the vehicle only
// registers with LL_LobbyManager on the server, and the group's RplComponent id is the
// same id the squad's slots register under.

class LL_VehicleSquadLinkComponentClass : ScriptComponentClass
{
}

class LL_VehicleSquadLinkComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Editor name of the squad's group entity (SCR_AIGroup) this vehicle belongs to. Empty = not linked.")]
	protected string m_sGroupName;

	protected int m_iRplId = -1;

	protected int m_iRegisterRetries;

	protected bool m_bRegistered;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		if (m_sGroupName == "")
			return;

		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(RegisterWithManager, 500, false);
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);

		if (Replication.IsServer() && m_bRegistered)
		{
			LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
			if (mgr)
				mgr.UnregisterVehicle_S(m_iRplId);
		}
	}

	protected void RegisterWithManager()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Preview/template spawns can live in another world.
		if (owner.GetWorld() != GetGame().GetWorld())
			return;

		// Typed validity, not a sign test: valid RplIds convert to negative ints.
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

		// The same replicated id the squad's character slots use.
		int groupId = -1;
		bool hasGroup = false;
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByName(m_sGroupName));
		if (group)
		{
			RplComponent groupRpl = RplComponent.Cast(group.FindComponent(RplComponent));
			if (groupRpl)
			{
				RplId groupRawId = groupRpl.Id();
				if (groupRawId.IsValid())
				{
					groupId = groupRawId;
					hasGroup = true;
				}
			}
		}

		// The vehicle id and the world-placed group both settle shortly after boot.
		if (!hasValidId || !hasGroup)
		{
			if (m_iRegisterRetries < 20)
			{
				m_iRegisterRetries++;
				GetGame().GetCallqueue().CallLater(RegisterWithManager, 500, false);
				return;
			}

			Print(string.Format("[LL_Lobby] Vehicle squad link gave up: group='%1' validId=%2 groupResolved=%3",
				m_sGroupName, hasValidId, group != null), LogLevel.WARNING);
			return;
		}

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		LL_VehicleData vehicle = new LL_VehicleData();
		vehicle.m_iRplId = m_iRplId;
		vehicle.m_iGroupId = groupId;

		// SCR_ResourceNameUtils rather than GetPrefabData().GetPrefabName(): the direct
		// getter returns empty for inherited prefabs, which vanilla vehicles are.
		vehicle.m_sPrefabName = SCR_ResourceNameUtils.GetPrefabName(owner);

		SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(
			owner.FindComponent(SCR_EditableVehicleComponent));
		if (editable)
		{
			SCR_UIInfo info = editable.GetInfo();
			if (info)
			{
				vehicle.m_sName = info.GetName();
				vehicle.m_sIconPath = info.GetIconPath();
			}
		}

		if (vehicle.m_sName == "")
			vehicle.m_sName = "Vehicle";

		// Affiliated key first, prefab default for loose vehicles.
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(
			owner.FindComponent(FactionAffiliationComponent));
		if (factionComp)
		{
			vehicle.m_sFactionKey = factionComp.GetAffiliatedFactionKey();
			if (vehicle.m_sFactionKey == "")
				vehicle.m_sFactionKey = factionComp.GetDefaultFactionKey();
		}

		mgr.RegisterVehicle_S(vehicle);
		m_bRegistered = true;

		Print(string.Format("[LL_Lobby] Vehicle linked to squad '%1': id=%2 name=%3 group=%4 prefab='%5'",
			m_sGroupName, vehicle.m_iRplId, vehicle.m_sName, groupId, vehicle.m_sPrefabName), LogLevel.NORMAL);
	}
}