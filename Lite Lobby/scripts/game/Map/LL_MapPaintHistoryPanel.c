//! Consume history-window input before it reaches the map underneath.
class LL_MapPaintHistoryPanelHandler : ScriptedWidgetEventHandler
{
	protected LL_MapPaintHistoryPanel m_Panel;

	void LL_MapPaintHistoryPanelHandler(LL_MapPaintHistoryPanel panel)
	{
		m_Panel = panel;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		return true;
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		m_Panel.Scroll(wheel);
		return true;
	}
}

//! Rebuilds the list once per open, after a complete owner-only snapshot arrives.
class LL_MapPaintHistoryPanel
{
	protected static const float WIDTH = 400;
	protected static const float HEIGHT = 420;
	protected static const float MIN_ROW_HEIGHT = 24;
	protected static const float ROW_GAP = 10;
	protected static const float ROW_WIDTH = WIDTH - 52;
	protected static const int REQUEST_TIMEOUT = 10000;
	protected FrameWidget m_wRoot;
	protected CanvasWidget m_wBackground;
	// CanvasWidget retains the command array until the window is removed.
	protected ref array<ref CanvasWidgetCommand> m_BackgroundCommands = {};
	protected ScrollLayoutWidget m_wScroll;
	protected SizeLayoutWidget m_wContentSize;
	protected FrameWidget m_wContent;
	protected TextWidget m_wStatus;
	protected ref LL_MapPaintHistoryPanelHandler m_Handler;
	protected SCR_MapMarkerSyncComponent m_Sync;
	protected string m_sFactionKey;
	protected int m_iRequest = -1;
	protected int m_iRequestedAt;
	protected bool m_bWaiting;

	void ~LL_MapPaintHistoryPanel()
	{
		Close();
	}

	bool IsOpen()
	{
		return m_wRoot != null;
	}

	bool ContainsPoint(float x, float y)
	{
		return LL_MapPaintWidgets.IsPointInsideWidget(m_wRoot, x, y);
	}

	void Close()
	{
		StopWaiting();
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		m_wBackground = null;
		m_BackgroundCommands.Clear();
		m_wScroll = null;
		m_wContentSize = null;
		m_wContent = null;
		m_wStatus = null;
		m_Handler = null;
	}

	protected void StopWaiting()
	{
		if (m_Sync)
			m_Sync.LL_CancelPaintHistory();
		m_Sync = null;
		m_bWaiting = false;
		m_iRequest = -1;
	}

	protected void CreateWindow(WorkspaceWidget workspace, SCR_MapToolEntry menuEntry)
	{
		Close();
		m_wRoot = LL_MapPaintWidgets.CreateMapPanelRoot(workspace, "LL_MapPaintHistory", -(WIDTH + 16), -(HEIGHT + 16), WIDTH, HEIGHT);
		if (!m_wRoot)
			return;
		m_wRoot.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Handler = new LL_MapPaintHistoryPanelHandler(this);
		m_wRoot.AddHandler(m_Handler);
		Position(workspace, menuEntry);
		m_wBackground = LL_MapPaintWidgets.CreatePaintCanvas(workspace, m_wRoot, "LL_MapPaintHistoryBackground", 0);
		if (!m_wBackground)
		{
			Close();
			return;
		}
		m_wBackground.SetIsColorInherited(false);
		m_wStatus = LL_MapPaintWidgets.CreatePanelText(workspace, m_wRoot, "LL_MapPaintHistoryStatus", 16, 12, WIDTH - 16, 40, WidgetManager.Translate("#LL-MapPaint_Loading"));
		if (m_wStatus)
			m_wStatus.SetZOrder(31);
		if (!m_wStatus || !CreateScrollContent(workspace))
		{
			Close();
			return;
		}
		m_wRoot.Update();
		UpdateBackground();
	}

	protected void UpdateBackground()
	{
		bool changed = LL_MapPaintWidgets.ConfigurePixelCanvas(m_wBackground);
		if (!changed && !m_BackgroundCommands.IsEmpty())
			return;
		float width, height;
		m_wBackground.GetScreenSize(width, height);
		if (width <= 0 || height <= 0)
			return;
		// Bypass the default panel style's translucency with a fully opaque fill.
		PolygonDrawCommand background = new PolygonDrawCommand();
		background.m_iColor = LL_MapPaintSettings.PANEL_COLOR;
		background.m_Vertices = {0, 0, width, 0, width, height, 0, height};
		m_BackgroundCommands.Clear();
		m_BackgroundCommands.Insert(background);
		m_wBackground.SetDrawCommands(m_BackgroundCommands);
	}

	protected bool CreateScrollContent(WorkspaceWidget workspace)
	{
		m_wScroll = ScrollLayoutWidget.Cast(LL_MapPaintWidgets.CreatePanelWidget(workspace, m_wRoot, WidgetType.ScrollLayoutWidgetTypeID, "LL_MapPaintHistoryScroll", 0, 12, 12, WIDTH - 12, HEIGHT - 12, 0xffffffff));
		if (!m_wScroll)
			return false;
		// Keep the clip rectangle fixed to the visible scroll area.
		m_wScroll.ClearFlags(WidgetFlags.INHERIT_CLIPPING);
		m_wScroll.SetFlags(WidgetFlags.CLIPCHILDREN);
		// Scrolling containers must retain the viewport's clip rectangle, not replace it
		// with their own full content bounds as they move outside the viewport.
		WidgetFlags contentFlags = WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.INHERIT_CLIPPING | WidgetFlags.DO_NOT_CLIP_RECT | WidgetFlags.CLIPCHILDREN;
		m_wContentSize = SizeLayoutWidget.Cast(workspace.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, contentFlags, Color.White, 0, m_wScroll));
		if (!m_wContentSize)
			return false;
		ScrollLayoutSlot.SetHorizontalAlign(m_wContentSize, LayoutHorizontalAlign.Stretch);
		ScrollLayoutSlot.SetVerticalAlign(m_wContentSize, LayoutVerticalAlign.Top);
		m_wContentSize.EnableHeightOverride(true);
		m_wContentSize.SetHeightOverride(0);
		m_wContent = FrameWidget.Cast(workspace.CreateWidget(WidgetType.FrameWidgetTypeID, contentFlags, Color.White, 0, m_wContentSize));
		return m_wContent != null;
	}

	void Open(WorkspaceWidget workspace, SCR_MapToolEntry menuEntry, SCR_MapEntity mapEntity, SCR_MapMarkerSyncComponent sync)
	{
		CreateWindow(workspace, menuEntry);
		if (!IsOpen())
			return;
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		m_sFactionKey = string.Empty;
		if (manager)
			m_sFactionKey = manager.LL_GetLocalPaintFactionKey();
		if (LL_MapPaintPermissions.IsLocalPaintFallback(manager, sync))
		{
			ShowEntries(mapEntity.LL_GetLocalPaintHistory().Snapshot(System.GetTickCount()));
			return;
		}
		m_Sync = sync;
		if (m_Sync)
			m_iRequest = m_Sync.LL_RequestPaintHistory();
		m_iRequestedAt = System.GetTickCount();
		m_bWaiting = m_iRequest >= 0;
		if (!m_bWaiting)
			m_wStatus.SetText(WidgetManager.Translate("#LL-MapPaint_Unavailable"));
	}

#ifdef ENF_WB
	Widget LL_TestShowEntries(WorkspaceWidget workspace, array<ref LL_MapPaintHistoryEntry> entries)
	{
		CreateWindow(workspace, null);
		if (IsOpen())
			ShowEntries(entries);
		return m_wRoot;
	}
#endif

	protected void Position(WorkspaceWidget workspace, SCR_MapToolEntry menuEntry)
	{
		if (!menuEntry || !menuEntry.m_ButtonComp)
			return;
		LL_MapPaintWidgets.PositionPanelBesideMenu(workspace, m_wRoot, menuEntry.m_ButtonComp.GetRootWidget());
	}

	//! Only a pending opening can consume a snapshot; a displayed list never updates.
	void Update()
	{
		if (!IsOpen())
			return;
		UpdateBackground();
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager && manager.LL_GetLocalPaintFactionKey() != m_sFactionKey)
		{
			Close();
			return;
		}
		if (!m_bWaiting)
			return;
		LL_MapPaintHistorySnapshot snapshot = m_Sync.LL_GetPaintHistorySnapshot(m_iRequest);
		if (snapshot)
		{
			if (snapshot.m_bAvailable)
				ShowEntries(snapshot.m_Entries);
			else
				m_wStatus.SetText(WidgetManager.Translate("#LL-MapPaint_Unavailable"));
			StopWaiting();
		}
		else if (System.GetTickCount() - m_iRequestedAt > REQUEST_TIMEOUT)
		{
			StopWaiting();
			m_wStatus.SetText(WidgetManager.Translate("#LL-MapPaint_Timeout"));
		}
	}

	protected void ShowEntries(array<ref LL_MapPaintHistoryEntry> entries)
	{
		m_wStatus.SetVisible(entries.IsEmpty());
		m_wStatus.SetText(WidgetManager.Translate("#LL-MapPaint_Empty"));
		// Resolve the available width before measuring wrapped text in physical pixels.
		m_wContentSize.SetHeightOverride(MIN_ROW_HEIGHT);
		m_wRoot.Update();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float top;
		foreach (int index, LL_MapPaintHistoryEntry entry : entries)
			top += CreateRow(workspace, index, top, entry);
		m_wContentSize.SetHeightOverride(Math.Max(0, top - ROW_GAP));
		m_wRoot.Update();
		m_wScroll.Update();
		m_wScroll.SetSliderPos(0, 0);
	}

	//! Return the occupied height after measuring native text wrapping.
	protected float CreateRow(WorkspaceWidget workspace, int index, float top, LL_MapPaintHistoryEntry entry)
	{
		TextWidget label = LL_MapPaintWidgets.CreatePanelText(workspace, m_wContent, string.Format("LL_HistoryEntry%1", index), 4, top, 4 + ROW_WIDTH, top + MIN_ROW_HEIGHT, entry.GetText());
		if (!label)
			return 0;
		ConfigureRowText(label);
		label.Update();
		float textWidth, textHeight;
		label.GetTextSize(textWidth, textHeight);
		float rowHeight = Math.Max(MIN_ROW_HEIGHT, workspace.DPIUnscale(textHeight));
		FrameSlot.SetSize(label, ROW_WIDTH, rowHeight);
		// An identically sized prefix preserves native wrapping and literal nicknames.
		TextWidget author = LL_MapPaintWidgets.CreatePanelText(workspace, m_wContent, string.Format("LL_HistoryAuthor%1", index), 4, top, 4 + ROW_WIDTH, top + rowHeight, LL_MapPaintHistory.GetDisplayName(entry.m_sAuthor), LL_MapPaintSettings.ACCENT_COLOR);
		if (author)
		{
			ConfigureRowText(author);
			author.SetColorInt(LL_MapPaintSettings.ACCENT_COLOR);
			author.SetShadow(2, 0x99c26414, 1, 0, 0);
			author.SetZOrder(31);
		}
		return rowHeight + ROW_GAP;
	}

	protected void ConfigureRowText(TextWidget label)
	{
		label.ClearFlags(WidgetFlags.CENTER | WidgetFlags.VCENTER | WidgetFlags.RALIGN | WidgetFlags.NOWRAP);
		label.SetFlags(WidgetFlags.INHERIT_CLIPPING | WidgetFlags.DO_NOT_CLIP_RECT | WidgetFlags.CLIPCHILDREN | WidgetFlags.NO_LOCALIZATION);
		label.SetTextOffset(0, 0);
		label.SetTextWrapping(true);
	}

	void Scroll(int wheel)
	{
		if (!m_wScroll || wheel == 0)
			return;
		float x, y;
		m_wScroll.GetSliderPosPixels(x, y);
		float step = GetGame().GetWorkspace().DPIScale(MIN_ROW_HEIGHT + ROW_GAP);
		if (wheel > 0)
			step = -step;
		m_wScroll.SetSliderPosPixels(0, y + step);
	}
}
