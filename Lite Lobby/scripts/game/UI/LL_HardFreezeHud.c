// Banner for the hard freeze, read from the replicated scalars so a late joiner sees
// the correct state. The release notice is a local 3-second edge, not replicated.
// Z-order 100 so admin announcements (1000) stay readable over the hold.

class LL_HardFreezeHud : ScriptedWidgetComponent
{
	protected const int TICK_MS = 200;
	protected const float RESUMED_NOTICE_S = 3.0;
	protected const int Z_ORDER = 100;

	protected Widget m_wRoot;
	protected TextWidget m_wTitle;
	protected TextWidget m_wDetail;
	protected LL_GameModeCoop m_GameMode;

	protected bool m_bWasHolding;
	protected float m_fResumedNoticeLeft;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wTitle = TextWidget.Cast(w.FindAnyWidget("PauseTitleText"));
		m_wDetail = TextWidget.Cast(w.FindAnyWidget("PauseDetailText"));
		m_GameMode = LL_GameModeCoop.GetInstance();

		w.SetZOrder(Z_ORDER);
		w.SetVisible(false);

		GetGame().GetCallqueue().CallLater(UpdateDisplay, TICK_MS, true);
	}

	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);
		GetGame().GetCallqueue().Remove(UpdateDisplay);
	}

	protected void UpdateDisplay()
	{
		if (!m_GameMode)
			m_GameMode = LL_GameModeCoop.GetInstance();
		if (!m_GameMode || !m_wRoot)
			return;

		bool holding = LL_GameModeCoop.IsHardFreezeActive();

		if (m_bWasHolding && !holding)
			m_fResumedNoticeLeft = RESUMED_NOTICE_S;
		m_bWasHolding = holding;

		if (holding)
		{
			// Ceil so the last whole second reads "1".
			int remaining = Math.Ceil(m_GameMode.GetHardFreezeRemaining());
			ShowBanner("#LL-HardFreeze_Title", "#LL-HardFreeze_Detail", remaining);
			return;
		}

		if (m_fResumedNoticeLeft > 0)
		{
			m_fResumedNoticeLeft -= TICK_MS / 1000.0;
			ShowBanner("#LL-HardFreeze_Resumed", "");
			return;
		}

		m_wRoot.SetVisible(false);
	}

	// Both widgets translate keys themselves; SetTextFormat substitutes %1. seconds < 0
	// = no count.
	protected void ShowBanner(string titleKey, string detailKey, int seconds = -1)
	{
		m_wRoot.SetVisible(true);

		if (m_wTitle)
			m_wTitle.SetText(titleKey);

		if (!m_wDetail)
			return;

		if (detailKey == "")
			m_wDetail.SetText("");
		else if (seconds < 0)
			m_wDetail.SetText(detailKey);
		else
			m_wDetail.SetTextFormat(detailKey, seconds);
	}
}