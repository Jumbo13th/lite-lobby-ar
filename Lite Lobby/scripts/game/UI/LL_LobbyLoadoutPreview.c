// Client-side character preview in the lobby: a local throwaway copy of the hovered
// slot's prefab rendered into an ItemPreviewWidget, with name, faction colour and a gear
// summary. Local because the playable characters only exist server-side during slotting.
// Debounced so sweeping the mouse down the list does not rebuild per row.

class LL_LobbyLoadoutPreview : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName PREVIEW_MANAGER_PREFAB = "{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et";

	protected static const int RENDER_DEBOUNCE_MS = 90;

	protected ItemPreviewManagerEntity m_PreviewManager;
	protected ItemPreviewWidget m_wPreview;
	protected TextWidget m_wName;
	protected ImageWidget m_wNameBackground;
	protected Widget m_wBorder;
	protected RichTextWidget m_wGear;

	protected ref SCR_InventoryCharacterWidgetHelper m_RotateHelper;
	protected PreviewRenderAttributes m_RenderAttributes;

	protected IEntity m_PreviewEntity;

	// The id is a slot or a vehicle RplId depending on the matching *Vehicle flag.
	protected int m_iCurrentSlotRplId;
	protected bool m_bShowing;
	protected bool m_bShowingVehicle;
	protected int m_iPendingSlotRplId;
	protected bool m_bPending;
	protected bool m_bPendingVehicle;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		m_PreviewManager = world.GetItemPreviewManager();
		if (!m_PreviewManager)
		{
			Resource res = Resource.Load(PREVIEW_MANAGER_PREFAB);
			if (res.IsValid())
				GetGame().SpawnEntityPrefabLocal(res, world);

			m_PreviewManager = world.GetItemPreviewManager();
		}

		m_wPreview = ItemPreviewWidget.Cast(w.FindAnyWidget("playerRender"));
		m_wName = TextWidget.Cast(w.FindAnyWidget("PreviewName"));
		m_wNameBackground = ImageWidget.Cast(w.FindAnyWidget("NameBackground"));
		m_wBorder = w.FindAnyWidget("PreviewBorder");
		m_wGear = RichTextWidget.Cast(w.FindAnyWidget("PreviewGear"));

		if (m_wPreview)
			m_RotateHelper = new SCR_InventoryCharacterWidgetHelper(m_wPreview, GetGame().GetWorkspace());

		Clear();
	}

	override void HandlerDeattached(Widget w)
	{
		GetGame().GetCallqueue().Remove(ApplyPendingPreview);

		// The helper registered itself on the workspace.
		if (m_RotateHelper)
		{
			m_RotateHelper.Destroy();
			m_RotateHelper = null;
		}

		super.HandlerDeattached(w);
	}

	void SetPreviewSlot(int slotRplId)
	{
		RequestPreview(slotRplId, false);
	}

	void SetPreviewVehicle(int vehicleRplId)
	{
		RequestPreview(vehicleRplId, true);
	}

	protected void RequestPreview(int id, bool isVehicle)
	{
		if (m_bShowing && !m_bPending && id == m_iCurrentSlotRplId && isVehicle == m_bShowingVehicle)
			return;

		if (m_bPending && id == m_iPendingSlotRplId && isVehicle == m_bPendingVehicle)
			return;

		m_iPendingSlotRplId = id;
		m_bPendingVehicle = isVehicle;
		m_bPending = true;

		GetGame().GetCallqueue().Remove(ApplyPendingPreview);
		GetGame().GetCallqueue().CallLater(ApplyPendingPreview, RENDER_DEBOUNCE_MS, false);
	}

	void ClearPreview()
	{
		GetGame().GetCallqueue().Remove(ApplyPendingPreview);
		m_bPending = false;
		Clear();
	}

	// Only does work while a model is shown and the user is rotating it.
	void Update(float tDelta)
	{
		if (!m_bShowing || !m_PreviewEntity || !m_PreviewManager || !m_wPreview)
			return;

		if (m_RotateHelper && m_RenderAttributes && m_RotateHelper.Update(tDelta, m_RenderAttributes))
			m_PreviewManager.SetPreviewItem(m_wPreview, m_PreviewEntity, m_RenderAttributes, true);
	}

	protected void ApplyPendingPreview()
	{
		m_bPending = false;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		if (m_bPendingVehicle)
		{
			LL_VehicleData vehicle = mgr.FindVehicleByRplId(m_iPendingSlotRplId);
			if (!vehicle || vehicle.m_sPrefabName == "")
				return;

			ShowVehicle(vehicle);
			return;
		}

		LL_SlotData slot = mgr.FindSlotByRplId(m_iPendingSlotRplId);
		if (!slot || slot.m_sPrefabName == "")
		{
			// The prefab's follow-up RPC lands just after the slot; keep the current preview.
			return;
		}

		ShowSlot(slot);
	}

	protected void ShowSlot(LL_SlotData slot)
	{
		if (!m_PreviewManager || !m_wPreview)
			return;

		m_PreviewEntity = m_PreviewManager.ResolvePreviewEntityForPrefab(slot.m_sPrefabName);
		if (!m_PreviewEntity)
		{
			Clear();
			return;
		}

		m_iCurrentSlotRplId = slot.m_iRplId;
		m_bShowing = true;
		m_bShowingVehicle = false;

		// Optional: without them the model still renders, just without drag-rotate.
		m_RenderAttributes = null;
		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(
			m_PreviewEntity.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (storage)
		{
			auto collection = storage.GetAttributes();
			if (collection)
				m_RenderAttributes = PreviewRenderAttributes.Cast(collection.FindAttribute(SCR_CharacterInventoryPreviewAttributes));
		}

		if (m_RenderAttributes)
			m_RenderAttributes.ResetDeltaRotation();

		m_PreviewManager.SetPreviewItem(m_wPreview, m_PreviewEntity, m_RenderAttributes, true);
		m_wPreview.SetVisible(true);

		ApplyFactionColour(slot.m_sFactionKey);

		if (m_wName)
		{
			m_wName.SetText(slot.m_sName);
			m_wName.SetVisible(true);
		}

		BuildGearText(m_PreviewEntity);
	}

	// No gear list and no drag-rotate: vehicles have no inventory preview attributes.
	protected void ShowVehicle(LL_VehicleData vehicle)
	{
		if (!m_PreviewManager || !m_wPreview)
			return;

		m_PreviewEntity = m_PreviewManager.ResolvePreviewEntityForPrefab(vehicle.m_sPrefabName);
		if (!m_PreviewEntity)
		{
			Clear();
			return;
		}

		m_iCurrentSlotRplId = vehicle.m_iRplId;
		m_bShowing = true;
		m_bShowingVehicle = true;
		m_RenderAttributes = null;

		m_PreviewManager.SetPreviewItem(m_wPreview, m_PreviewEntity, null, true);
		m_wPreview.SetVisible(true);

		ApplyFactionColour(vehicle.m_sFactionKey);

		if (m_wName)
		{
			m_wName.SetText(vehicle.m_sName);
			m_wName.SetVisible(true);
		}

		if (m_wGear)
			m_wGear.SetText("");
	}

	protected void ApplyFactionColour(string factionKey)
	{
		// The single place that re-reveals the framing Clear hides.
		if (m_wNameBackground)
			m_wNameBackground.SetVisible(true);
		if (m_wBorder)
			m_wBorder.SetVisible(true);

		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm)
			return;

		SCR_Faction faction = SCR_Faction.Cast(fm.GetFactionByKey(factionKey));
		if (!faction)
			return;

		if (m_wNameBackground)
			m_wNameBackground.SetColor(faction.GetFactionColor());
	}

	protected void Clear()
	{
		m_bShowing = false;
		m_bShowingVehicle = false;
		m_iCurrentSlotRplId = -1;
		m_PreviewEntity = null;
		m_RenderAttributes = null;

		// SetPreviewItemFromPrefab("") does not blank a model set via SetPreviewItem(entity).
		if (m_PreviewManager && m_wPreview)
			m_PreviewManager.SetPreviewItem(m_wPreview, null);
		if (m_wPreview)
			m_wPreview.SetVisible(false);

		if (m_wNameBackground)
			m_wNameBackground.SetVisible(false);
		if (m_wBorder)
			m_wBorder.SetVisible(false);
		if (m_wName)
			m_wName.SetVisible(false);
		if (m_wGear)
			m_wGear.SetText("");
	}

	protected void BuildGearText(IEntity character)
	{
		if (!m_wGear)
			return;

		string text = "";

		CharacterWeaponManagerComponent weaponsMgr = CharacterWeaponManagerComponent.Cast(
			character.FindComponent(CharacterWeaponManagerComponent));
		if (weaponsMgr)
		{
			// Vanilla weapon storage order: grenade(0), primary(2), secondary(3), sidearm(4).
			array<WeaponSlotComponent> weaponSlots = {};
			weaponsMgr.GetWeaponsSlots(weaponSlots);
			// Translated up front: the gear text is a concatenated plain string.
			text += GearLine(WidgetManager.Translate("#LL-Gear_Primary"), WeaponName(WeaponInSlot(weaponSlots, 2)));
			text += GearLine(WidgetManager.Translate("#LL-Gear_Secondary"), WeaponName(WeaponInSlot(weaponSlots, 3)));
			text += GearLine(WidgetManager.Translate("#LL-Gear_Sidearm"), WeaponName(WeaponInSlot(weaponSlots, 4)));
			text += GearLine(WidgetManager.Translate("#LL-Gear_Grenade"), WeaponName(WeaponInSlot(weaponSlots, 0)));
		}

		SCR_InventoryStorageManagerComponent invMgr = SCR_InventoryStorageManagerComponent.Cast(
			character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (invMgr)
		{
			array<string> gear = {};
			if (invMgr.FindItemWithComponents({SCR_MapGadgetComponent}, EStoragePurpose.PURPOSE_ANY))
				gear.Insert(WidgetManager.Translate("#LL-Gear_Map"));
			if (invMgr.FindItemWithComponents({SCR_BinocularsComponent}, EStoragePurpose.PURPOSE_ANY))
				gear.Insert(WidgetManager.Translate("#LL-Gear_Binoculars"));
			if (invMgr.FindItemWithComponents({SCR_CompassComponent}, EStoragePurpose.PURPOSE_ANY))
				gear.Insert(WidgetManager.Translate("#LL-Gear_Compass"));
			if (invMgr.FindItemWithComponents({SCR_FlashlightComponent}, EStoragePurpose.PURPOSE_ANY))
				gear.Insert(WidgetManager.Translate("#LL-Gear_Flashlight"));
			if (invMgr.FindItemWithComponents({SCR_RadioComponent}, EStoragePurpose.PURPOSE_ANY))
				gear.Insert(WidgetManager.Translate("#LL-Gear_Radio"));
			if (invMgr.FindItemWithComponents({SCR_WristwatchComponent}, EStoragePurpose.PURPOSE_ANY))
				gear.Insert(WidgetManager.Translate("#LL-Gear_Watch"));

			if (gear.Count() > 0)
			{
				if (text != "")
					text += "\n";

				text += JoinComma(gear);
			}
		}

		m_wGear.SetText(text);
	}

	// "Label: Value\n"; a value equal to the label drops the prefix.
	protected string GearLine(string label, string value)
	{
		if (value == "")
			return "";

		if (value == label)
			return string.Format("%1\n", value);

		return string.Format("%1: %2\n", label, value);
	}

	protected string JoinComma(notnull array<string> parts)
	{
		string result = "";
		foreach (string part : parts)
		{
			if (result != "")
				result += ", ";

			result += part;
		}

		return result;
	}

	protected string WeaponName(IEntity weapon)
	{
		if (!weapon)
			return "";

		BaseWeaponComponent weaponComp = BaseWeaponComponent.Cast(weapon.FindComponent(BaseWeaponComponent));
		if (!weaponComp)
			return "";

		UIInfo info = weaponComp.GetUIInfo();
		if (!info)
			return "";

		return WidgetManager.Translate(info.GetName());
	}

	protected IEntity WeaponInSlot(notnull array<WeaponSlotComponent> slots, int index)
	{
		if (index < 0 || index >= slots.Count())
			return null;

		WeaponSlotComponent slot = slots[index];
		if (!slot)
			return null;

		return slot.GetWeaponEntity();
	}
}