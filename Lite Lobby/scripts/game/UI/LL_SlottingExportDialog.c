// Overlay opened by /slotexport: the lobby as website slotting JSON plus a copy button.
// Created on the workspace root so it draws above the lobby, the map and editor UI.
class LL_SlottingExportDialog
{
	protected static const ResourceName LAYOUT = "{69F1A2B3C4D50300}UI/Lobby/SlottingExportDialog.layout";

	protected static ref LL_SlottingExportDialog s_Instance;

	protected Widget m_wRoot;
	protected string m_sJson;
	protected string m_sClipboardJson;
	protected TextWidget m_wCopyLabel;

	static void Open(string json)
	{
		if (s_Instance)
			return;

		LL_SlottingExportDialog dialog = new LL_SlottingExportDialog();
		if (!dialog.Create(json))
			return;

		s_Instance = dialog;
	}

	protected bool Create(string json)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		m_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!m_wRoot)
			return false;

		m_sJson = json;

		RichTextWidget jsonText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("JsonText"));
		if (jsonText)
			jsonText.SetText(json);

		m_wCopyLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("CopyLabel"));

		SCR_ButtonBaseComponent copyButton = FindButton("CopyButton");
		if (copyButton)
			copyButton.m_OnClicked.Insert(OnCopy);

		SCR_ButtonBaseComponent closeButton = FindButton("CloseButton");
		if (closeButton)
			closeButton.m_OnClicked.Insert(OnClose);

		return true;
	}

	protected SCR_ButtonBaseComponent FindButton(string name)
	{
		Widget button = m_wRoot.FindAnyWidget(name);
		if (!button)
			return null;

		return SCR_ButtonBaseComponent.Cast(button.FindHandler(SCR_ButtonBaseComponent));
	}

	protected void OnCopy()
	{
		// The clipboard carries only plain ASCII intact; the panel keeps the original names.
		if (m_sClipboardJson == "")
			m_sClipboardJson = LL_WebsiteSlotting.EscapeNonAscii(m_sJson);

		System.ExportToClipboard(m_sClipboardJson);

		if (m_wCopyLabel)
			m_wCopyLabel.SetText("#LL-SlottingExport_Copied");
	}

	protected void OnClose()
	{
		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		if (s_Instance == this)
			s_Instance = null;
	}
}