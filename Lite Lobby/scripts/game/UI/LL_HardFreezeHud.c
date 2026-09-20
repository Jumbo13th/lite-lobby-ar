// Banner for the hard freeze, read from the replicated scalars so a late joiner sees
// the correct state. The release notice is a local 3-second edge, not replicated.
// Z-order 100 so admin announcements (1000) stay readable over the hold.

class LL_HardFreezeHud : ScriptedWidgetComponent
{
	protected const int TICK_MS = 200;
	protected const float RESUMED_NOTICE_S = 3.0;
	protected const int Z_ORDER = 100;

	// The layout's box is 720 wide with 24 px above and below the column; the height is set
	// from the text every tick, so a one-word notice and the resume text get a box that fits.
	protected const float BANNER_WIDTH = 720;
	protected const float BANNER_PAD_Y = 24;

	protected Widget m_wRoot;
	protected Widget m_wBanner;
	protected TextWidget m_wTitle;
	protected TextWidget m_wDetail;
	protected LL_GameModeCoop m_GameMode;

	protected bool m_bWasHolding;
	protected float m_fResumedNoticeLeft;
	protected float m_fBannerHeight;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wBanner = w.FindAnyWidget("Banner");
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

		FitBanner();

		bool holding = LL_GameModeCoop.IsHardFreezeActive();

		if (m_bWasHolding && !holding)
			m_fResumedNoticeLeft = RESUMED_NOTICE_S;
		m_bWasHolding = holding;

		if (holding)
		{
			// A hold with no countdown is the session-resume hold, released by the admin.
			float remainingRaw = m_GameMode.GetHardFreezeRemaining();
			if (remainingRaw < 0)
			{
				ShowResumeBanner();
				return;
			}

			// Ceil so the last whole second reads "1".
			int remaining = Math.Ceil(remainingRaw);
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

	// "N of M" comes from the slot table every client already has: M = slots with a holder,
	// N = holders not on the replicated disconnect list. Nothing is added to the wire.
	protected void ShowResumeBanner()
	{
		int back, expected;
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
		{
			foreach (LL_SlotData slot : mgr.GetSlots())
			{
				if (slot.m_iPlayerId < 0)
					continue;

				expected++;
				if (!mgr.IsPlayerDisconnected(slot.m_iPlayerId))
					back++;
			}
		}

		m_wRoot.SetVisible(true);
		if (m_wTitle)
			m_wTitle.SetText("#LL-Resume_HoldTitle");
		if (!m_wDetail)
			return;

		int snapshotSeconds = m_GameMode.GetResumedSnapshotTime();
		string snapshotTime = SCR_FormatHelper.FormatTime(snapshotSeconds);
		m_wDetail.SetText(WidgetManager.Translate("#LL-Resume_HoldBody", snapshotTime, back, expected) + "\n" + WidgetManager.Translate("#LL-Resume_HoldLimits"));
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

	// Measured, not laid out: the text sizes are known only after a layout pass, so the box
	// follows one tick behind a change. Screen sizes are DPI-scaled; slot sizes are not.
	protected void FitBanner()
	{
		if (!m_wBanner)
			return;

		float width, titleHeight, detailHeight;
		if (m_wTitle)
			m_wTitle.GetScreenSize(width, titleHeight);
		if (m_wDetail)
			m_wDetail.GetScreenSize(width, detailHeight);

		float height = GetGame().GetWorkspace().DPIUnscale(titleHeight + detailHeight) + 2 * BANNER_PAD_Y;
		if (Math.AbsFloat(height - m_fBannerHeight) < 1)
			return;

		m_fBannerHeight = height;
		FrameSlot.SetSize(m_wBanner, BANNER_WIDTH, height);
		FrameSlot.SetPos(m_wBanner, -BANNER_WIDTH / 2, -height / 2);
	}
}