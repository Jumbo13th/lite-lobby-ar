// Radio radial menu extras: while the menu is open the highlighted channel accepts ear
// routing, volume steps and a typed frequency (the dialog survives the menu closing;
// Enter applies). Squad channels are named from replicated lobby data because the stock
// lookup needs the groups manager this mode does not run. The repeated
// [BaseContainerProps()] is required: a modded config-instantiated script object that
// drops the decorator loads as "Unknown class" and the radial never opens.

[BaseContainerProps()]
modded class SCR_VONMenu
{
	protected const string LL_ACTION_EAR_CYCLE = "LL_RadioEarCycle";
	protected const string LL_ACTION_FREQUENCY = "LL_RadioFrequency";
	protected const string LL_ACTION_VOLUME_UP = "LL_RadioVolumeUp";
	protected const string LL_ACTION_VOLUME_DOWN = "LL_RadioVolumeDown";

	protected ref LL_RadioFrequencyDialog m_LLFrequencyDialog;

	override protected void OnOpenMenu(SCR_SelectionMenu menu)
	{
		super.OnOpenMenu(menu);

		InputManager inputMgr = GetGame().GetInputManager();
		inputMgr.AddActionListener(LL_ACTION_EAR_CYCLE, EActionTrigger.DOWN, LL_ActionEarCycle);
		inputMgr.AddActionListener(LL_ACTION_FREQUENCY, EActionTrigger.DOWN, LL_ActionFrequency);
		inputMgr.AddActionListener(LL_ACTION_VOLUME_UP, EActionTrigger.DOWN, LL_ActionVolumeUp);
		inputMgr.AddActionListener(LL_ACTION_VOLUME_DOWN, EActionTrigger.DOWN, LL_ActionVolumeDown);
	}

	override protected void OnCloseMenu(SCR_SelectionMenu menu)
	{
		super.OnCloseMenu(menu);

		InputManager inputMgr = GetGame().GetInputManager();
		inputMgr.RemoveActionListener(LL_ACTION_EAR_CYCLE, EActionTrigger.DOWN, LL_ActionEarCycle);
		inputMgr.RemoveActionListener(LL_ACTION_FREQUENCY, EActionTrigger.DOWN, LL_ActionFrequency);
		inputMgr.RemoveActionListener(LL_ACTION_VOLUME_UP, EActionTrigger.DOWN, LL_ActionVolumeUp);
		inputMgr.RemoveActionListener(LL_ACTION_VOLUME_DOWN, EActionTrigger.DOWN, LL_ActionVolumeDown);
	}

	// The dialog closes and applies when its edit box leaves write mode; polled here.
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		if (m_LLFrequencyDialog && m_LLFrequencyDialog.ShouldAutoClose())
			m_LLFrequencyDialog.Close(true);
	}

	protected SCR_VONEntryRadio LL_GetSelectedRadioEntry()
	{
		if (!m_RadialMenu)
			return null;

		return SCR_VONEntryRadio.Cast(m_RadialMenu.GetSelectionEntry());
	}

	// Only entry.Update, never UpdateEntries: a full rebuild resets the selection
	// indicator to rotation 0 and the highlight wedge detaches from the entry.
	protected void LL_ActionEarCycle(float value, EActionTrigger reason)
	{
		SCR_VONEntryRadio radioEntry = LL_GetSelectedRadioEntry();
		if (!radioEntry)
			return;

		LL_RadioSettings.GetInstance().CycleEar(radioEntry.GetTransceiver());
		radioEntry.Update();
	}

	protected void LL_ActionVolumeUp(float value, EActionTrigger reason)
	{
		LL_AdjustSelectedVolume(1);
	}

	protected void LL_ActionVolumeDown(float value, EActionTrigger reason)
	{
		LL_AdjustSelectedVolume(-1);
	}

	protected void LL_AdjustSelectedVolume(int direction)
	{
		SCR_VONEntryRadio radioEntry = LL_GetSelectedRadioEntry();
		if (!radioEntry)
			return;

		LL_RadioSettings.GetInstance().AdjustVolume(radioEntry.GetTransceiver(), direction);
		radioEntry.Update();
	}

	protected void LL_ActionFrequency(float value, EActionTrigger reason)
	{
		SCR_VONEntryRadio radioEntry = LL_GetSelectedRadioEntry();
		if (!radioEntry || !radioEntry.GetTransceiver())
			return;

		if (!m_LLFrequencyDialog)
			m_LLFrequencyDialog = new LL_RadioFrequencyDialog();

		m_LLFrequencyDialog.Open(radioEntry.GetTransceiver(), radioEntry);
	}

	override static string GetKnownChannel(int frequency)
	{
		Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (localFaction)
		{
			LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
			if (mgr)
			{
				string squadName = mgr.GetSquadChannelName(frequency, localFaction.GetFactionKey());
				if (squadName != "")
					return squadName;
			}
		}

		return super.GetKnownChannel(frequency);
	}
}