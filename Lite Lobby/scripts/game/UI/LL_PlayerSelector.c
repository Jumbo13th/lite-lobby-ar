// One player row in the lobby's player list.

class LL_PlayerSelector : SCR_ButtonBaseComponent
{
	protected ImageWidget m_wPlayerFactionColor;
	protected RichTextWidget m_wPlayerName;
	protected TextWidget m_wPlayerGroupName;
	protected ImageWidget m_wReadyImage;
	protected ImageWidget m_wImageCurrent;

	protected int m_iPlayerId;
	protected LL_CoopLobby m_CoopLobby;

	// Lowercased "name role squad" for the search box; the displayed name carries colour
	// markup that would match "color".
	protected string m_sSearchText;
	protected string m_sPlayerName;
	protected string m_sRoleText;

	void Init(int playerId, string playerName, LL_CoopLobby lobby)
	{
		m_iPlayerId = playerId;
		m_CoopLobby = lobby;

		Widget root = GetRootWidget();
		if (!root)
			return;

		m_wPlayerFactionColor = ImageWidget.Cast(root.FindAnyWidget("PlayerFactionColor"));
		m_wPlayerName = RichTextWidget.Cast(root.FindAnyWidget("PlayerName"));
		m_wPlayerGroupName = TextWidget.Cast(root.FindAnyWidget("PlayerGroupName"));
		m_wReadyImage = ImageWidget.Cast(root.FindAnyWidget("ReadyImage"));
		m_wImageCurrent = ImageWidget.Cast(root.FindAnyWidget("ImageCurrent"));

		// Unmanaged widgets show broken defaults.
		Widget voiceBtn = root.FindAnyWidget("VoiceHideableButton");
		if (voiceBtn)
			voiceBtn.SetVisible(false);

		// The pin feature is not implemented.
		Widget pinImage = root.FindAnyWidget("PinImage");
		if (pinImage)
			pinImage.SetVisible(false);

		Widget pinButton = root.FindAnyWidget("PinButton");
		if (pinButton)
			pinButton.SetVisible(false);

		UpdateName(playerName);

		Refresh();
	}

	// Takes the raw name so the plain name stays available for search matching.
	void UpdateName(string name)
	{
		m_sPlayerName = name;
		RebuildSearchText();

		if (m_wPlayerName)
			m_wPlayerName.SetText(LL_LobbyManager.FormatPlayerNameRich(name));
	}

	void Refresh()
	{
		if (!m_CoopLobby)
			return;

		LL_LobbyManager mgr = m_CoopLobby.GetLobbyManager();
		if (!mgr)
			return;

		LL_SlotData slot = mgr.FindSlotByPlayerId(m_iPlayerId);
		if (slot)
		{
			SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (fm && m_wPlayerFactionColor)
			{
				SCR_Faction faction = SCR_Faction.Cast(fm.GetFactionByKey(slot.m_sFactionKey));
				if (faction)
					m_wPlayerFactionColor.SetColor(faction.GetFactionColor());
			}

			// Role alone is ambiguous with several squads.
			if (slot.m_sGroupName != "")
				m_sRoleText = string.Format("%1 · %2", slot.m_sName, slot.m_sGroupName);
			else
				m_sRoleText = slot.m_sName;

			if (m_wPlayerGroupName)
				m_wPlayerGroupName.SetText(m_sRoleText);
		}
		else
		{
			m_sRoleText = "";

			if (m_wPlayerFactionColor)
				m_wPlayerFactionColor.SetColor(Color.Gray);
			if (m_wPlayerGroupName)
				m_wPlayerGroupName.SetText("");
		}

		RebuildSearchText();

		// Manager state, not the live IsPlayerConnected: the engine's view is not reliably
		// flipped at notification time and is not stored for rebuilds or JIP.
		bool disconnected = mgr.IsPlayerDisconnected(m_iPlayerId);
		bool ready = mgr.IsPlayerReady(m_iPlayerId);

		// Priority: disconnected → ready → admin → default.
		if (m_wPlayerName)
		{
			if (disconnected)
				m_wPlayerName.SetColor(Color.FromInt(0xFF2c2c2c));
			else if (ready)
				m_wPlayerName.SetColor(Color.Green);
			else if (SCR_Global.IsAdmin(m_iPlayerId))
				m_wPlayerName.SetColor(Color.FromInt(0xfff2a34b));
			else
				m_wPlayerName.SetColor(Color.White);
		}

		if (m_wReadyImage)
			m_wReadyImage.SetVisible(!disconnected && ready);

		// The admin's move-to-slot selection, which defaults to the local player.
		if (m_wImageCurrent)
			m_wImageCurrent.SetVisible(m_iPlayerId == m_CoopLobby.GetSelectedPlayer());
	}

	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	protected void RebuildSearchText()
	{
		m_sSearchText = m_sPlayerName + " " + m_sRoleText;
		m_sSearchText.ToLower();
	}

	// The filter arrives lowercased and trimmed; empty means no filter. Returns whether the
	// row is visible. A hidden child is skipped by the VerticalLayout.
	bool ApplyFilter(string filter)
	{
		bool match = filter == "" || m_sSearchText.Contains(filter);

		Widget root = GetRootWidget();
		if (root)
			root.SetVisible(match);

		return match;
	}

	// SCR_ButtonBaseComponent invokes m_OnClicked for the left button only; the right
	// button opens the admin menu.
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);

		if (button == 1)
			OpenContextMenu();

		return false;
	}

	protected void OpenContextMenu()
	{
		if (!m_CoopLobby)
			return;

		LL_LobbyManager mgr = m_CoopLobby.GetLobbyManager();
		if (!mgr)
			return;

		LL_ContextMenu menu = LL_ContextMenu.Create(m_CoopLobby, LL_LobbyManager.FormatPlayerNameRich(mgr.GetPlayerName(m_iPlayerId)));
		if (!menu)
			return;

		// Units are the public statistics entity; no admin actions against yourself.
		menu.AddUnitStatsAction(m_iPlayerId);

		if (!SCR_Global.IsAdmin() || m_iPlayerId == m_CoopLobby.GetLocalPlayerId())
			return;

		menu.AddSelectAction(m_iPlayerId);
		menu.AddKickActions(m_iPlayerId);
	}
}