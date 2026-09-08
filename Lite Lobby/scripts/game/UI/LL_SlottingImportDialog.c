// Overlay opened by /slotimport: paste the website's slotting JSON, Validate previews the
// match, Apply sends the labels to the server, which re-checks admin and stage. Apply
// re-validates the current text, so an edit after Validate never applies a stale preview.
class LL_SlottingImportDialog
{
	protected static const ResourceName LAYOUT = "{69F1A2B3C4D50400}UI/Lobby/SlottingImportDialog.layout";

	protected static const int MAX_WARNING_LINES = 8;

	protected static ref LL_SlottingImportDialog s_Instance;

	protected LL_LobbyPlayerComponent m_LobbyPlayer;
	protected Widget m_wRoot;
	protected MultilineEditBoxWidget m_wJsonEdit;
	protected RichTextWidget m_wSummary;

	static void Open(notnull LL_LobbyPlayerComponent lobbyPlayer)
	{
		if (s_Instance)
			return;

		LL_SlottingImportDialog dialog = new LL_SlottingImportDialog();
		if (!dialog.Create(lobbyPlayer))
			return;

		s_Instance = dialog;
	}

	protected bool Create(LL_LobbyPlayerComponent lobbyPlayer)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		m_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!m_wRoot)
			return false;

		m_LobbyPlayer = lobbyPlayer;

		m_wJsonEdit = MultilineEditBoxWidget.Cast(m_wRoot.FindAnyWidget("JsonEdit"));
		m_wSummary = RichTextWidget.Cast(m_wRoot.FindAnyWidget("SummaryText"));

		SCR_ButtonBaseComponent button = FindButton("PasteButton");
		if (button)
			button.m_OnClicked.Insert(OnPaste);

		button = FindButton("ValidateButton");
		if (button)
			button.m_OnClicked.Insert(OnValidate);

		button = FindButton("ApplyButton");
		if (button)
			button.m_OnClicked.Insert(OnApply);

		button = FindButton("ClearButton");
		if (button)
			button.m_OnClicked.Insert(OnClear);

		button = FindButton("CancelButton");
		if (button)
			button.m_OnClicked.Insert(OnCancel);

		return true;
	}

	protected SCR_ButtonBaseComponent FindButton(string name)
	{
		Widget button = m_wRoot.FindAnyWidget(name);
		if (!button)
			return null;

		return SCR_ButtonBaseComponent.Cast(button.FindHandler(SCR_ButtonBaseComponent));
	}

	protected void OnPaste()
	{
		if (!m_wJsonEdit)
			return;

		m_wJsonEdit.SetText(CompactJson(System.ImportFromClipboard()));
		SetSummary("");
	}

	// The edit box renders pretty-printed JSON as ragged wrapped soup. Meaningful whitespace
	// only lives inside quoted strings, so stripping around structure is lossless.
	protected string CompactJson(string json)
	{
		json.Replace("\r", "");
		json.Replace("\t", " ");

		array<string> lines = {};
		json.Split("\n", lines, true);

		string compact = "";
		foreach (string line : lines)
		{
			compact += line.Trim();
		}

		return compact;
	}

	protected void OnValidate()
	{
		LL_WebsiteSlottingImportResult result = MatchCurrentText();
		if (!result)
			return;

		SetSummary(BuildSummaryText(result));
	}

	protected void OnApply()
	{
		LL_WebsiteSlottingImportResult result = MatchCurrentText();
		if (!result)
			return;

		if (!result.m_bParsed)
		{
			SetSummary(BuildSummaryText(result));
			return;
		}

		if (m_LobbyPlayer)
			m_LobbyPlayer.AskApplyWebsiteSlotting(LL_WebsiteSlotting.EncodeLabelChunks(result.m_mLabels));

		OnCancel();
	}

	protected void OnClear()
	{
		if (m_LobbyPlayer)
		{
			array<string> noChunks = {};
			m_LobbyPlayer.AskApplyWebsiteSlotting(noChunks);
		}

		OnCancel();
	}

	protected void OnCancel()
	{
		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_LobbyPlayer = null;

		if (s_Instance == this)
			s_Instance = null;
	}

	protected LL_WebsiteSlottingImportResult MatchCurrentText()
	{
		if (!m_wJsonEdit)
			return null;

		return LL_WebsiteSlotting.MatchImportJson(m_wJsonEdit.GetText());
	}

	protected string BuildSummaryText(LL_WebsiteSlottingImportResult result)
	{
		if (!result.m_bParsed)
			return WidgetManager.Translate("#LL-SlottingImport_ParseError");

		string text = WidgetManager.Translate("#LL-SlottingImport_Summary",
			result.m_iMatchedSlots.ToString(), result.m_iJsonSlots.ToString(), result.m_mLabels.Count().ToString());

		int shown = 0;
		foreach (string warning : result.m_aWarnings)
		{
			if (shown >= MAX_WARNING_LINES)
			{
				int remaining = result.m_aWarnings.Count() - shown;
				text += "\n" + WidgetManager.Translate("#LL-SlottingImport_MoreWarnings", remaining.ToString());
				break;
			}

			text += "\n! " + warning;
			shown++;
		}

		return text;
	}

	protected void SetSummary(string text)
	{
		if (m_wSummary)
			m_wSummary.SetText(text);
	}
}