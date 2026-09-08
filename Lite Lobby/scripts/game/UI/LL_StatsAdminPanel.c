// The Game Master's statistics control panel (/stats): a live score preview pulled for
// this admin only, winner and commander pickers, Publish (writes the approved snapshot
// and force-opens every client's stats screen) and Hide. Picks recompute the preview
// locally; nothing reaches the server until Publish. Commander options are the website
// roster when the units endpoint answered, otherwise every unit seen in this game. A
// real menu because only menus get a mouse cursor.

modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	StatsAdminMenu,
}

class LL_StatsAdminPanel : MenuBase
{
	protected LL_LobbyPlayerComponent m_LobbyPlayer;
	protected bool m_bLockHeld;
	protected Widget m_wRoot;
	protected TextWidget m_wStateText;
	protected TextWidget m_wWinnerValue;
	protected TextWidget m_wCommanderALabel;
	protected TextWidget m_wCommanderAValue;
	protected TextWidget m_wCommanderBLabel;
	protected TextWidget m_wCommanderBValue;
	protected Widget m_wPreviewList;

	protected ref LL_StatsView m_View;
	protected ref array<string> m_aUnitTags = {};
	protected ref array<string> m_aUnitNames = {};

	// "faction<TAB>tag" lines from the server: last published, and the first-slot
	// suggestion, plus whether a picker currently shows the suggestion.
	protected string m_sCommanders;
	protected string m_sSuggested;
	protected bool m_bCommanderASuggested;
	protected bool m_bCommanderBSuggested;

	// Selections: winner 0 = none, 1 = draw, 2+i = view.sides[i].
	// Commanders: 0 = none, 1+i = unit option i.
	protected int m_iWinner;
	protected int m_iCommanderA;
	protected int m_iCommanderB;
	protected bool m_bTouched;
	protected bool m_bPublished;
	protected bool m_bRecording;

	static void Open()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.StatsAdminMenu))
			return;

		// The map screens' crosshair cursor survives menu stacking; the game mode brings
		// the stage screen back.
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.GameMapMenu);

		menuManager.OpenMenu(ChimeraMenuPreset.StatsAdminMenu);
	}

	override void OnMenuOpen()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Close();
			return;
		}

		m_wRoot = GetRootWidget();
		m_LobbyPlayer = LL_LobbyPlayerComponent.GetLocalInstance();
		if (!m_wRoot || !m_LobbyPlayer)
		{
			Print("[LL_Lobby] Stats: admin panel opened without a root widget or local player component", LogLevel.WARNING);
			Close();
			return;
		}

		m_wStateText = TextWidget.Cast(m_wRoot.FindAnyWidget("StateText"));
		m_wWinnerValue = TextWidget.Cast(m_wRoot.FindAnyWidget("WinnerValue"));
		m_wCommanderALabel = TextWidget.Cast(m_wRoot.FindAnyWidget("CommanderALabel"));
		m_wCommanderAValue = TextWidget.Cast(m_wRoot.FindAnyWidget("CommanderAValue"));
		m_wCommanderBLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("CommanderBLabel"));
		m_wCommanderBValue = TextWidget.Cast(m_wRoot.FindAnyWidget("CommanderBValue"));
		m_wPreviewList = m_wRoot.FindAnyWidget("PreviewList");

		// 'func' parameters are not supported in script methods, so no wiring helper.
		SCR_ButtonBaseComponent comp = FindButton("WinnerButton");
		if (comp)
			comp.m_OnClicked.Insert(OnWinnerDropdown);
		comp = FindButton("CommanderAButton");
		if (comp)
			comp.m_OnClicked.Insert(OnCommanderADropdown);
		comp = FindButton("CommanderBButton");
		if (comp)
			comp.m_OnClicked.Insert(OnCommanderBDropdown);
		comp = FindButton("RefreshButton");
		if (comp)
			comp.m_OnClicked.Insert(OnRefresh);
		comp = FindButton("PublishButton");
		if (comp)
			comp.m_OnClicked.Insert(OnPublish);
		comp = FindButton("HideButton");
		if (comp)
			comp.m_OnClicked.Insert(OnHideClicked);
		comp = FindButton("CloseButton");
		if (comp)
			comp.m_OnClicked.Insert(OnClose);

		// Menus do not close on Esc on their own.
		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnBackAction);

		m_LobbyPlayer.GetOnStatsPanelData().Insert(OnPanelData);
		m_LobbyPlayer.AskStatsPanelData();

		SetState(WidgetManager.Translate("#LL-Stats_PanelLoading"));
		LL_InputLock.Acquire();
		m_bLockHeld = true;
	}

	protected SCR_ButtonBaseComponent FindButton(string name)
	{
		Widget button = m_wRoot.FindAnyWidget(name);
		if (!button)
			return null;

		return SCR_ButtonBaseComponent.Cast(button.FindHandler(SCR_ButtonBaseComponent));
	}

	protected void OnClose()
	{
		Close();
	}

	protected void OnBackAction()
	{
		// Ignored while another menu has focus above this panel.
		if (!IsFocused())
			return;

		Close();
	}

	override void OnMenuClose()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnBackAction);

		if (m_LobbyPlayer)
			m_LobbyPlayer.GetOnStatsPanelData().Remove(OnPanelData);
		m_LobbyPlayer = null;
		m_wRoot = null;

		if (m_bLockHeld)
		{
			m_bLockHeld = false;
			LL_InputLock.Release();
		}

		// A CallLater on this menu would die with it (weak-ref call queue).
		LL_GameModeCoop gm = LL_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gm)
			gm.RestoreStateMenuIfBare();
	}

	protected void OnPanelData(string viewJson, string unitsJson, bool published, bool recording, string commanders, string suggestedCommanders)
	{
		m_bPublished = published;
		m_bRecording = recording;
		m_sCommanders = commanders;
		m_sSuggested = suggestedCommanders;

		m_View = LL_StatsScreen.ParseView(viewJson);

		string keepWinner = SelectedWinnerKey();
		string keepA = SelectedCommanderTag(m_iCommanderA);
		string keepB = SelectedCommanderTag(m_iCommanderB);

		m_aUnitTags.Clear();
		m_aUnitNames.Clear();
		if (unitsJson != "")
		{
			LL_StatsWebUnitsResponse units = new LL_StatsWebUnitsResponse();
			units.ExpandFromRAW(unitsJson);
			foreach (LL_StatsWebUnit unit : units.units)
			{
				if (unit.tag == "")
					continue;
				m_aUnitTags.Insert(unit.tag);
				m_aUnitNames.Insert(unit.name);
			}
		}

		if (m_bTouched)
		{
			m_iWinner = FindWinnerIndex(keepWinner);
			m_iCommanderA = FindCommanderIndex(keepA);
			m_iCommanderB = FindCommanderIndex(keepB);

			// A guess the server has since stamped is no longer a guess (the post-publish refresh).
			if (m_bCommanderASuggested && keepA != "" && EqualsNoCase(keepA, CommanderTagOfSide(0)))
				m_bCommanderASuggested = false;
			if (m_bCommanderBSuggested && keepB != "" && EqualsNoCase(keepB, CommanderTagOfSide(1)))
				m_bCommanderBSuggested = false;
		}
		else if (m_View)
		{
			// Prefill from a previous publish, else the first-slot suggestion.
			m_iWinner = FindWinnerIndex(m_View.winner);
			m_iCommanderA = FindCommanderIndex(CommanderTagOfSide(0));
			m_iCommanderB = FindCommanderIndex(CommanderTagOfSide(1));

			// Tested on the tag, not the index: a stamped unit missing from the options list
			// also lands on 0 and must stay blank.
			m_bCommanderASuggested = false;
			if (CommanderTagOfSide(0) == "")
			{
				m_iCommanderA = FindCommanderIndex(SuggestedTagOfSide(0));
				m_bCommanderASuggested = m_iCommanderA != 0;
			}

			m_bCommanderBSuggested = false;
			if (CommanderTagOfSide(1) == "")
			{
				m_iCommanderB = FindCommanderIndex(SuggestedTagOfSide(1));
				m_bCommanderBSuggested = m_iCommanderB != 0;
			}

			// One unit cannot command both sides; the guessing side gives way.
			if (m_iCommanderA != 0 && m_iCommanderA == m_iCommanderB)
			{
				if (m_bCommanderBSuggested)
				{
					m_iCommanderB = 0;
					m_bCommanderBSuggested = false;
				}
				else if (m_bCommanderASuggested)
				{
					m_iCommanderA = 0;
					m_bCommanderASuggested = false;
				}
			}
		}

		UpdateAll();
	}

	// From the server's own list, not m_View.sides: that one is derived from a computed
	// row, so a commanding unit that fielded nobody reads back as "no commander".
	protected string CommanderTagOfSide(int sideIndex)
	{
		return TagOfSide(m_sCommanders, sideIndex);
	}

	// "" when the side has no taken slot or its top holder has no unit.
	protected string SuggestedTagOfSide(int sideIndex)
	{
		return TagOfSide(m_sSuggested, sideIndex);
	}

	protected string TagOfSide(string encoded, int sideIndex)
	{
		if (encoded == "" || !m_View || sideIndex >= m_View.sides.Count())
			return "";

		array<ref LL_StatsCommander> list = {};
		LL_StatsManager.DecodeCommanders(encoded, list);
		foreach (LL_StatsCommander cmd : list)
		{
			if (cmd.faction == m_View.sides[sideIndex].faction)
				return cmd.unitTag;
		}
		return "";
	}

	protected int FindWinnerIndex(string winnerKey)
	{
		if (winnerKey == "draw")
			return 1;
		if (m_View)
		{
			for (int i = 0; i < m_View.sides.Count(); i++)
			{
				if (m_View.sides[i].faction == winnerKey)
					return 2 + i;
			}
		}
		return 0;
	}

	protected int FindCommanderIndex(string unitTag)
	{
		if (unitTag == "")
			return 0;

		string lowerTag = unitTag;
		lowerTag.ToLower();
		for (int i = 0; i < m_aUnitTags.Count(); i++)
		{
			string probe = m_aUnitTags[i];
			probe.ToLower();
			if (probe == lowerTag)
				return 1 + i;
		}
		return 0;
	}

	// LL_ContextMenu doubles as the option list at the clicked picker. Wiring is
	// duplicated per picker: 'func' parameters are not supported in script methods.

	protected void OnWinnerDropdown()
	{
		LL_ContextMenu menu = LL_ContextMenu.CreateAt(m_wRoot, WidgetManager.Translate("#LL-Stats_PanelWinner"));
		if (!menu)
			return;

		LL_ContextAction draw = menu.AddAction("", "", "#LL-Stats_ResultDrawShort", 1);
		if (draw)
			draw.GetOnActivated().Insert(OnWinnerPicked);

		if (m_View)
		{
			for (int i = 0; i < m_View.sides.Count(); i++)
			{
				LL_ContextAction side = menu.AddAction("", "", m_View.sides[i].displayName, 2 + i);
				if (side)
					side.GetOnActivated().Insert(OnWinnerPicked);
			}
		}
	}

	protected void OnWinnerPicked(LL_ContextAction action)
	{
		m_iWinner = action.GetActionData();
		m_bTouched = true;
		UpdateAll();
	}

	protected LL_ContextMenu CreateCommanderMenu(int sideIndex)
	{
		string header = "";
		if (m_View && sideIndex < m_View.sides.Count())
			header = WidgetManager.Translate("#LL-Stats_PanelCommander", m_View.sides[sideIndex].displayName);

		return LL_ContextMenu.CreateAt(m_wRoot, header);
	}

	protected void OnCommanderADropdown()
	{
		LL_ContextMenu menu = CreateCommanderMenu(0);
		if (!menu)
			return;

		LL_ContextAction none = menu.AddAction("", "", "#LL-Stats_PanelNone", 0);
		if (none)
			none.GetOnActivated().Insert(OnCommanderAPicked);

		for (int i = 0; i < m_aUnitTags.Count(); i++)
		{
			// One unit cannot command both sides.
			if (1 + i == m_iCommanderB)
				continue;

			LL_ContextAction option = menu.AddAction("", "", string.Format("[%1]%2", m_aUnitTags[i], m_aUnitNames[i]), 1 + i);
			if (option)
				option.GetOnActivated().Insert(OnCommanderAPicked);
		}
	}

	protected void OnCommanderAPicked(LL_ContextAction action)
	{
		m_iCommanderA = action.GetActionData();
		m_bCommanderASuggested = false;
		m_bTouched = true;
		UpdateAll();
	}

	protected void OnCommanderBDropdown()
	{
		LL_ContextMenu menu = CreateCommanderMenu(1);
		if (!menu)
			return;

		LL_ContextAction none = menu.AddAction("", "", "#LL-Stats_PanelNone", 0);
		if (none)
			none.GetOnActivated().Insert(OnCommanderBPicked);

		for (int i = 0; i < m_aUnitTags.Count(); i++)
		{
			if (1 + i == m_iCommanderA)
				continue;

			LL_ContextAction option = menu.AddAction("", "", string.Format("[%1]%2", m_aUnitTags[i], m_aUnitNames[i]), 1 + i);
			if (option)
				option.GetOnActivated().Insert(OnCommanderBPicked);
		}
	}

	protected void OnCommanderBPicked(LL_ContextAction action)
	{
		m_iCommanderB = action.GetActionData();
		m_bCommanderBSuggested = false;
		m_bTouched = true;
		UpdateAll();
	}

	protected string SelectedWinnerKey()
	{
		if (m_iWinner == 1)
			return "draw";
		if (m_View && m_iWinner >= 2 && m_iWinner - 2 < m_View.sides.Count())
			return m_View.sides[m_iWinner - 2].faction;
		return "";
	}

	protected string SelectedCommanderTag(int selection)
	{
		if (selection < 1 || selection > m_aUnitTags.Count())
			return "";
		return m_aUnitTags[selection - 1];
	}

	protected void OnRefresh()
	{
		if (!m_LobbyPlayer)
			return;

		m_LobbyPlayer.AskStatsWebsiteRefresh();
		SetState(WidgetManager.Translate("#LL-Stats_PanelRefreshing"));
	}

	protected void OnPublish()
	{
		if (!m_LobbyPlayer)
			return;

		if (m_iWinner == 0)
		{
			SetState(WidgetManager.Translate("#LL-Stats_PanelNeedWinner"), true);
			return;
		}

		array<ref LL_StatsCommander> commanders = {};
		AddCommanderPick(commanders, 0, m_iCommanderA);
		AddCommanderPick(commanders, 1, m_iCommanderB);

		m_LobbyPlayer.AskStatsPublish(SelectedWinnerKey(), LL_StatsManager.EncodeCommanders(commanders));
		SetState(WidgetManager.Translate("#LL-Stats_PanelPublished"));

		GetGame().GetCallqueue().CallLater(RequestRefresh, 800, false);
	}

	protected void AddCommanderPick(notnull array<ref LL_StatsCommander> commanders, int sideIndex, int selection)
	{
		string tag = SelectedCommanderTag(selection);
		if (tag == "" || !m_View || sideIndex >= m_View.sides.Count())
			return;

		LL_StatsCommander cmd = new LL_StatsCommander();
		cmd.faction = m_View.sides[sideIndex].faction;
		cmd.unitTag = tag;
		commanders.Insert(cmd);
	}

	// "OnHide" would collide with the inherited widget event of that name.
	protected void OnHideClicked()
	{
		if (!m_LobbyPlayer)
			return;

		m_LobbyPlayer.AskStatsHide();
		SetState(WidgetManager.Translate("#LL-Stats_PanelHidden"));
		GetGame().GetCallqueue().CallLater(RequestRefresh, 800, false);
	}

	protected void RequestRefresh()
	{
		if (m_LobbyPlayer)
			m_LobbyPlayer.AskStatsPanelData();
	}

	protected void UpdateAll()
	{
		if (m_wWinnerValue)
		{
			if (m_iWinner == 0)
				m_wWinnerValue.SetText(WidgetManager.Translate("#LL-Stats_PanelNone"));
			else if (m_iWinner == 1)
				m_wWinnerValue.SetText(WidgetManager.Translate("#LL-Stats_ResultDrawShort"));
			else if (m_View && m_iWinner - 2 < m_View.sides.Count())
				m_wWinnerValue.SetText(m_View.sides[m_iWinner - 2].displayName);
		}

		UpdateCommanderRow(m_wCommanderALabel, m_wCommanderAValue, 0, m_iCommanderA, m_bCommanderASuggested);
		UpdateCommanderRow(m_wCommanderBLabel, m_wCommanderBValue, 1, m_iCommanderB, m_bCommanderBSuggested);

		if (!m_bTouched)
			UpdateStateLine();

		if (m_wPreviewList)
		{
			LL_StatsTableUI.Clear(m_wPreviewList);
			if (!m_View)
			{
				LL_StatsTableUI.AddNotice(m_wPreviewList, WidgetManager.Translate("#LL-Stats_PanelNoData"));
			}
			else
			{
				ApplySelectionToView();
				LL_StatsTableUI.BuildGameView(m_wPreviewList, m_View, "");
			}
		}
	}

	protected void UpdateCommanderRow(TextWidget label, TextWidget value, int sideIndex, int selection, bool suggested)
	{
		bool hasSide = m_View && sideIndex < m_View.sides.Count();

		if (label)
		{
			if (hasSide)
				label.SetText(WidgetManager.Translate("#LL-Stats_PanelCommander", m_View.sides[sideIndex].displayName));
			else
				label.SetText("");
		}

		if (value)
		{
			if (!hasSide)
			{
				value.SetText("");
			}
			else
			{
				string tag = SelectedCommanderTag(selection);
				if (tag == "")
				{
					value.SetText(WidgetManager.Translate("#LL-Stats_PanelNone"));
				}
				else
				{
					string text = string.Format("[%1]%2", tag, m_aUnitNames[selection - 1]);
					// A guess must not read like an approved result.
					if (suggested)
						text += " " + WidgetManager.Translate("#LL-Stats_PanelCommanderGuess");
					value.SetText(text);
				}
			}
		}
	}

	protected void UpdateStateLine()
	{
		string state;
		if (m_bPublished)
			state = WidgetManager.Translate("#LL-Stats_PanelStatePublished");
		else if (m_bRecording)
			state = WidgetManager.Translate("#LL-Stats_PanelStateRecording");
		else
			state = WidgetManager.Translate("#LL-Stats_PanelStateIdle");

		SetState(state);
	}

	// The same formula the server applies at publish, so the preview is the outcome.
	protected void ApplySelectionToView()
	{
		string winnerKey = SelectedWinnerKey();
		bool winnerDeclared = winnerKey != "" && winnerKey != "draw";

		string cmdA = SelectedCommanderTag(m_iCommanderA);
		string cmdB = SelectedCommanderTag(m_iCommanderB);

		m_View.winner = winnerKey;

		foreach (LL_StatsViewRow row : m_View.rows)
		{
			string sideCommander = "";
			if (m_View.sides.Count() > 0 && row.faction == m_View.sides[0].faction)
				sideCommander = cmdA;
			else if (m_View.sides.Count() > 1 && row.faction == m_View.sides[1].faction)
				sideCommander = cmdB;

			row.isCommander = sideCommander != "" && EqualsNoCase(row.unitTag, sideCommander);
			row.isWinnerSide = winnerDeclared && row.faction == winnerKey;

			float raw = row.basePoints + row.objectivePoints;

			// Multipliers only reward positive work, and a game never goes below zero.
			row.multiplier = 1;
			if (row.isWinnerSide && raw > 0)
			{
				row.multiplier = m_View.sideWinMultiplier;
				if (row.isCommander)
					row.multiplier = m_View.commanderWinMultiplier;
			}

			row.finalPoints = Math.Max(0, raw * row.multiplier);
		}

		for (int i = 0; i < m_View.sides.Count(); i++)
		{
			LL_StatsViewSide side = m_View.sides[i];
			side.isWinner = winnerDeclared && side.faction == winnerKey;
			side.totalPoints = 0;
			if (i == 0)
				side.commanderTag = cmdA;
			else if (i == 1)
				side.commanderTag = cmdB;

			foreach (LL_StatsViewRow row : m_View.rows)
			{
				if (row.faction == side.faction)
					side.totalPoints += row.finalPoints;
			}
		}
	}

	// Grey under the title was invisible enough that a refused publish looked like a dead button.
	protected void SetState(string text, bool error = false)
	{
		if (!m_wStateText)
			return;

		m_wStateText.SetText(text);
		if (error)
			m_wStateText.SetColor(new Color(0.878, 0.376, 0.243, 1));
		else
			m_wStateText.SetColor(new Color(0.6, 0.6, 0.6, 1));
	}

	protected static bool EqualsNoCase(string a, string b)
	{
		a.ToLower();
		b.ToLower();
		return a == b;
	}
}