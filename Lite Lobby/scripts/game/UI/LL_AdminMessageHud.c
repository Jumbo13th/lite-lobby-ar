// Transient announcement banner (/message and mission triggers): drains over a fixed
// window and removes itself. Client-side, nothing replicated; a late joiner misses an
// old one. Parented under the open menu's root with a high z-order because workspace
// widgets render below menus.

class LL_AdminMessageHud : ScriptedWidgetComponent
{
	protected const ResourceName LAYOUT = "{1D13D4E5F6A7B8C9}UI/HUD/AdminMessage.layout";
	protected const float DURATION_S = 10.0;
	protected const int TICK_MS = 32;

	protected static LL_AdminMessageHud s_Active;

	protected Widget m_wRoot;
	protected TextWidget m_wHeader;
	protected RichTextWidget m_wMessageText;
	protected Widget m_wProgressBar;
	protected float m_fElapsed;
	// No-op without a workspace (dedicated server).
	static void ShowMessage(string text, string title)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (s_Active)
			s_Active.Dismiss();

		Widget parent = null;
		MenuManager menuMgr = GetGame().GetMenuManager();
		if (menuMgr)
		{
			MenuBase topMenu = menuMgr.GetTopMenu();
			if (topMenu)
				parent = topMenu.GetRootWidget();
		}

		Widget root = workspace.CreateWidgets(LAYOUT, parent);
		if (!root)
			return;

		// A FrameWidgetSlot defaults to zero size in the top-left corner; the root is
		// stretched so the banner's centred anchors resolve.
		FrameSlot.SetAnchorMin(root, 0, 0);
		FrameSlot.SetAnchorMax(root, 1, 1);
		FrameSlot.SetOffsets(root, 0, 0, 0, 0);

		root.SetZOrder(1000);

		LL_AdminMessageHud comp = LL_AdminMessageHud.Cast(root.FindHandler(LL_AdminMessageHud));
		if (!comp)
		{
			root.RemoveFromHierarchy();
			return;
		}

		comp.Begin(text, title);
	}

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wHeader = TextWidget.Cast(w.FindAnyWidget("Header"));
		m_wMessageText = RichTextWidget.Cast(w.FindAnyWidget("MessageText"));
		m_wProgressBar = w.FindAnyWidget("ProgressBar");
	}

	// The host menu can close mid-display and take the widget with it.
	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);

		GetGame().GetCallqueue().Remove(Tick);

		if (s_Active == this)
			s_Active = null;

		m_wRoot = null;
		m_wHeader = null;
		m_wMessageText = null;
		m_wProgressBar = null;
	}

	protected void Begin(string text, string title)
	{
		s_Active = this;
		m_fElapsed = 0;

		if (m_wHeader)
			m_wHeader.SetText(title);
		if (m_wMessageText)
			m_wMessageText.SetText(text);

		// TASK_CREATED is a prominent jingle (HINT was too quiet); force cuts through other
		// UI sounds. Loudness is the event choice; there is no gain parameter.
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TASK_CREATED, true);

		if (m_wProgressBar)
			FrameSlot.SetAnchorMax(m_wProgressBar, 1, 1);

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	protected void Tick()
	{
		m_fElapsed += TICK_MS / 1000.0;

		float frac = 1.0 - (m_fElapsed / DURATION_S);
		if (frac < 0)
			frac = 0;

		if (m_wProgressBar)
			FrameSlot.SetAnchorMax(m_wProgressBar, frac, 1);

		if (m_fElapsed >= DURATION_S)
			Dismiss();
	}

	protected void Dismiss()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}
}