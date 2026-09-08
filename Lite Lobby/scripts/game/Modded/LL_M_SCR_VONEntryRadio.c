// Radio channel entry in the VON radial menu: show the channel's listening
// preferences, click on power toggle, and accept a typed frequency.
modded class SCR_VONEntryRadio
{
	//! Apply a frequency chosen in the input dialog. The transceiver's own getter
	//! lags behind SetFrequency (see the TODO in vanilla AdjustEntryModif), so the
	//! cached value and display text are written directly for instant UI feedback.
	void LL_ApplyFrequency(int freqKHz)
	{
		if (!m_RadioTransceiver)
			return;

		m_RadioTransceiver.SetFrequency(freqKHz);
		m_iFrequency = freqKHz;

		float fFrequency = Math.Round(m_iFrequency * 0.1) * 0.01;
		m_sText = fFrequency.ToString(3, 1) + " " + LABEL_FREQUENCY_UNITS;

		SetChannelText(SCR_VONMenu.GetKnownChannel(m_iFrequency));
		Update();
	}

	//! Append the ear-routing tag and non-default volume to the frequency line,
	//! e.g. "38.5 MHz L 80%". Plain L/R on purpose: the row already mixes latin
	//! radio jargon (CH1, MHz), and the frequency text is assembled by string
	//! concatenation where a second localization token may not resolve.
	override void Update()
	{
		super.Update();

		SCR_VONEntryComponent entryComp = SCR_VONEntryComponent.Cast(m_EntryComponent);
		if (!entryComp || !m_RadioTransceiver || !m_sFrequencyTextOverwrite.IsEmpty())
			return;

		LL_RadioSettings settings = LL_RadioSettings.GetInstance();

		string suffix;
		switch (settings.GetEar(m_RadioTransceiver))
		{
			case LL_ERadioEar.LEFT: suffix = " L"; break;
			case LL_ERadioEar.RIGHT: suffix = " R"; break;
		}

		int volumePercent = settings.GetVolumePercent(m_RadioTransceiver);
		if (volumePercent != 100)
			suffix = suffix + " " + volumePercent.ToString() + "%";

		if (!suffix.IsEmpty())
			entryComp.SetFrequencyText(m_sText + suffix);
	}

	//! Vanilla's power toggle plays its on/off UI events swapped (just-powered
	//! radios get SOUND_RADIO_TURN_OFF); replaced wholesale with the radio click
	//! set so all radio manipulation sounds share one voice.
	override void ToggleEntry()
	{
		if (!m_RadioTransceiver)
			return;

		BaseRadioComponent radio = m_RadioTransceiver.GetRadio();

		SetUsable(!radio.IsPowered());
		radio.SetPower(IsUsable());

		AdjustEntryModif(0);

		LL_RadioSettings.PlayPowerSound(IsUsable());
	}
}
