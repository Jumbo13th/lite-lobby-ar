// Mission description panel for the briefing and in-game map screens: a folder list of
// LL_MissionDescription entries plus the generated Objectives, Squad Roster and
// Communications entries. The panel creates the list and hands itself down.

class LL_MissionDescriptionPanel : ScriptedWidgetComponent
{
	protected ResourceName m_sListLayout = "{AF74B5DC3E8064BC}UI/Briefing/MissionDescriptionList.layout";

	// Plain text content layout, reused for the generated entries so they share the
	// default entry's margins.
	protected ResourceName m_sTextLayout = "{CB96D7FE5AA286DE}UI/Briefing/MissionDescriptionText.layout";

	protected ResourceName m_sSelectorLayout = "{BA85C6ED4F9175CD}UI/Briefing/MissionDescriptionSelector.layout";

	// The vertical scrollbar overlays the right ~18 px of the 450 px content.
	static const float RICH_WRAP_WIDTH = 430;

	protected Widget m_wRoot;
	protected FrameWidget m_wContentFrame;
	protected Widget m_wCurrentContent;
	protected TextWidget m_wHeaderText;
	protected ImageWidget m_wFolderIcon;

	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		m_wContentFrame = FrameWidget.Cast(w.FindAnyWidget("ContentFrame"));
		m_wHeaderText = TextWidget.Cast(w.FindAnyWidget("MissionDescriptionHeaderText"));
		m_wFolderIcon = ImageWidget.Cast(w.FindAnyWidget("FolderIcon"));

		if (!GetGame().InPlayMode())
			return;

		// Sibling handlers on the header button may not be attached yet.
		GetGame().GetCallqueue().CallLater(WireHeaderButton, 0);

		ShowList();
	}

	protected void WireHeaderButton()
	{
		Widget wHeaderButton = m_wRoot.FindAnyWidget("MissionDescriptionHeaderButton");
		if (!wHeaderButton)
			return;

		SCR_ButtonBaseComponent headerComp = SCR_ButtonBaseComponent.Cast(wHeaderButton.FindHandler(SCR_ButtonBaseComponent));
		if (headerComp)
			headerComp.m_OnClicked.Insert(OnHeaderClicked);
	}

	protected void OnHeaderClicked()
	{
		ShowList();
	}

	void ShowList()
	{
		ClearContent();

		if (!m_wContentFrame)
			return;

		m_wCurrentContent = GetGame().GetWorkspace().CreateWidgets(m_sListLayout, m_wContentFrame);
		if (!m_wCurrentContent)
			return;

		LL_MissionDescriptionListUI listHandler = LL_MissionDescriptionListUI.Cast(m_wCurrentContent.FindHandler(LL_MissionDescriptionListUI));
		if (listHandler)
			listHandler.Init(this);

		if (m_wHeaderText)
		{
			m_wHeaderText.SetText("#LL-Briefing_MissionDescription");
			AlignableSlot.SetPadding(m_wHeaderText, 8, 0, 0, 0);
		}
		if (m_wFolderIcon)
			m_wFolderIcon.SetVisible(false);
	}

	void ShowDescription(LL_MissionDescription description)
	{
		if (!description)
			return;

		ClearContent();

		if (!m_wContentFrame)
			return;

		m_wCurrentContent = GetGame().GetWorkspace().CreateWidgets(description.GetDescriptionLayout(), m_wContentFrame);
		if (!m_wCurrentContent)
			return;

		LL_MissionDescriptionContentUI contentHandler = LL_MissionDescriptionContentUI.Cast(m_wCurrentContent.FindHandler(LL_MissionDescriptionContentUI));
		if (contentHandler)
			contentHandler.SetDescription(description);

		if (m_wHeaderText)
		{
			m_wHeaderText.SetText(description.GetTitle());
			// Room for the 33 px back-arrow.
			AlignableSlot.SetPadding(m_wHeaderText, 41, 0, 0, 0);
		}

		if (m_wFolderIcon)
			m_wFolderIcon.SetVisible(true);
	}

	// Generated from the replicated lobby slots; no backing entity.
	void ShowRoster()
	{
		ClearContent();

		if (!m_wContentFrame)
			return;

		m_wCurrentContent = GetGame().GetWorkspace().CreateWidgets(m_sTextLayout, m_wContentFrame);
		if (!m_wCurrentContent)
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			RenderRich(m_wCurrentContent, mgr.BuildBriefingRoster(GetLocalFactionKey()));

		if (m_wHeaderText)
		{
			m_wHeaderText.SetText("#LL-Briefing_SquadRoster");
			AlignableSlot.SetPadding(m_wHeaderText, 41, 0, 0, 0);
		}
		if (m_wFolderIcon)
			m_wFolderIcon.SetVisible(true);
	}

	// Generated from the triggers; zones are clickable links that fly the map there.
	void ShowObjectives()
	{
		ClearContent();

		if (!m_wContentFrame)
			return;

		m_wCurrentContent = GetGame().GetWorkspace().CreateWidgets(m_sTextLayout, m_wContentFrame);
		if (!m_wCurrentContent)
			return;

		RenderRich(m_wCurrentContent, LL_TriggerComponent.BuildObjectivesMarkup());

		if (m_wHeaderText)
		{
			m_wHeaderText.SetText("#LL-Briefing_MissionConditions");
			AlignableSlot.SetPadding(m_wHeaderText, 41, 0, 0, 0);
		}
		if (m_wFolderIcon)
			m_wFolderIcon.SetVisible(true);
	}

	// Generated, static localized content; the key names shown are the shipped defaults.
	void ShowCommsGuide()
	{
		ClearContent();

		if (!m_wContentFrame)
			return;

		m_wCurrentContent = GetGame().GetWorkspace().CreateWidgets(m_sTextLayout, m_wContentFrame);
		if (!m_wCurrentContent)
			return;

		RenderRich(m_wCurrentContent, BuildCommsGuideMarkup());

		if (m_wHeaderText)
		{
			m_wHeaderText.SetText("#LL-Briefing_CommsGuide");
			AlignableSlot.SetPadding(m_wHeaderText, 41, 0, 0, 0);
		}
		if (m_wFolderIcon)
			m_wFolderIcon.SetVisible(true);
	}

	// Structure lives here, every sentence comes from the string table.
	protected static string BuildCommsGuideMarkup()
	{
		string COL_DIM = "150,150,150,255";

		string t = "";
		t += string.Format("<h>%1</h>", WidgetManager.Translate("#LL-CommsGuide_DirectHeader"));
		t += string.Format("%1<br/>", WidgetManager.Translate("#LL-CommsGuide_ModesBody"));
		t += "<hr/>";
		t += string.Format("<h>%1</h>", WidgetManager.Translate("#LL-CommsGuide_RadioHeader"));
		t += string.Format("%1<gap/>", WidgetManager.Translate("#LL-CommsGuide_MenuBody"));
		t += string.Format("%1<br/>", WidgetManager.Translate("#LL-CommsGuide_KeyEar"));
		t += string.Format("%1<br/>", WidgetManager.Translate("#LL-CommsGuide_KeyVolume"));
		t += string.Format("%1<gap/>", WidgetManager.Translate("#LL-CommsGuide_KeyFrequency"));
		t += string.Format("%1<br/>", WidgetManager.Translate("#LL-CommsGuide_EntryExample"));
		t += "<hr/>";
		t += string.Format("<h>%1</h>", WidgetManager.Translate("#LL-CommsGuide_EffectsHeader"));
		t += string.Format("%1<gap/>", WidgetManager.Translate("#LL-CommsGuide_EffectsBody"));
		t += string.Format("<color=%1>%2</color>", COL_DIM, WidgetManager.Translate("#LL-CommsGuide_Rebind"));
		return t;
	}

	// Renders markup into a content layout's DescriptionScroll through the rich-text
	// renderer, clipped to the scroll.
	static void RenderRich(Widget contentRoot, string markup)
	{
		if (!contentRoot)
			return;

		Widget target = contentRoot.FindAnyWidget("DescriptionScroll");
		Widget oldText = contentRoot.FindAnyWidget("DescriptionText");
		if (oldText)
			oldText.RemoveFromHierarchy();
		if (!target)
			target = contentRoot;

		target.SetFlags(WidgetFlags.CLIPCHILDREN);
		LL_RichTextUI.Render(target, markup, RICH_WRAP_WIDTH);
	}

	protected void ClearContent()
	{
		if (m_wCurrentContent)
		{
			m_wCurrentContent.RemoveFromHierarchy();
			m_wCurrentContent = null;
		}
	}

	FactionKey GetLocalFactionKey()
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		PlayerController pc = GetGame().GetPlayerController();
		if (!mgr || !pc)
			return "";

		LL_SlotData slot = mgr.FindSlotByPlayerId(pc.GetPlayerId());
		if (!slot)
			return "";

		return slot.m_sFactionKey;
	}
}

class LL_MissionDescriptionListUI : ScriptedWidgetComponent
{
	protected ResourceName m_sSelectorLayout = "{BA85C6ED4F9175CD}UI/Briefing/MissionDescriptionSelector.layout";

	protected Widget m_wRoot;
	protected LL_MissionDescriptionPanel m_Panel;

	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
	}

	void Init(LL_MissionDescriptionPanel panel)
	{
		m_Panel = panel;
		FillList();
	}

	protected void FillList()
	{
		VerticalLayoutWidget wList = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("MissionDescriptionList"));
		if (!wList || !m_Panel)
			return;

		FactionKey factionKey = m_Panel.GetLocalFactionKey();

		array<LL_MissionDescription> descriptions = {};
		LL_MissionDescription.GetDescriptions(descriptions);

		foreach (LL_MissionDescription description : descriptions)
		{
			if (!description.IsVisibleFor(factionKey))
				continue;

			Widget row = GetGame().GetWorkspace().CreateWidgets(m_sSelectorLayout, wList);
			if (!row)
				continue;

			LL_MissionDescriptionSelectorUI rowHandler = LL_MissionDescriptionSelectorUI.Cast(row.FindHandler(LL_MissionDescriptionSelectorUI));
			if (rowHandler)
				rowHandler.Init(description, m_Panel);
		}

		// Trigger objectives, shown to everyone.
		if (LL_TriggerComponent.HasObjectives())
		{
			Widget objRow = GetGame().GetWorkspace().CreateWidgets(m_sSelectorLayout, wList);
			if (objRow)
			{
				LL_MissionDescriptionSelectorUI objHandler = LL_MissionDescriptionSelectorUI.Cast(objRow.FindHandler(LL_MissionDescriptionSelectorUI));
				if (objHandler)
					objHandler.InitObjectives(m_Panel, "#LL-Briefing_MissionConditions");
			}
		}

		// The own faction's roster; a player with no faction has no side to show.
		if (factionKey != "")
		{
			Widget rosterRow = GetGame().GetWorkspace().CreateWidgets(m_sSelectorLayout, wList);
			if (rosterRow)
			{
				LL_MissionDescriptionSelectorUI rosterHandler = LL_MissionDescriptionSelectorUI.Cast(rosterRow.FindHandler(LL_MissionDescriptionSelectorUI));
				if (rosterHandler)
					rosterHandler.InitRoster(m_Panel, "#LL-Briefing_SquadRoster");
			}
		}

		// Relevant to every player regardless of faction.
		Widget commsRow = GetGame().GetWorkspace().CreateWidgets(m_sSelectorLayout, wList);
		if (commsRow)
		{
			LL_MissionDescriptionSelectorUI commsHandler = LL_MissionDescriptionSelectorUI.Cast(commsRow.FindHandler(LL_MissionDescriptionSelectorUI));
			if (commsHandler)
				commsHandler.InitCommsGuide(m_Panel, "#LL-Briefing_CommsGuide");
		}
	}
}

class LL_MissionDescriptionSelectorUI : SCR_ButtonBaseComponent
{
	protected LL_MissionDescription m_Description;
	protected LL_MissionDescriptionPanel m_Panel;

	protected bool m_bRoster;
	protected bool m_bObjectives;
	protected bool m_bCommsGuide;

	void Init(LL_MissionDescription description, LL_MissionDescriptionPanel panel)
	{
		m_Description = description;
		m_Panel = panel;

		RichTextWidget wName = RichTextWidget.Cast(GetRootWidget().FindAnyWidget("DescriptionName"));
		if (wName)
			wName.SetText(description.GetTitle());

		m_OnClicked.Insert(OnRowClicked);
	}

	void InitRoster(LL_MissionDescriptionPanel panel, string title)
	{
		m_Panel = panel;
		m_bRoster = true;

		RichTextWidget wName = RichTextWidget.Cast(GetRootWidget().FindAnyWidget("DescriptionName"));
		if (wName)
			wName.SetText(title);

		m_OnClicked.Insert(OnRowClicked);
	}

	void InitObjectives(LL_MissionDescriptionPanel panel, string title)
	{
		m_Panel = panel;
		m_bObjectives = true;

		RichTextWidget wName = RichTextWidget.Cast(GetRootWidget().FindAnyWidget("DescriptionName"));
		if (wName)
			wName.SetText(title);

		m_OnClicked.Insert(OnRowClicked);
	}

	void InitCommsGuide(LL_MissionDescriptionPanel panel, string title)
	{
		m_Panel = panel;
		m_bCommsGuide = true;

		RichTextWidget wName = RichTextWidget.Cast(GetRootWidget().FindAnyWidget("DescriptionName"));
		if (wName)
			wName.SetText(title);

		m_OnClicked.Insert(OnRowClicked);
	}

	protected void OnRowClicked()
	{
		if (!m_Panel)
			return;

		if (m_bObjectives)
			m_Panel.ShowObjectives();
		else if (m_bRoster)
			m_Panel.ShowRoster();
		else if (m_bCommsGuide)
			m_Panel.ShowCommsGuide();
		else
			m_Panel.ShowDescription(m_Description);
	}
}

// Content handler; custom description layouts must carry this handler or a subclass.
class LL_MissionDescriptionContentUI : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected LL_MissionDescription m_Description;

	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
	}

	void SetDescription(LL_MissionDescription description)
	{
		m_Description = description;
		Render();
	}

	protected void Render()
	{
		if (m_Description)
			LL_MissionDescriptionPanel.RenderRich(m_wRoot, m_Description.GetTextData());
	}
}