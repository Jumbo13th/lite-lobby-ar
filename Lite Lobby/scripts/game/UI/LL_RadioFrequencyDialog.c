// Manual frequency input for a radio channel, opened from the VON radial. One edit box
// whose write mode is the dialog's lifetime: Enter, Escape and focus loss all end it,
// and the owner then calls Close(true), which parses, clamps to the band and applies.
// Garbage parses to 0 and is ignored, so clearing the field cancels.
class LL_RadioFrequencyDialog
{
	protected static const ResourceName LAYOUT = "{69F1A2B3C4D50020}UI/HUD/RadioFrequencyDialog.layout";

	// Write mode is requested asynchronously; without a settle window the auto-close
	// poll can read "not writing" on the next frame.
	protected static const float SETTLE_TIME_MS = 300;

	protected BaseTransceiver m_Transceiver;
	protected SCR_VONEntryRadio m_RadioEntry;

	protected Widget m_wRoot;
	protected EditBoxWidget m_wFrequencyEdit;

	protected float m_fOpenedAt;

	void Open(notnull BaseTransceiver transceiver, notnull SCR_VONEntryRadio radioEntry)
	{
		if (m_wRoot)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!m_wRoot)
			return;

		m_Transceiver = transceiver;
		m_RadioEntry = radioEntry;
		m_fOpenedAt = GetGame().GetWorld().GetWorldTime();

		m_wFrequencyEdit = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("FrequencyEdit"));
		if (!m_wFrequencyEdit)
		{
			Close(false);
			return;
		}

		TextWidget rangeText = TextWidget.Cast(m_wRoot.FindAnyWidget("RangeText"));
		if (rangeText)
			rangeText.SetText(string.Format("%1 - %2", FormatMHz(transceiver.GetMinFrequency()), FormatMHz(transceiver.GetMaxFrequency())));

		m_wFrequencyEdit.SetText(FormatMHz(transceiver.GetFrequency()));

		workspace.SetFocusedWidget(m_wFrequencyEdit);
		m_wFrequencyEdit.ActivateWriteMode();
	}

	//! True once the player finished (or abandoned) typing — the owner closes then.
	bool ShouldAutoClose()
	{
		if (!m_wRoot || !m_wFrequencyEdit)
			return false;

		if (GetGame().GetWorld().GetWorldTime() - m_fOpenedAt < SETTLE_TIME_MS)
			return false;

		return !m_wFrequencyEdit.IsInWriteMode();
	}

	void Close(bool submit)
	{
		if (submit && m_wFrequencyEdit && m_Transceiver)
			Submit(m_wFrequencyEdit.GetText());

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_wFrequencyEdit = null;
		m_Transceiver = null;
		m_RadioEntry = null;
	}

	protected void Submit(string input)
	{
		float inputMHz = input.ToFloat();
		if (inputMHz <= 0)
			return;

		// Typed input ignores the radio's tuning step (factions ship 2 MHz steps and squads
		// could never meet on 39.5); only the 0.1 MHz display precision and the band apply.
		int freqKHz = Math.Round(inputMHz * 10) * 100;

		freqKHz = Math.ClampInt(freqKHz, m_Transceiver.GetMinFrequency(), m_Transceiver.GetMaxFrequency());

		if (m_RadioEntry)
			m_RadioEntry.LL_ApplyFrequency(freqKHz);
		else
			m_Transceiver.SetFrequency(freqKHz);
	}

	//! Same rounding/format the radial entries display (39500 → "39.5").
	protected string FormatMHz(int freqKHz)
	{
		float mhz = Math.Round(freqKHz * 0.1) * 0.01;
		return mhz.ToString(3, 1);
	}
}