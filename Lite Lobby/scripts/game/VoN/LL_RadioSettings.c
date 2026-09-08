// Per-radio listening preferences (local, never replicated) and the audio-variable
// bridge that makes them audible. The engine has no API to pan or attenuate VoN
// playback; the voice audio projects route the radio path through a bus whose gains
// come from the EarRouting variable, and VON_RADIO's volume port reads ChannelVolume.
// The variables are global and sampled at play time, so they are written on the
// listener in OnReceive and on the sender at key-up and unkey (you never receive your
// own transmission). Keyed by transceiver instance; a fresh body resets them.

//! Wired into the audio graph's routing intervals: do not renumber.
enum LL_ERadioEar
{
	CENTER = 0,
	RIGHT = 1,
	LEFT = 2
}

class LL_RadioSettings
{
	protected static const string SOUNDS_PROJECT = "{69F1A2B3C4D50012}Sounds/VON/ll_radio_sounds.acp";
	protected static const string ROUTING_VARIABLES = "{69F1A2B3C4D50010}Sounds/VON/ll_radio_routing.conf";

	// Key-up only; the vanilla roger beep is the end-of-transmission sound.
	protected static const string EVENT_SR_PTT_START = "LL_SR_PTT_START";
	protected static const string EVENT_LR_PTT_START = "LL_LR_PTT_START";

	protected static const float VOLUME_STEP = 0.1;

	// Equal loudness steps need an exponential curve: 50% on the dial = 25% gain.
	protected static const float VOLUME_CURVE_EXPONENT = 2.0;

	// The engine culls sources below the audibility threshold, and the VoN stream is a
	// persistent source that never resumes once culled.
	protected static const float VOLUME_GAIN_FLOOR = 0.02;

	protected static ref LL_RadioSettings s_Instance;

	protected ref map<BaseTransceiver, LL_ERadioEar> m_mEarByTransceiver = new map<BaseTransceiver, LL_ERadioEar>();
	protected ref map<BaseTransceiver, float> m_mVolumeByTransceiver = new map<BaseTransceiver, float>();

	protected bool m_bSoundsInitialized;

	static LL_RadioSettings GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LL_RadioSettings();

		return s_Instance;
	}

	LL_ERadioEar GetEar(BaseTransceiver transceiver)
	{
		LL_ERadioEar ear;
		if (transceiver && m_mEarByTransceiver.Find(transceiver, ear))
			return ear;

		return LL_ERadioEar.CENTER;
	}

	LL_ERadioEar CycleEar(BaseTransceiver transceiver)
	{
		if (!transceiver)
			return LL_ERadioEar.CENTER;

		LL_ERadioEar next;
		switch (GetEar(transceiver))
		{
			case LL_ERadioEar.CENTER: next = LL_ERadioEar.LEFT; break;
			case LL_ERadioEar.LEFT: next = LL_ERadioEar.RIGHT; break;
			default: next = LL_ERadioEar.CENTER; break;
		}

		m_mEarByTransceiver.Set(transceiver, next);
		return next;
	}

	float GetVolume(BaseTransceiver transceiver)
	{
		float volume;
		if (transceiver && m_mVolumeByTransceiver.Find(transceiver, volume))
			return volume;

		return 1.0;
	}

	float AdjustVolume(BaseTransceiver transceiver, int direction)
	{
		if (!transceiver)
			return 1.0;

		float volume = Math.Clamp(GetVolume(transceiver) + direction * VOLUME_STEP, VOLUME_STEP, 1.0);
		m_mVolumeByTransceiver.Set(transceiver, volume);
		return volume;
	}

	int GetVolumePercent(BaseTransceiver transceiver)
	{
		return Math.Round(GetVolume(transceiver) * 100);
	}

	//! Written just before anything on the radio path plays, the vanilla roger beep
	//! included. EarRouting carries the LL_ERadioEar value; ChannelVolume drives the
	//! VON_RADIO and SOUND_ROGER_BEEP volume ports.
	void ApplyAudioVariables(BaseTransceiver transceiver)
	{
		float earRouting = GetEar(transceiver);
		float channelGain = Math.Max(VOLUME_GAIN_FLOOR, Math.Pow(GetVolume(transceiver), VOLUME_CURVE_EXPONENT));

		AudioSystem.SetVariableByName("EarRouting", earRouting, ROUTING_VARIABLES);
		AudioSystem.SetVariableByName("ChannelVolume", channelGain, ROUTING_VARIABLES);
	}

	//! PlayEvent on a not-yet-initialised project swallows the event.
	void PreloadSounds()
	{
		if (m_bSoundsInitialized)
			return;

		m_bSoundsInitialized = true;
		AudioSystem.PlayEventInitialize(SOUNDS_PROJECT);
	}

	//! Key-up beep in the routed ear. Panning is baked into _L/_R wav variants; audio-
	//! graph routing for it proved undebuggable.
	void PlayPttSound(BaseTransceiver transceiver)
	{
		if (!transceiver)
			return;

		PreloadSounds();

		string eventName;
		if (IsLongRange(transceiver))
			eventName = EVENT_LR_PTT_START;
		else
			eventName = EVENT_SR_PTT_START;

		switch (GetEar(transceiver))
		{
			case LL_ERadioEar.LEFT: eventName = eventName + "_L"; break;
			case LL_ERadioEar.RIGHT: eventName = eventName + "_R"; break;
		}

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		AudioSystem.PlayEvent(SOUNDS_PROJECT, eventName, mat);
	}

	// UI clicks, not ear-routed. Project events rather than raw PlaySound: only sounds
	// through the FinalMix VON input obey the player's audio settings.

	protected static const string EVENT_CYCLE = "LL_RADIO_CYCLE";
	protected static const string EVENT_POWER_ON = "LL_RADIO_ON";
	protected static const string EVENT_POWER_OFF = "LL_RADIO_OFF";

	static void PlayCycleSound()
	{
		GetInstance().PlayUiEvent(EVENT_CYCLE);
	}

	static void PlayPowerSound(bool poweredOn)
	{
		if (poweredOn)
			GetInstance().PlayUiEvent(EVENT_POWER_ON);
		else
			GetInstance().PlayUiEvent(EVENT_POWER_OFF);
	}

	protected void PlayUiEvent(string eventName)
	{
		PreloadSounds();

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		AudioSystem.PlayEvent(SOUNDS_PROJECT, eventName, mat);
	}

	//! Long range = backpack radio, same rule the radial menu uses for its entries.
	static bool IsLongRange(BaseTransceiver transceiver)
	{
		if (!transceiver)
			return false;

		BaseRadioComponent radio = transceiver.GetRadio();
		if (!radio)
			return false;

		IEntity radioEntity = radio.GetOwner();
		if (!radioEntity)
			return false;

		SCR_GadgetComponent gadget = SCR_GadgetComponent.Cast(radioEntity.FindComponent(SCR_GadgetComponent));
		return gadget && gadget.GetType() == EGadgetType.RADIO_BACKPACK;
	}
}