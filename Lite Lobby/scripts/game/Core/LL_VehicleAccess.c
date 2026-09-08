// Crew/pilot qualification lock for vehicle seats, per character rather than per group,
// so an infantry squad rides in the back of its APC while only its crew slots drive and
// gun. Two prefab lists on the game-mode prefab: crew roles (what a character may
// operate) and restricted vehicles (what their driver and turret seats demand); cargo is
// never restricted. A base prefab covers every variant that inherits from it, nearest
// match first. Seats are locked by compartment type: PILOT, TURRET (commander, gunner
// and observer alike), CARGO. The lock lives in the client-evaluated user actions, so it
// does not police a modified client, and a Game Master may place anyone anywhere. Zero
// replication: prefabs and attributes load identically on every machine.

//! Bit flags: a character may hold several, a seat demands one of them.
enum LL_EVehicleQualification
{
	NONE = 0,
	CREW = 1,
	PILOT = 2,
}

[BaseContainerProps()]
class LL_CrewRoleEntry
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Character prefab that counts as qualified crew. Covers every prefab inherited from it.", params: "et")]
	protected ResourceName m_sPrefab;

	// Flags, not a dropdown: a character can be both crew and aircrew.
	[Attribute("1", UIWidgets.Flags, "What this character is qualified to operate. Tick both for a character allowed to do either.", "", ParamEnumArray.FromEnum(LL_EVehicleQualification))]
	protected LL_EVehicleQualification m_eGrants;

	ResourceName GetPrefab()				{ return m_sPrefab; }
	LL_EVehicleQualification GetGrants()	{ return m_eGrants; }
}

[BaseContainerProps()]
class LL_RestrictedVehicleEntry
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Vehicle prefab to restrict. Covers every prefab inherited from it.", params: "et")]
	protected ResourceName m_sPrefab;

	// Any one ticked flag admits the character; nothing ticked = open seat.
	[Attribute("1", UIWidgets.Flags, "Qualification needed for the DRIVER / PILOT seat. Nothing ticked = anyone may drive.", "", ParamEnumArray.FromEnum(LL_EVehicleQualification))]
	protected LL_EVehicleQualification m_ePilotRequires;

	[Attribute("1", UIWidgets.Flags, "Qualification needed for TURRET seats — commander, gunner and observer positions are all turrets, so this covers them together. Nothing ticked = anyone may gun.", "", ParamEnumArray.FromEnum(LL_EVehicleQualification))]
	protected LL_EVehicleQualification m_eTurretRequires;

	ResourceName GetPrefab()						{ return m_sPrefab; }
	LL_EVehicleQualification GetPilotRequires()		{ return m_ePilotRequires; }
	LL_EVehicleQualification GetTurretRequires()	{ return m_eTurretRequires; }
}

class LL_VehicleAccessClass : SCR_BaseGameModeComponentClass
{
}

class LL_VehicleAccess : SCR_BaseGameModeComponent
{
	// Not named "Enabled": every script component already shows an engine Enabled box.
	[Attribute("1", UIWidgets.CheckBox, "Lock driver and gunner seats to qualified characters. Off = vanilla behaviour, anyone may take any seat.", category: "Vehicle Access")]
	protected bool m_bRestrictSeats;

	[Attribute("", UIWidgets.Object, "Character prefabs that count as qualified crew or aircrew. Anyone not listed here is unqualified.", category: "Vehicle Access")]
	protected ref array<ref LL_CrewRoleEntry> m_aCrewRoles;

	[Attribute("", UIWidgets.Object, "Vehicle prefabs whose driver and gunner seats are locked. Vehicles not listed here stay open to everyone.", category: "Vehicle Access")]
	protected ref array<ref LL_RestrictedVehicleEntry> m_aRestrictedVehicles;

	// The arrays give the ancestry walk something to scan; the maps carry the payload.
	protected ref array<string> m_aRolePrefabs = {};
	protected ref map<string, int> m_mRoleGrants = new map<string, int>();

	protected ref array<string> m_aVehiclePrefabs = {};
	protected ref map<string, ref LL_RestrictedVehicleEntry> m_mVehicleEntries = new map<string, ref LL_RestrictedVehicleEntry>();

	// Evaluated every frame per visible seat prompt; one ancestry resolution per prefab
	// covers the session, misses included.
	protected ref map<string, int> m_mCharQualCache = new map<string, int>();
	protected ref map<string, string> m_mVehicleMatchCache = new map<string, string>();

	protected static LL_VehicleAccess s_Instance;

	static LL_VehicleAccess GetInstance()
	{
		return s_Instance;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
		BuildTables();
	}

	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	protected void BuildTables()
	{
		if (m_aCrewRoles)
		{
			foreach (LL_CrewRoleEntry role : m_aCrewRoles)
			{
				ResourceName prefab = role.GetPrefab();
				if (prefab.IsEmpty())
					continue;

				// The same prefab listed twice accumulates its grants.
				int existing;
				if (m_mRoleGrants.Find(prefab, existing))
				{
					m_mRoleGrants.Set(prefab, existing | role.GetGrants());
					continue;
				}

				m_aRolePrefabs.Insert(prefab);
				m_mRoleGrants.Set(prefab, role.GetGrants());
			}
		}

		if (m_aRestrictedVehicles)
		{
			foreach (LL_RestrictedVehicleEntry vehicle : m_aRestrictedVehicles)
			{
				ResourceName prefab = vehicle.GetPrefab();
				if (prefab.IsEmpty())
					continue;

				// First row wins: requirements cannot be merged without loosening or tightening the lock.
				if (m_mVehicleEntries.Contains(prefab))
				{
					Print(string.Format("[LL_Lobby] Vehicle access: duplicate Restricted Vehicles entry for %1 — keeping the first, ignoring the rest.",
						prefab), LogLevel.WARNING);
					continue;
				}

				m_aVehiclePrefabs.Insert(prefab);
				m_mVehicleEntries.Set(prefab, vehicle);
			}
		}

		if (!Replication.IsServer())
			return;

		// A vehicle with no matching crew role locks itself out of being driven.
		Print(string.Format("[LL_Lobby] Vehicle access: restrict=%1 crewRoles=%2 restrictedVehicles=%3",
			m_bRestrictSeats, m_aRolePrefabs.Count(), m_aVehiclePrefabs.Count()), LogLevel.NORMAL);
	}

	//! True for anything it cannot classify: the feature only adds restrictions that were
	//! explicitly configured.
	bool CanUseCompartment(IEntity user, BaseCompartmentSlot compartment, out LocalizedString reason)
	{
		reason = string.Empty;

		// Runs every frame per prompt and the component is on by default; without the
		// empty-list test a mission with no vehicles listed pays a prefab walk per prompt.
		if (!m_bRestrictSeats || m_aVehiclePrefabs.IsEmpty() || !user || !compartment)
			return true;

		int required = GetRequiredQualification(compartment);
		if (required == LL_EVehicleQualification.NONE)
			return true;

		if ((GetCharacterQualification(user) & required) != 0)
			return true;

		if ((required & LL_EVehicleQualification.CREW) != 0)
			reason = "#LL-Vehicle_CrewOnly";
		else
			reason = "#LL-Vehicle_PilotOnly";

		return false;
	}

	protected int GetRequiredQualification(notnull BaseCompartmentSlot compartment)
	{
		// Cargo is never restricted.
		ECompartmentType type = compartment.GetType();
		if (type != ECompartmentType.PILOT && type != ECompartmentType.TURRET)
			return LL_EVehicleQualification.NONE;

		// Turret compartments belong to a child entity.
		IEntity vehicle = SCR_EntityHelper.GetMainParent(compartment.GetOwner(), true);
		if (!vehicle)
			return LL_EVehicleQualification.NONE;

		string prefab = SCR_ResourceNameUtils.GetPrefabName(vehicle);
		if (prefab == "")
			return LL_EVehicleQualification.NONE;

		string matched;
		if (!m_mVehicleMatchCache.Find(prefab, matched))
		{
			matched = FindConfiguredPrefab(vehicle, m_aVehiclePrefabs);
			m_mVehicleMatchCache.Set(prefab, matched);
		}

		if (matched == "")
			return LL_EVehicleQualification.NONE;

		LL_RestrictedVehicleEntry entry;
		if (!m_mVehicleEntries.Find(matched, entry))
			return LL_EVehicleQualification.NONE;

		if (type == ECompartmentType.PILOT)
			return entry.GetPilotRequires();

		return entry.GetTurretRequires();
	}

	protected int GetCharacterQualification(notnull IEntity character)
	{
		string prefab = SCR_ResourceNameUtils.GetPrefabName(character);
		if (prefab == "")
			return LL_EVehicleQualification.NONE;

		int mask;
		if (m_mCharQualCache.Find(prefab, mask))
			return mask;

		mask = LL_EVehicleQualification.NONE;

		string matched = FindConfiguredPrefab(character, m_aRolePrefabs);
		if (matched != "")
			m_mRoleGrants.Find(matched, mask);

		m_mCharQualCache.Set(prefab, mask);
		return mask;
	}

	//! First resource name in the entity's prefab ancestry that is configured, nearest
	//! ancestor first; "" when none.
	protected string FindConfiguredPrefab(notnull IEntity entity, notnull array<string> configured)
	{
		if (configured.IsEmpty())
			return "";

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return "";

		BaseContainer container = prefabData.GetPrefab();
		while (container)
		{
			// A container without a path is an inline override; only named resources can be
			// picked in Workbench.
			ResourceName res = container.GetResourceName();
			if (res.Contains("/") && configured.Contains(res))
				return res;

			container = container.GetAncestor();
		}

		return "";
	}
}