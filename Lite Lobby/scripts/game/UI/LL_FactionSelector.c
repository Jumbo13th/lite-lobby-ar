// Faction tab in the lobby sidebar: name, flag, colour and player count.

class LL_FactionSelector : SCR_ButtonBaseComponent
{
	protected ImageWidget m_wFactionFlag;
	protected TextWidget m_wFactionName;
	protected ImageWidget m_wFactionColor;
	protected TextWidget m_wFactionCounter;
	protected ImageWidget m_wLockImage;

	protected string m_sFactionKey;
	protected SCR_Faction m_Faction;
	protected LL_CoopLobby m_CoopLobby;
	protected bool m_bSelected;

	void Init(SCR_Faction faction, string factionKey, LL_CoopLobby lobby)
	{
		m_Faction = faction;
		m_sFactionKey = factionKey;
		m_CoopLobby = lobby;

		Widget root = GetRootWidget();
		if (!root)
			return;

		m_wFactionFlag = ImageWidget.Cast(root.FindAnyWidget("FactionFlag"));
		m_wFactionName = TextWidget.Cast(root.FindAnyWidget("FactionName"));
		m_wFactionColor = ImageWidget.Cast(root.FindAnyWidget("FactionColor"));
		m_wFactionCounter = TextWidget.Cast(root.FindAnyWidget("FactionCounter"));
		m_wLockImage = ImageWidget.Cast(root.FindAnyWidget("LockImage"));

		if (m_Faction)
		{
			UIInfo uiInfo = m_Faction.GetUIInfo();
			if (uiInfo)
			{
				if (m_wFactionName)
					m_wFactionName.SetText(uiInfo.GetName());

				if (m_wFactionFlag && uiInfo.GetIconPath() != "")
					m_wFactionFlag.LoadImageTexture(0, uiInfo.GetIconPath());
			}

			if (m_wFactionColor)
				m_wFactionColor.SetColor(m_Faction.GetFactionColor());
		}
		else
		{
			if (m_wFactionName)
				m_wFactionName.SetText(factionKey);
		}

		if (m_wLockImage)
			m_wLockImage.SetVisible(false);

		m_OnClicked.Insert(OnClicked);
	}

	protected void OnClicked()
	{
		if (m_CoopLobby)
			m_CoopLobby.SwitchCurrentFaction(m_sFactionKey);
	}

	void SetCounts(int occupied, int total, int locked)
	{
		if (m_wFactionCounter)
		{
			int available = total - locked;
			m_wFactionCounter.SetTextFormat("%1 / %2", occupied, available);
		}

		if (m_wLockImage)
			m_wLockImage.SetVisible(locked > 0);
	}

	void SetSelected(bool selected)
	{
		m_bSelected = selected;
		Widget root = GetRootWidget();
		if (root)
		{
			SCR_ButtonBaseComponent btnComp = SCR_ButtonBaseComponent.Cast(root.FindHandler(SCR_ButtonBaseComponent));
			if (btnComp)
				btnComp.SetToggled(selected);
		}
	}

	string GetFactionKey()
	{
		return m_sFactionKey;
	}
}