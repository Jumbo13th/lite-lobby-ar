// A live map marker on any prefab. The marker follows the target's root parent, so a
// tracked radio in a vest reports the player and a crate in a boot reports the car.
// Sampled on the server because an item in a container, or anything past
// networkViewDistance, never streams to a given client. Reports during GAME only and
// draws only on the in-game map. Needs LL_TrackerManager on the game-mode prefab.

class LL_TrackerComponentClass : ScriptComponentClass
{
}

class LL_TrackerComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Track this entity. Off = no marker, no network cost at all.", category: "Tracker")]
	protected bool m_bEnabled;

	// Appearance mirrors LL_ManualMarker.
	[Attribute("{E23427CAC80DA8B7}UI/Textures/Icons/icons_mapMarkersUI.imageset", UIWidgets.ResourceNamePicker, "Marker icon imageset (or a direct .edds texture).", category: "Tracker")]
	protected ResourceName m_sImageSet;

	[Attribute("circle-2", UIWidgets.EditBox, "Quad (sprite) name inside the imageset.", category: "Tracker")]
	protected string m_sQuadName;

	[Attribute("1 1 1 1", UIWidgets.ColorPicker, "Icon tint (used when 'Use target faction color' is off).", category: "Tracker")]
	protected ref Color m_MarkerColor;

	[Attribute("0", UIWidgets.CheckBox, "Tint the icon with the TRACKED entity's own faction color instead of the color above.", category: "Tracker")]
	protected bool m_bUseFactionColor;

	[Attribute("5", UIWidgets.EditBox, "Marker size. Meters (scales with map zoom) when world scale is on, pixels otherwise.", category: "Tracker")]
	protected float m_fWorldSize;

	[Attribute("1", UIWidgets.CheckBox, "Size in meters that scales with map zoom (on) vs a fixed pixel size (off).", category: "Tracker")]
	protected bool m_bUseWorldScale;

	[Attribute("24", UIWidgets.EditBox, "Minimum on-screen pixel size, so a world-scaled marker never shrinks to nothing when zoomed out.", category: "Tracker")]
	protected float m_fMinSize;

	[Attribute("0", UIWidgets.EditBox, "Draw order; higher draws on top.", category: "Tracker")]
	protected int m_iZOrder;

	[Attribute("0", UIWidgets.CheckBox, "Rotate the icon to the target's heading (for directional icons such as arrows).", category: "Tracker")]
	protected bool m_bShowHeading;

	[Attribute("", UIWidgets.EditBox, "Hover label. Plain text or a localization key (#LL-...) — keys are translated on each player's client.", category: "Tracker")]
	protected string m_sLabel;

	[Attribute("1", UIWidgets.CheckBox, "When the label above is empty, fall back to the prefab's display name.", category: "Tracker")]
	protected bool m_bLabelFromEntity;

	[Attribute("1", UIWidgets.CheckBox, "Visible to everyone — all factions, and players holding no slot. Disable to restrict to the faction list below.", category: "Tracker")]
	protected bool m_bShowForAnyFaction;

	[Attribute("", UIWidgets.Auto, "Faction keys that may see this tracker (used when 'show for all' is off). A player with no slot has no faction to match, so they never see a restricted tracker.", category: "Tracker")]
	protected ref array<FactionKey> m_aVisibleForFactions;

	[Attribute("2", UIWidgets.EditBox, "Seconds between marker position updates — the intel refresh rate. Higher = staler marker and less traffic. Rounded to the manager's 0.25s tick; values below that are clamped.", category: "Tracker")]
	protected float m_fUpdateInterval;

	[Attribute("0", UIWidgets.CheckBox, "Forbid using the map while this object is held. Raising the map stows whatever is in the player's hands, which would pocket the object; this refuses the map key instead, so the player has to put the object down first.", category: "Tracker")]
	protected bool m_bBlockMapWhileHeld;

	[Attribute("0", UIWidgets.CheckBox, "Emit a periodic beep from the tracked object. Diegetic: ANY player within earshot hears it, enemies included, so a hidden tracker can be hunted by ear. Independent of who may see the marker.", category: "Tracker Beacon")]
	protected bool m_bBeaconSound;

	[Attribute("3", UIWidgets.EditBox, "Seconds between beeps. Rounded to the manager's 0.25s tick.", category: "Tracker Beacon")]
	protected float m_fBeaconInterval;

	[Attribute("40", UIWidgets.EditBox, "Earshot radius in meters. Players farther from the object than this do not hear the beep.", category: "Tracker Beacon")]
	protected float m_fBeaconRange;

	[Attribute("1", UIWidgets.CheckBox, "Stop tracking once the target is destroyed (a dead character, a wrecked vehicle, a broken item).", category: "Tracker")]
	protected bool m_bStopWhenDestroyed;

	[Attribute("1", UIWidgets.CheckBox, "When tracking stops (target destroyed or deleted), leave the marker at its last known position, dimmed. Off = the marker disappears.", category: "Tracker")]
	protected bool m_bKeepLastPosition;

	// Server-side registry the manager publishes from at GAME. World-guarded because
	// statics survive a scenario restart.
	protected static ref array<LL_TrackerComponent> s_aTrackers = {};
	protected static BaseWorld s_RegistryWorld;

	static array<LL_TrackerComponent> GetAll()
	{
		return s_aTrackers;
	}

	//! Read client-side by the modded gadget manager; the held entity is always streamed.
	static bool BlocksMapWhileHeld(IEntity held)
	{
		if (!held)
			return false;

		LL_TrackerComponent tracker = LL_TrackerComponent.Cast(held.FindComponent(LL_TrackerComponent));
		return tracker && tracker.m_bBlockMapWhileHeld;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!m_MarkerColor)
			m_MarkerColor = new Color(1, 1, 1, 1);

		// Proxies never read this component: the config travels with the tracker data.
		if (!Replication.IsServer())
			return;

		if (!m_bEnabled)
			return;

		BaseWorld world = owner.GetWorld();
		if (world != s_RegistryWorld)
		{
			s_aTrackers.Clear();
			s_RegistryWorld = world;
		}

		if (!s_aTrackers.Contains(this))
			s_aTrackers.Insert(this);

		// A prefab spawned after the mission started missed the bulk publish.
		LL_TrackerManager mgr = LL_TrackerManager.GetInstance();
		if (mgr)
			mgr.OnTrackerAdded(this);
	}

	override void OnDelete(IEntity owner)
	{
		if (s_aTrackers)
			s_aTrackers.RemoveItem(this);

		// The manager holds a raw handle and must drop or freeze the marker first.
		LL_TrackerManager mgr = LL_TrackerManager.GetInstance();
		if (mgr)
			mgr.OnTrackerRemoved(this);

		super.OnDelete(owner);
	}

	//! False when the target is gone or destroyed.
	bool SampleTarget(out vector pos, out float yaw)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (m_bStopWhenDestroyed)
		{
			DamageManagerComponent dmg = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
			if (dmg && dmg.GetState() == EDamageState.DESTROYED)
				return false;
		}

		// An entity in an inventory or attached to a vehicle keeps a local transform.
		IEntity root = SCR_EntityHelper.GetMainParent(owner, true);
		if (!root)
			return false;

		pos = root.GetOrigin();
		yaw = root.GetYawPitchRoll()[0];
		return true;
	}

	//! Tint source when 'use faction color' is on; "" when the target has no faction.
	FactionKey GetTargetFactionKey()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return "";

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return "";

		// A placed entity usually has no affiliated faction; the default key is the faction.
		FactionKey key = fac.GetAffiliatedFactionKey();
		if (key == "")
			key = fac.GetDefaultFactionKey();
		return key;
	}

	float GetUpdateInterval()
	{
		return m_fUpdateInterval;
	}

	float GetBeaconInterval()
	{
		return m_fBeaconInterval;
	}

	//! Everything the client needs to draw this marker.
	void ExportConfig(notnull LL_TrackerData data)
	{
		data.m_sImageSet = m_sImageSet;
		data.m_sQuad = m_sQuadName;
		data.m_fColorR = m_MarkerColor.R();
		data.m_fColorG = m_MarkerColor.G();
		data.m_fColorB = m_MarkerColor.B();
		data.m_fColorA = m_MarkerColor.A();
		data.m_bUseFactionColor = m_bUseFactionColor;
		data.m_fWorldSize = m_fWorldSize;
		data.m_bUseWorldScale = m_bUseWorldScale;
		data.m_fMinSize = m_fMinSize;
		data.m_iZOrder = m_iZOrder;
		data.m_bShowHeading = m_bShowHeading;
		data.m_bAnyFaction = m_bShowForAnyFaction;
		data.m_bKeepLastPosition = m_bKeepLastPosition;
		data.m_bBeacon = m_bBeaconSound;
		data.m_fBeaconRange = m_fBeaconRange;
		data.m_sLabel = ResolveLabel();
		data.m_sVisibleFactions = JoinFactions();
		data.m_sFactionKey = GetTargetFactionKey();
	}

	// The editable display name is a localization key the client widget resolves.
	protected string ResolveLabel()
	{
		if (m_sLabel != "")
			return m_sLabel;

		if (!m_bLabelFromEntity)
			return "";

		IEntity owner = GetOwner();
		if (!owner)
			return "";

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.GetEditableEntity(owner);
		if (editable)
			return editable.GetDisplayName();

		return "";
	}

	// Comma-joined so the wire format stays a flat array.
	protected string JoinFactions()
	{
		if (!m_aVisibleForFactions)
			return "";

		string joined = "";
		foreach (FactionKey key : m_aVisibleForFactions)
		{
			if (key == "")
				continue;
			if (joined != "")
				joined += ",";
			joined += key;
		}
		return joined;
	}
}