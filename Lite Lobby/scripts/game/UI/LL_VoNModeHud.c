// Popup on the left screen border showing the voice mode when it changes; holds, then
// fades. Cycling quickly reuses the live popup. Created only during gameplay, so the
// widget goes straight onto the workspace.
class LL_VoNModeHud : ScriptedWidgetComponent
{
	protected const ResourceName LAYOUT = "{7C4B92A1E0D1F2A5}UI/HUD/VoNModePopup.layout";
	protected const float HOLD_S = 1.2;
	protected const float FADE_S = 0.4;
	protected const int TICK_MS = 32;

	protected static LL_VoNModeHud s_Active;

	protected Widget m_wRoot;
	protected TextWidget m_wModeName;
	protected float m_fElapsed;

	static void Show(LL_EVoNMode mode)
	{
		if (s_Active)
		{
			s_Active.Begin(mode);
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget root = workspace.CreateWidgets(LAYOUT, null);
		if (!root)
			return;

		// A new FrameWidgetSlot child defaults to zero size; stretch the root so the
		// pill's anchors resolve.
		FrameSlot.SetAnchorMin(root, 0, 0);
		FrameSlot.SetAnchorMax(root, 1, 1);
		FrameSlot.SetOffsets(root, 0, 0, 0, 0);

		LL_VoNModeHud comp = LL_VoNModeHud.Cast(root.FindHandler(LL_VoNModeHud));
		if (!comp)
		{
			root.RemoveFromHierarchy();
			return;
		}

		s_Active = comp;
		comp.Begin(mode);
		GetGame().GetCallqueue().CallLater(comp.Tick, TICK_MS, true);
	}

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wModeName = TextWidget.Cast(w.FindAnyWidget("ModeName"));
	}

	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);

		GetGame().GetCallqueue().Remove(Tick);

		if (s_Active == this)
			s_Active = null;

		m_wRoot = null;
		m_wModeName = null;
	}

	protected void Begin(LL_EVoNMode mode)
	{
		m_fElapsed = 0;

		if (m_wModeName)
			m_wModeName.SetText(LL_VoNModes.GetDisplayName(mode));

		if (m_wRoot)
			m_wRoot.SetOpacity(1);
	}

	protected void Tick()
	{
		m_fElapsed += TICK_MS / 1000.0;

		if (m_fElapsed <= HOLD_S)
			return;

		float fade = 1.0 - (m_fElapsed - HOLD_S) / FADE_S;
		if (fade <= 0)
		{
			Dismiss();
			return;
		}

		if (m_wRoot)
			m_wRoot.SetOpacity(fade);
	}

	protected void Dismiss()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}
}