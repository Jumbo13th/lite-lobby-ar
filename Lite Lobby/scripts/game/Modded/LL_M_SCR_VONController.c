// Voice-mode switching (whisper / normal / shout, see LL_VoNModes): Ctrl+Tab points
// this controller at the matching mode component on the living character. Local only.
// Every possession resets to NORMAL, which also fixes vanilla's order-dependent
// FindComponent pick among four VoN components. Never fights the other voice owners: the
// menu device (which detaches this controller's component) and an open Game Master editor.
modded class SCR_VONController
{
	protected const string LL_ACTION_VON_MODE_CYCLE = "LL_VoNModeCycle";

	protected LL_EVoNMode m_eLLVoNMode = LL_EVoNMode.NORMAL;
	protected bool m_bLLVoNModeHooked;

	// Hooked only on the instance whose vanilla Init completed: vanilla early-returns for
	// every controller but the first, and m_DirectSpeechEntry is created at the end of a
	// successful Init.
	override protected void Init(IEntity owner)
	{
		super.Init(owner);

		if (!m_DirectSpeechEntry)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener(LL_ACTION_VON_MODE_CYCLE, EActionTrigger.DOWN, LL_ActionVoNModeCycle);
		m_bLLVoNModeHooked = true;

		// Warm up the radio sound project before anyone keys a radio.
		LL_RadioSettings.GetInstance().PreloadSounds();
	}

	override protected void Cleanup()
	{
		if (m_bLLVoNModeHooked)
		{
			InputManager inputManager = GetGame().GetInputManager();
			if (inputManager)
				inputManager.RemoveActionListener(LL_ACTION_VON_MODE_CYCLE, EActionTrigger.DOWN, LL_ActionVoNModeCycle);

			m_bLLVoNModeHooked = false;
		}

		super.Cleanup();
	}

	// Vanilla dereferences m_DirectSpeechEntry without a null check; ResetVON reaches it
	// on a controller that skipped full Init.
	override protected void SetVONProximityToggle(bool activate)
	{
		if (!m_DirectSpeechEntry)
			return;

		super.SetVONProximityToggle(activate);
	}

	override protected void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		m_eLLVoNMode = LL_EVoNMode.NORMAL;
		LL_ApplyVoNMode();
	}

	// Vanilla re-fetches the component here too (editor manager on Game Master close).
	override bool AssignVONComponent()
	{
		bool result = super.AssignVONComponent();
		LL_ApplyVoNMode();
		return result;
	}

	// Since 1.8 vanilla SetActiveTransmit re-attaches the controlled entity's component
	// when none is set, which for a dead spectator is the corpse the menu device detached.
	override protected void SetActiveTransmit(notnull SCR_VONEntry entry)
	{
		if (!m_VONComp && LL_MenuVoN.IsActive())
			return;

		super.SetActiveTransmit(entry);
	}

	// Sender side of radio PTT: key-up sound plus the routing variables for the sender's
	// own machine. Here and not in SetActiveTransmit, which also runs on channel
	// switching; a beep must mean "you are on the air".
	override protected bool ActivateVON(notnull SCR_VONEntry entry, EVONTransmitType transmitType = EVONTransmitType.NONE)
	{
		bool activated = super.ActivateVON(entry, transmitType);

		if (activated)
		{
			SCR_VONEntryRadio radioEntry = SCR_VONEntryRadio.Cast(entry);
			if (radioEntry)
			{
				LL_RadioSettings.GetInstance().ApplyAudioVariables(radioEntry.GetTransceiver());
				LL_RadioSettings.GetInstance().PlayPttSound(radioEntry.GetTransceiver());
			}
		}

		return activated;
	}

	// Unkey sets the routing variables only: ApplyAudioVariables otherwise runs on the
	// listener, and you never receive your own transmission, so the vanilla roger beep
	// would play unpanned on the transmitting machine.
	override protected void DeactivateVON(EVONTransmitType transmitType = EVONTransmitType.NONE)
	{
		if (m_bIsActive && m_eVONType != EVONTransmitType.NONE && m_eVONType != EVONTransmitType.DIRECT
			&& (transmitType == EVONTransmitType.NONE || transmitType == m_eVONType))
		{
			SCR_VONEntryRadio radioEntry = SCR_VONEntryRadio.Cast(GetEntryByTransmitType(m_eVONType));
			if (radioEntry)
				LL_RadioSettings.GetInstance().ApplyAudioVariables(radioEntry.GetTransceiver());
		}

		super.DeactivateVON(transmitType);
	}

	override protected void ActionVONTransceiverCycle(float value, EActionTrigger reason = EActionTrigger.UP)
	{
		if (GetVONEntryCount() > 0)
			LL_RadioSettings.PlayCycleSound();

		super.ActionVONTransceiverCycle(value, reason);
	}

	protected void LL_ActionVoNModeCycle(float value, EActionTrigger reason)
	{
		LL_EVoNMode previous = m_eLLVoNMode;
		m_eLLVoNMode = LL_VoNModes.GetNext(previous);

		if (LL_ApplyVoNMode())
			LL_VoNModeHud.Show(m_eLLVoNMode);
		else
			m_eLLVoNMode = previous;
	}

	// False when voice is owned elsewhere or the character has no mode components;
	// nothing is touched then.
	protected bool LL_ApplyVoNMode()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetOwner());
		if (!pc || pc.GetPlayerId() <= 0)
			return false;

		if (m_VONComp && m_VONComp.IsLocalActiveEditor())
			return false;

		if (SCR_VoNComponent.LL_IsMenuSpeaker(pc.GetPlayerId()))
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!character)
			return false;

		SCR_VoNComponent modeComp = SCR_VoNComponent.Cast(character.FindComponent(LL_VoNModes.GetComponentType(m_eLLVoNMode)));
		if (!modeComp)
			return false;

		if (m_VONComp == modeComp)
			return true;

		// Stop any transmission still keyed on the old component before the swap.
		if (m_VONComp)
			ResetVON();

		SetVONComponent(modeComp);
		return true;
	}
}