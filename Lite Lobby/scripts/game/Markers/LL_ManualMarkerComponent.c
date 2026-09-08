// Widget of a static map marker; every value comes from LL_ManualMarker through setters.
class LL_ManualMarkerComponent : SCR_ScriptedWidgetComponent
{
	protected SCR_MapEntity m_MapEntity;

	protected ImageWidget m_wMarkerIcon;
	protected FrameWidget m_wMarkerFrame;
	protected RichTextWidget m_wDescriptionText;
	protected PanelWidget m_wDescriptionPanel;
	protected OverlayWidget m_wMarkerScrollLayout;

	protected string m_sDescription;
	protected int m_iZOrder;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		m_MapEntity = SCR_MapEntity.GetMapInstance();
		m_wMarkerIcon = ImageWidget.Cast(w.FindAnyWidget("MarkerIcon"));
		m_wMarkerFrame = FrameWidget.Cast(w.FindAnyWidget("MarkerFrame"));
		m_wDescriptionText = RichTextWidget.Cast(w.FindAnyWidget("DescriptionText"));
		m_wMarkerScrollLayout = OverlayWidget.Cast(w.FindAnyWidget("MarkerScrollLayout"));
		m_wDescriptionPanel = PanelWidget.Cast(w.FindAnyWidget("DescriptionPanel"));
	}

	float GetYScale()
	{
		int x, y;
		m_wMarkerIcon.GetImageSize(0, x, y);
		if (y == 0) y = 1;
		if (x == 0) x = 1;
		float scale = (float) y / (float) x;
		return scale;
	}

	void SetImage(ResourceName m_sImageSet, string quadName)
	{
		if (m_sImageSet.EndsWith(".edds"))
			m_wMarkerIcon.LoadImageTexture(0, m_sImageSet);
		else
			m_wMarkerIcon.LoadImageFromSet(0, m_sImageSet, quadName);
	}
	void SetDescription(string description)
	{
		m_sDescription = description;

		m_wDescriptionText.SetText(description);
	}
	void SetColor(Color color)
	{
		m_wMarkerIcon.SetColor(color);
	}

	void SetOpacity(float opacity)
	{
		m_wRoot.SetOpacity(opacity);
	}

	void SetSlotWorld(vector worldPosition, vector rotation, float worldSize, bool useWorldScale, float minSize = 0.0)
	{
		float wX, wY, screenX, screenY, screenXEnd, screenYEnd;
		wX = worldPosition[0];
		wY = worldPosition[2];
		m_MapEntity.WorldToScreen(wX, wY, screenX, screenY, true);
		m_MapEntity.WorldToScreen(wX + worldSize, wY + worldSize, screenXEnd, screenYEnd, true);

		float screenXD = GetGame().GetWorkspace().DPIUnscale(screenX);
		float screenYD = GetGame().GetWorkspace().DPIUnscale(screenY);
		float sizeXD = worldSize;
		float sizeYD = worldSize;
		if (useWorldScale)
		{
			sizeXD = GetGame().GetWorkspace().DPIUnscale(screenXEnd - screenX);
			sizeYD = GetGame().GetWorkspace().DPIUnscale(screenY - screenYEnd);
		}
		if (minSize > 0)
		{
			if (sizeXD < minSize) sizeXD = minSize;
			if (sizeYD < minSize) sizeYD = minSize;
		}
		sizeYD *= GetYScale();

		SetSlot(screenXD, screenYD, sizeXD, sizeYD, rotation[0] - 90);
	}

	void SetSlot(float posX, float posY, float sizeX, float sizeY, float rotation)
	{
		FrameSlot.SetPos(m_wRoot, posX, posY);

		FrameSlot.SetPos(m_wMarkerIcon, -sizeX/2, -sizeY/2);
		FrameSlot.SetSize(m_wMarkerIcon, sizeX, sizeY);
		FrameSlot.SetPos(m_wMarkerScrollLayout, -sizeX/2, -sizeY/2);
		FrameSlot.SetSize(m_wMarkerScrollLayout, sizeX, sizeY);

		float panelX, panelY;
		m_wDescriptionPanel.GetScreenSize(panelX, panelY);
		float panelXD = GetGame().GetWorkspace().DPIUnscale(panelX);
		float panelYD = GetGame().GetWorkspace().DPIUnscale(panelY);
		if (panelX == 0)
			m_wDescriptionPanel.SetOpacity(0);
		else
			m_wDescriptionPanel.SetOpacity(1);
		FrameSlot.SetPos(m_wDescriptionPanel, -panelXD/2, -panelYD/2);
		m_wMarkerIcon.SetRotation(rotation);
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		m_iZOrder = m_wRoot.GetZOrder();
		m_wRoot.SetZOrder(10000);
		if (m_sDescription != "") m_wDescriptionPanel.SetVisible(true);

		return true;
	}
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		m_wRoot.SetZOrder(m_iZOrder);
		m_wDescriptionPanel.SetVisible(false);

		return true;
	}
};