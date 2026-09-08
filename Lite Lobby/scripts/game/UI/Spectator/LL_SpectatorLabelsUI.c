// Character marker on the spectator screen. One widget per slot serves both the 3D view
// and the spectator map; only the projection differs. Close: faction circle, ring and
// role icon; far: a small dot; name when close or hovered; dead: desaturated and
// translucent; unconscious: yellow ring.

class LL_SpectatorLabel : SCR_ScriptedWidgetComponent
{
	protected static const float NAME_REVEAL_DISTANCE = 30.0;
	protected static const float ICON_MAX_DISTANCE = 120.0;
	protected static const float ICON_SIZE = 32.0;
	// World-space lift above the head bone so the icon floats clear of helmets.
	protected static const float HEAD_OFFSET = 0.5;

	// One size on the map: a marker that shrank with zoom would be unreadable at overview zoom.
	protected static const float MAP_ICON_SIZE = 28.0;

	protected ref Color m_GreyColor = new Color(0.4, 0.4, 0.4, 1.0);

	protected IEntity m_TargetEntity;
	protected TNodeId m_TargetBone;
	protected SCR_CharacterControllerComponent m_CharController;
	protected int m_iSlotRplId = -1;
	protected bool m_bIsHovered;

	// Mark/sweep flag for LL_SpectatorMenu.UpdateLabels.
	protected bool m_bTouched;

	protected ref Color m_FactionColor = Color.White;
	protected ref Color m_OutlineColor = Color.White;

	protected Widget m_wOverlayCircle;
	protected ImageWidget m_wIconBackground;
	protected ImageWidget m_wIconCircle;
	protected ImageWidget m_wCircleSmall;
	protected ImageWidget m_wUnitIcon;
	protected RichTextWidget m_wText;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wOverlayCircle = w.FindAnyWidget("OverlayCircle");
		m_wIconBackground = ImageWidget.Cast(w.FindAnyWidget("IconBackground"));
		m_wIconCircle = ImageWidget.Cast(w.FindAnyWidget("IconCircle"));
		m_wCircleSmall = ImageWidget.Cast(w.FindAnyWidget("CircleSmall"));
		m_wUnitIcon = ImageWidget.Cast(w.FindAnyWidget("UnitIcon"));
		m_wText = RichTextWidget.Cast(w.FindAnyWidget("Text"));
	}

	void SetTarget(notnull IEntity entity, notnull LL_SlotData slot)
	{
		m_TargetEntity = entity;
		m_iSlotRplId = slot.m_iRplId;

		if (entity.GetAnimation())
			m_TargetBone = entity.GetAnimation().GetBoneIndex("Head");

		m_CharController = SCR_CharacterControllerComponent.Cast(
			entity.FindComponent(SCR_CharacterControllerComponent));

		SCR_Faction faction = SCR_Faction.Cast(
			GetGame().GetFactionManager().GetFactionByKey(slot.m_sFactionKey));
		if (faction)
		{
			m_FactionColor = faction.GetFactionColor();
			m_OutlineColor = faction.GetOutlineFactionColor();
		}

		if (m_wIconBackground)
			m_wIconBackground.SetColor(m_FactionColor);
		if (m_wIconCircle)
			m_wIconCircle.SetColor(m_OutlineColor);
		if (m_wCircleSmall)
			m_wCircleSmall.SetColor(m_FactionColor);

		if (m_wUnitIcon)
		{
			if (slot.m_sIconPath != "")
			{
				if (slot.m_sIconName != "")
					m_wUnitIcon.LoadImageFromSet(0, slot.m_sIconPath, slot.m_sIconName);
				else
					m_wUnitIcon.LoadImageTexture(0, slot.m_sIconPath);
			}
			m_wUnitIcon.SetColor(m_OutlineColor);
		}
	}

	int GetSlotRplId()
	{
		return m_iSlotRplId;
	}

	void SetTouched(bool state)
	{
		m_bTouched = state;
	}

	bool IsTouched()
	{
		return m_bTouched;
	}

	// Per frame; the slot is passed in to avoid a linear search per label. False when
	// the target is gone.
	bool UpdateLabel(LL_SlotData slot)
	{
		if (!m_TargetEntity || !slot)
			return false;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return false;

		vector labelWorldPos = GetLabelWorldPosition();
		vector screenPos = GetGame().GetWorkspace().ProjWorldToScreen(labelWorldPos, GetGame().GetWorld());

		// Behind the camera: hide, keep the widget.
		if (screenPos[2] < 0)
		{
			m_wRoot.SetVisible(false);
			return true;
		}

		CameraBase camera = GetGame().GetCameraManager().CurrentCamera();
		float dist = 0;
		if (camera)
			dist = vector.Distance(camera.GetOrigin(), labelWorldPos);

		m_wRoot.SetVisible(true);

		m_wRoot.SetZOrder(10000 - Math.Floor(dist * 100));

		FrameSlot.SetPos(m_wRoot, screenPos[0] - ICON_SIZE * 0.5, screenPos[1] - ICON_SIZE * 0.5);

		bool closeUp = dist <= ICON_MAX_DISTANCE;
		if (m_wOverlayCircle)
			m_wOverlayCircle.SetVisible(closeUp);
		if (m_wUnitIcon)
			m_wUnitIcon.SetVisible(closeUp);
		if (m_wCircleSmall)
			m_wCircleSmall.SetVisible(!closeUp);

		if (m_wText)
		{
			if (dist <= NAME_REVEAL_DISTANCE || m_bIsHovered)
				m_wText.SetText(LL_LobbyManager.FormatPlayerNameRich(ResolveName(slot, mgr)));
			else
				m_wText.SetText("");
		}

		ApplyState(slot);
		return true;
	}

	// Same widget projected through the map. Names are always shown: an unlabelled dot
	// on an overview carries no information, and the panel's filters bound the count.
	bool UpdateOnMap(LL_SlotData slot, SCR_MapEntity mapEntity)
	{
		if (!m_TargetEntity || !slot || !mapEntity)
			return false;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return false;

		vector worldPos = GetLabelWorldPosition();

		// The map works in whole screen pixels; DPI unscale puts it in workspace units.
		int screenX, screenY;
		mapEntity.WorldToScreen(worldPos[0], worldPos[2], screenX, screenY, true);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float x = workspace.DPIUnscale(screenX);
		float y = workspace.DPIUnscale(screenY);

		m_wRoot.SetVisible(true);
		m_wRoot.SetZOrder(0);
		FrameSlot.SetPos(m_wRoot, x - MAP_ICON_SIZE * 0.5, y - MAP_ICON_SIZE * 0.5);

		// The far-away dot is a camera-distance behaviour with no meaning on a map.
		if (m_wOverlayCircle)
			m_wOverlayCircle.SetVisible(true);
		if (m_wUnitIcon)
			m_wUnitIcon.SetVisible(true);
		if (m_wCircleSmall)
			m_wCircleSmall.SetVisible(false);

		if (m_wText)
			m_wText.SetText(LL_LobbyManager.FormatPlayerNameRich(ResolveName(slot, mgr)));

		ApplyState(slot);
		return true;
	}

	// Falls back to the entity origin for anything without an animation.
	protected vector GetLabelWorldPosition()
	{
		vector worldTransform[4], boneTransform[4];
		m_TargetEntity.GetWorldTransform(worldTransform);
		if (m_TargetEntity.GetAnimation())
		{
			m_TargetEntity.GetAnimation().GetBoneMatrix(m_TargetBone, boneTransform);
			Math3D.MatrixMultiply4(worldTransform, boneTransform, boneTransform);
		}
		else
		{
			boneTransform = worldTransform;
		}

		return boneTransform[3] + Vector(0, HEAD_OFFSET, 0);
	}

	protected string ResolveName(notnull LL_SlotData slot, notnull LL_LobbyManager mgr)
	{
		string name = "";
		if (slot.m_iPlayerId >= 0)
			name = mgr.GetPlayerName(slot.m_iPlayerId);
		if (name == "")
			name = slot.m_sName;

		return name;
	}

	protected void ApplyState(notnull LL_SlotData slot)
	{
		if (slot.IsDestroyed())
		{
			Color deadColor = Color.FromInt(m_FactionColor.PackToInt());
			deadColor.Lerp(m_GreyColor, 0.6);

			if (m_wIconBackground)
				m_wIconBackground.SetColor(deadColor);
			if (m_wCircleSmall)
				m_wCircleSmall.SetColor(deadColor);
			if (m_wIconCircle)
				m_wIconCircle.SetColor(m_GreyColor);

			m_wRoot.SetOpacity(0.6);
			return;
		}

		if (m_wIconBackground)
			m_wIconBackground.SetColor(m_FactionColor);
		if (m_wCircleSmall)
			m_wCircleSmall.SetColor(m_FactionColor);

		if (m_wIconCircle)
		{
			if (m_CharController && m_CharController.IsUnconscious())
				m_wIconCircle.SetColor(Color.Yellow);
			else
				m_wIconCircle.SetColor(m_OutlineColor);
		}

		m_wRoot.SetOpacity(1.0);
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		m_bIsHovered = true;
		return super.OnMouseEnter(w, x, y);
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		m_bIsHovered = false;
		return super.OnMouseLeave(w, enterW, x, y);
	}
}