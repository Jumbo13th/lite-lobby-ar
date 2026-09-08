// Statistics tables shared by the stats screen and the GM panel preview. Numeric columns
// are right-anchored in the row layouts, so the rows fit any container width.
class LL_StatsTableUI
{
	protected static const ResourceName SIDE_HEADER_LAYOUT = "{8C3E5A19D2B47640}UI/Stats/StatsSideHeader.layout";
	protected static const ResourceName UNIT_ROW_LAYOUT = "{8C3E5A19D2B47650}UI/Stats/StatsUnitRow.layout";
	protected static const ResourceName SEASON_ROW_LAYOUT = "{8C3E5A19D2B47690}UI/Stats/StatsSeasonRow.layout";
	protected static const ResourceName TEXT_LINE_LAYOUT = "{8C3E5A19D2B476A0}UI/Stats/StatsTextLine.layout";

	static void Clear(notnull Widget container)
	{
		Widget child = container.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}

	static void AddNotice(notnull Widget container, string text)
	{
		TextWidget line = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(TEXT_LINE_LAYOUT, container));
		if (line)
			line.SetText(text);
	}

	static void BuildGameView(notnull Widget container, notnull LL_StatsView view, string focusUnitTag)
	{
		if (view.winner == "draw")
		{
			AddSectionHeader(container, WidgetManager.Translate("#LL-Stats_ResultDraw"), "");
		}
		else if (view.winner != "")
		{
			string winnerName = view.winner;
			foreach (LL_StatsViewSide probe : view.sides)
			{
				if (probe.faction == view.winner)
					winnerName = probe.displayName;
			}
			AddSectionHeader(container, WidgetManager.Translate("#LL-Stats_ResultWinner", winnerName), "");
		}

		foreach (LL_StatsViewSide side : view.sides)
		{
			string title = side.displayName;
			if (side.isWinner)
				title += " " + WidgetManager.Translate("#LL-Stats_WinnerSuffix");
			AddSectionHeader(container, title, FormatPoints(side.totalPoints));

			AddUnitHeaderRow(container);

			array<LL_StatsViewRow> rows = {};
			CollectSideRowsSorted(view, side.faction, rows);

			if (rows.IsEmpty())
				AddNotice(container, WidgetManager.Translate("#LL-Stats_NoUnits"));

			int place = 0;
			foreach (LL_StatsViewRow row : rows)
			{
				place++;
				AddUnitRow(container, row, place, focusUnitTag);
			}
		}

		if (!view.timeline.IsEmpty())
		{
			AddSectionHeader(container, WidgetManager.Translate("#LL-Stats_TimelineHeader"), "");
			foreach (LL_StatsViewEvent ev : view.timeline)
				AddNotice(container, FormatTime(ev.t) + "  " + FormatTimelineText(ev));
		}
	}

	protected static void AddUnitHeaderRow(notnull Widget container)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(UNIT_ROW_LAYOUT, container);
		if (!row)
			return;

		SetRowBg(row, new Color(0, 0, 0, 0));

		Color dim = ColorDim();
		SetCell(row, "Place", "#", dim);
		SetCell(row, "Name", WidgetManager.Translate("#LL-Stats_ColUnit"), dim);
		SetCell(row, "Kills", WidgetManager.Translate("#LL-Stats_ColKills"), dim);
		SetCell(row, "ZoneKills", WidgetManager.Translate("#LL-Stats_ColZoneKills"), dim);
		SetCell(row, "Teamkills", WidgetManager.Translate("#LL-Stats_ColTeamkills"), dim);
		SetCell(row, "Losses", WidgetManager.Translate("#LL-Stats_ColLosses"), dim);
		SetCell(row, "Survivors", WidgetManager.Translate("#LL-Stats_ColSurvivors"), dim);
		SetCell(row, "Objectives", WidgetManager.Translate("#LL-Stats_ColObjectives"), dim);
		SetCell(row, "Mult", "×", dim);
		SetCell(row, "Total", WidgetManager.Translate("#LL-Stats_ColTotal"), dim);
	}

	protected static void AddUnitRow(notnull Widget container, notnull LL_StatsViewRow data, int place, string focusUnitTag)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(UNIT_ROW_LAYOUT, container);
		if (!row)
			return;

		bool focused = focusUnitTag != "" && EqualsNoCase(data.unitTag, focusUnitTag);
		if (focused)
			SetRowBg(row, new Color(0.761, 0.386, 0.078, 0.22));
		else if (place % 2 == 0)
			SetRowBg(row, new Color(1, 1, 1, 0.05));

		string name = string.Format("[%1]%2", data.unitTag, data.unitName);
		if (data.isCommander)
			name += " " + WidgetManager.Translate("#LL-Stats_CommanderSuffix");

		Color text = ColorText();
		SetCell(row, "Place", place.ToString() + ".", ColorDim());
		if (focused || data.isCommander)
			SetCell(row, "Name", name, ColorAccent());
		else
			SetCell(row, "Name", name, text);
		// Kills and zone kills are shown additively; data.kills is the total.
		SetCell(row, "Kills", (data.kills - data.zoneKills).ToString(), text);
		SetCell(row, "ZoneKills", data.zoneKills.ToString(), text);
		SetCell(row, "Teamkills", data.teamkills.ToString(), text);
		SetCell(row, "Losses", data.deaths.ToString(), text);
		SetCell(row, "Survivors", data.survivors.ToString(), text);
		SetCell(row, "Objectives", FormatPoints(data.objectivePoints), text);
		SetCell(row, "Mult", "×" + data.multiplier.ToString(-1, 2), ColorDim());
		SetCell(row, "Total", FormatPoints(data.finalPoints), ColorAccent());
	}

	protected static void CollectSideRowsSorted(notnull LL_StatsView view, string faction, notnull array<LL_StatsViewRow> outRows)
	{
		foreach (LL_StatsViewRow row : view.rows)
		{
			if (row.faction != faction)
				continue;

			int at = outRows.Count();
			for (int i = 0; i < outRows.Count(); i++)
			{
				if (row.finalPoints > outRows[i].finalPoints)
				{
					at = i;
					break;
				}
			}
			outRows.InsertAt(row, at);
		}
	}

	static void BuildSeason(notnull Widget container, string seasonJson, string focusUnitTag)
	{
		if (seasonJson == "")
		{
			AddNotice(container, WidgetManager.Translate("#LL-Stats_SeasonUnavailable"));
			return;
		}

		LL_StatsSeasonResponse season = new LL_StatsSeasonResponse();
		season.ExpandFromRAW(seasonJson);

		if (season.standings.IsEmpty())
		{
			AddNotice(container, WidgetManager.Translate("#LL-Stats_SeasonUnavailable"));
			return;
		}

		string title = "";
		if (season.season)
			title = season.season.name;
		if (title != "")
			AddSectionHeader(container, WidgetManager.Translate("#LL-Stats_SeasonHeader", title), "");

		AddSeasonHeaderRow(container);

		int stripe = 0;
		foreach (LL_StatsSeasonRow entry : season.standings)
		{
			stripe++;
			AddSeasonRow(container, entry, stripe, focusUnitTag);
		}

		string siteUrl = "";
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			siteUrl = mgr.GetSiteUrl();
		if (siteUrl != "")
			AddNotice(container, WidgetManager.Translate("#LL-Stats_SeasonSiteLink", siteUrl));
	}

	protected static void AddSeasonHeaderRow(notnull Widget container)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(SEASON_ROW_LAYOUT, container);
		if (!row)
			return;

		SetRowBg(row, new Color(0, 0, 0, 0));

		Color dim = ColorDim();
		SetCell(row, "Place", "#", dim);
		SetCell(row, "Name", WidgetManager.Translate("#LL-Stats_ColUnit"), dim);
		SetCell(row, "Rating", WidgetManager.Translate("#LL-Stats_ColRating"), dim);
		SetCell(row, "RawPoints", WidgetManager.Translate("#LL-Stats_ColRawPoints"), dim);
		SetCell(row, "Games", WidgetManager.Translate("#LL-Stats_ColGames"), dim);
		SetCell(row, "Wins", WidgetManager.Translate("#LL-Stats_ColWins"), dim);
		SetCell(row, "CommandWins", WidgetManager.Translate("#LL-Stats_ColCommandWins"), dim);
		SetCell(row, "Kills", WidgetManager.Translate("#LL-Stats_ColFrags"), dim);
		SetCell(row, "Losses", WidgetManager.Translate("#LL-Stats_ColLosses"), dim);
		SetCell(row, "Teamkills", WidgetManager.Translate("#LL-Stats_ColTeamkills"), dim);
		SetCell(row, "AvgPlayers", WidgetManager.Translate("#LL-Stats_ColAvgPlayers"), dim);
	}

	protected static void AddSeasonRow(notnull Widget container, notnull LL_StatsSeasonRow data, int stripe, string focusUnitTag)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(SEASON_ROW_LAYOUT, container);
		if (!row)
			return;

		bool focused = focusUnitTag != "" && EqualsNoCase(data.tag, focusUnitTag);
		if (focused)
			SetRowBg(row, new Color(0.761, 0.386, 0.078, 0.22));
		else if (stripe % 2 == 0)
			SetRowBg(row, new Color(1, 1, 1, 0.05));

		Color text = ColorText();
		SetCell(row, "Place", data.rank.ToString() + ".", ColorDim());
		if (focused)
			SetCell(row, "Name", string.Format("[%1]%2", data.tag, data.name), ColorAccent());
		else
			SetCell(row, "Name", string.Format("[%1]%2", data.tag, data.name), text);
		SetCell(row, "Rating", FormatPoints(data.score), ColorAccent());
		SetCell(row, "RawPoints", FormatPoints(data.rawPoints), text);
		SetCell(row, "Games", data.games.ToString(), text);
		SetCell(row, "Wins", data.wins.ToString(), text);
		SetCell(row, "CommandWins", data.commandWins.ToString(), text);
		SetCell(row, "Kills", data.kills.ToString(), text);
		SetCell(row, "Losses", data.deaths.ToString(), text);
		SetCell(row, "Teamkills", data.teamkills.ToString(), text);
		SetCell(row, "AvgPlayers", data.avgPlayers.ToString(-1, 1), text);
	}

	protected static void AddSectionHeader(notnull Widget container, string title, string value)
	{
		Widget header = GetGame().GetWorkspace().CreateWidgets(SIDE_HEADER_LAYOUT, container);
		if (!header)
			return;

		TextWidget titleWidget = TextWidget.Cast(header.FindAnyWidget("HeaderTitle"));
		if (titleWidget)
			titleWidget.SetText(title);

		TextWidget valueWidget = TextWidget.Cast(header.FindAnyWidget("HeaderValue"));
		if (valueWidget)
			valueWidget.SetText(value);
	}

	protected static void SetCell(notnull Widget row, string name, string value, notnull Color color)
	{
		TextWidget cell = TextWidget.Cast(row.FindAnyWidget(name));
		if (!cell)
			return;

		cell.SetText(value);
		cell.SetColor(color);
	}

	protected static void SetRowBg(notnull Widget row, notnull Color color)
	{
		ImageWidget bg = ImageWidget.Cast(row.FindAnyWidget("RowBg"));
		if (bg)
			bg.SetColor(color);
	}

	protected static Color ColorText()
	{
		return new Color(0.85, 0.85, 0.85, 1);
	}

	protected static Color ColorDim()
	{
		return new Color(0.55, 0.55, 0.55, 1);
	}

	protected static Color ColorAccent()
	{
		return new Color(0.761, 0.392, 0.078, 1);
	}

	static string FormatPoints(float points)
	{
		return points.ToString(-1, 1);
	}

	protected static string FormatTime(float seconds)
	{
		int total = seconds;
		int mins = total / 60;
		int secs = total - mins * 60;
		return string.Format("%1:%2", mins.ToString(), secs.ToString(2));
	}

	protected static string FormatTimelineText(notnull LL_StatsViewEvent ev)
	{
		if (ev.type == LL_StatsEventType.CAPTURE)
			return WidgetManager.Translate("#LL-Stats_EventCapture", ev.text);
		if (ev.type == LL_StatsEventType.DEFENSE)
			return WidgetManager.Translate("#LL-Stats_EventDefense", ev.text);
		if (ev.type == LL_StatsEventType.KEY_TARGET)
			return WidgetManager.Translate("#LL-Stats_EventKeyTarget", ev.text);

		// The faction name is itself a key; the engine does not translate one embedded in another.
		if (ev.type == LL_StatsEventType.ZONE_FLIP)
		{
			string zone = WidgetManager.Translate(ev.text);
			if (ev.faction == "")
				return WidgetManager.Translate("#LL-Stats_EventZoneNeutral", zone);

			string faction = WidgetManager.Translate(LL_TriggerComponent.FactionName(ev.faction));
			return WidgetManager.Translate("#LL-Stats_EventZoneFlip", zone, faction);
		}

		return WidgetManager.Translate(ev.text);
	}

	protected static bool EqualsNoCase(string a, string b)
	{
		a.ToLower();
		b.ToLower();
		return a == b;
	}
}