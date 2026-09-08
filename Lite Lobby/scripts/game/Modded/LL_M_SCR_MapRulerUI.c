// Ruler/protractor on a map without a controlled character: vanilla SetVisible aborts
// unless the player carries a map gadget, so the no-gadget case falls back to the
// component's configured ruler length and the default protractor texture.

modded class SCR_MapRulerUI
{
	// Vanilla default from SCR_MapGadgetComponentClass.m_sProtractorTexture.
	protected static const ResourceName LL_FALLBACK_PROTRACTOR_TEXTURE = "{FC855D20AA561819}UI/Textures/Map/ProtractorScale.edds";

	// Mirrors the gadget-resolution chain vanilla SetVisible uses.
	protected bool LL_HasMapGadgetData()
	{
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || !player.GetCharacterController())
			return false;

		IEntity mapItem = player.GetCharacterController().GetAttachedGadgetAtLeftHandSlot();
		if (!mapItem)
		{
			SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(player);
			if (!gadgetManager)
				return false;

			mapItem = gadgetManager.GetGadgetByType(EGadgetType.MAP);
			if (!mapItem)
				return false;
		}

		SCR_MapGadgetComponent mapGadget = SCR_MapGadgetComponent.Cast(mapItem.FindComponent(SCR_MapGadgetComponent));
		if (!mapGadget)
			return false;

		return SCR_MapGadgetComponentClass.Cast(mapGadget.GetComponentData(mapItem)) != null;
	}

	override protected void SetVisible(bool visible, bool saveState = false)
	{
		if (!visible || LL_HasMapGadgetData())
		{
			super.SetVisible(visible, saveState);
			return;
		}

		// Vanilla's visible branch with the gadget reads replaced.
		m_bIsVisible = visible;
		m_wFrame.SetEnabled(visible);
		m_wFrame.SetVisible(visible);

		if (!m_wImage)
			return;

		if (m_wImage.LoadImageTexture(0, LL_FALLBACK_PROTRACTOR_TEXTURE))
			m_wImage.SetImage(0);

		float zoomVal = m_MapEntity.GetCurrentZoom();
		m_fSizeCoef = 1000 / (m_fRulerLength / m_fBaseImageSize[0]);
		float sizeVal = m_wWorkspace.DPIUnscale(zoomVal * m_fSizeCoef);
		SetSize(sizeVal, sizeVal);

		if (m_fPosX == 0 && m_fPosY == 0)
		{
			float sizeX, sizeY;
			m_MapEntity.GetMapWidget().GetScreenSize(sizeX, sizeY);
			m_fPosX = sizeX * 0.5;
			m_fPosY = sizeY * 0.5;

			m_MapEntity.ScreenToWorld(m_fPosX, m_fPosY, m_fWorldX, m_fWorldY);
		}

		FrameSlot.SetPos(m_wFrame, m_wWorkspace.DPIUnscale(m_fPosX), m_wWorkspace.DPIUnscale(m_fPosY));
		m_wImage.SetRotation(m_fAngle);
		m_vMapPan = m_MapEntity.GetCurrentPan();

		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		m_MapEntity.GetOnMapPan().Insert(OnMapPan);

		if (m_ToolMenuEntry)
			m_ToolMenuEntry.SetActive(visible);
	}
}