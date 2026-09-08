// Effects volume stepping (F3 quieter, F4 louder) with a 5-segment popup. The lever is
// AudioSystem.SetMasterVolume(SFX), a runtime multiplier on the configured volume (the
// channel vanilla uses to mute SFX during loading). Session-local: nothing is written.
class LL_EffectsVolume
{
	static const int LEVELS = 5;

	// -1 = derived from the engine's current multiplier on first use.
	protected static int s_iLevel = -1;

	static void Step(int direction)
	{
		if (s_iLevel < 0)
		{
			float current = AudioSystem.GetMasterVolume(AudioSystem.SFX);
			s_iLevel = Math.ClampInt(Math.Round(current * LEVELS), 1, LEVELS);
		}

		s_iLevel = Math.ClampInt(s_iLevel + direction, 1, LEVELS);
		AudioSystem.SetMasterVolume(AudioSystem.SFX, s_iLevel * 0.2);

		LL_EffectsVolumeHud.Show(s_iLevel);
	}
}

// Same lifecycle as the voice-mode popup: brief hold, fade, rapid presses reuse the widget.
class LL_EffectsVolumeHud : ScriptedWidgetComponent
{
	protected const ResourceName LAYOUT = "{69D8B4C2F1E20301}UI/HUD/EffectsVolumePopup.layout";
	protected const float HOLD_S = 1.2;
	protected const float FADE_S = 0.4;
	protected const int TICK_MS = 32;

	protected static LL_EffectsVolumeHud s_Active;

	protected Widget m_wRoot;
	protected ref array<ImageWidget> m_aSegments = {};
	protected float m_fElapsed;

	static void Show(int level)
	{
		if (s_Active)
		{
			s_Active.Begin(level);
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget root = workspace.CreateWidgets(LAYOUT, null);
		if (!root)
			return;

		// A new FrameWidgetSlot child defaults to zero size; stretch the root so the
		// pill's offsets resolve.
		FrameSlot.SetAnchorMin(root, 0, 0);
		FrameSlot.SetAnchorMax(root, 1, 1);
		FrameSlot.SetOffsets(root, 0, 0, 0, 0);

		LL_EffectsVolumeHud comp = LL_EffectsVolumeHud.Cast(root.FindHandler(LL_EffectsVolumeHud));
		if (!comp)
		{
			root.RemoveFromHierarchy();
			return;
		}

		s_Active = comp;
		comp.Begin(level);
		GetGame().GetCallqueue().CallLater(comp.Tick, TICK_MS, true);
	}

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;

		for (int i = 1; i <= LL_EffectsVolume.LEVELS; i++)
		{
			ImageWidget segment = ImageWidget.Cast(w.FindAnyWidget("Seg" + i));
			if (segment)
				m_aSegments.Insert(segment);
		}
	}

	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);

		GetGame().GetCallqueue().Remove(Tick);

		if (s_Active == this)
			s_Active = null;

		m_wRoot = null;
		m_aSegments.Clear();
	}

	protected void Begin(int level)
	{
		m_fElapsed = 0;

		for (int i = 0; i < m_aSegments.Count(); i++)
		{
			if (i < level)
				m_aSegments[i].SetColor(new Color(0.761, 0.392, 0.078, 1));
			else
				m_aSegments[i].SetColor(new Color(0.35, 0.35, 0.35, 0.8));
		}

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