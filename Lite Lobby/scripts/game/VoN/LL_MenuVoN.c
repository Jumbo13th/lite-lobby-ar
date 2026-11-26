// Client-local press-to-talk for players without a living character, through the
// player's replicated VoN proxy connected the way the Game Master editor connects its
// component (ConnectEditorToVoNSystem). Refresh is idempotent: the device is active only
// while the local player is a menu speaker and the editor is closed.

class LL_MenuVoN
{
	protected static ref LL_MenuVoN s_Instance;

	protected SCR_VoNComponent m_VoNComp;
	protected bool m_bActive;
	protected bool m_bEditorSubscribed;

	static void Refresh()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		if (!s_Instance)
			s_Instance = new LL_MenuVoN();

		s_Instance.RefreshInternal();
	}

	static bool IsActive()
	{
		return s_Instance && s_Instance.m_bActive;
	}

	protected void RefreshInternal()
	{
		PlayerController pc = GetGame().GetPlayerController();
		// On dedicated clients the controller exists before the id is assigned (ids start
		// at 1), and during slot selection no later hook fires once it arrives.
		if (!pc || pc.GetPlayerId() <= 0)
		{
			Deactivate();
			GetGame().GetCallqueue().Remove(RetryRefresh);
			GetGame().GetCallqueue().CallLater(RetryRefresh, 500, false);
			return;
		}

		TrySubscribeEditor();

		bool shouldBeActive = SCR_VoNComponent.LL_IsMenuSpeaker(pc.GetPlayerId()) && !IsEditorOpened();

		if (shouldBeActive)
			Activate(pc);
		else
			Deactivate();
	}

	// An opened editor connects its own component for the same player id; two connected
	// components fight over the sender slot.
	protected bool IsEditorOpened()
	{
		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!core)
			return false;

		SCR_EditorManagerEntity editorManager = core.GetEditorManager();
		if (!editorManager)
			return false;

		return editorManager.IsOpened();
	}

	// The local editor manager is created asynchronously after connect.
	protected void TrySubscribeEditor()
	{
		if (m_bEditorSubscribed)
			return;

		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!core)
			return;

		SCR_EditorManagerEntity editorManager = core.GetEditorManager();
		if (!editorManager)
			return;

		editorManager.GetOnOpened().Insert(OnEditorToggled);
		editorManager.GetOnClosed().Insert(OnEditorToggled);
		m_bEditorSubscribed = true;
	}

	protected void OnEditorToggled()
	{
		RefreshInternal();
	}

	protected void RetryRefresh()
	{
		RefreshInternal();
	}

	protected void Activate(notnull PlayerController pc)
	{
		if (m_bActive)
			return;

		// Detach whatever component SCR_VONController holds: its own VONDirect handlers
		// capture on that component, which after death is the corpse (transmit icon shows,
		// nobody hears) and after closing Game Master the disconnected editor component.
		// Vanilla re-sets it on possession and editor open, so this is self-healing.
		// Vanilla stops a transmission only on the component it holds; once detached nothing
		// else would, so a radio keyed at death would stay on the air.
		SCR_VONController vonController = SCR_VONController.Cast(pc.FindComponent(SCR_VONController));
		if (vonController)
		{
			SCR_VoNComponent held = vonController.GetVONComponent();
			if (held)
			{
				held.SetCapture(false);
				vonController.SetVONComponent(null);
			}
		}

		SilenceCorpse(pc.GetControlledEntity());

		IEntity proxy = LL_VoNProxyComponent.GetProxyEntity(pc.GetPlayerId());
		if (!proxy)
		{
			// The proxy may still be streaming in.
			GetGame().GetCallqueue().Remove(RetryRefresh);
			GetGame().GetCallqueue().CallLater(RetryRefresh, 500, false);
			return;
		}

		SCR_VoNComponent vonComp = SCR_VoNComponent.Cast(proxy.FindComponent(SCR_VoNComponent));
		if (!vonComp)
		{
			Print("[LL_VoN] MenuVoN: no SCR_VoNComponent on the VoN proxy prefab — menu voice disabled", LogLevel.WARNING);
			return;
		}

		// Same forced gadget init the editor manager does on open.
		SCR_RadioComponent radioGadget = SCR_RadioComponent.Cast(proxy.FindComponent(SCR_RadioComponent));
		if (radioGadget)
			radioGadget.OnPostInit(proxy);

		BaseRadioComponent radio = BaseRadioComponent.Cast(proxy.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
		{
			Print("[LL_VoN] MenuVoN: no radio transceiver on the VoN proxy prefab — menu voice disabled", LogLevel.WARNING);
			return;
		}

		m_VoNComp = vonComp;

		// Required: without this the engine never starts capture for a component that is
		// not on the controlled entity.
		m_VoNComp.ConnectEditorToVoNSystem(pc.GetPlayerId());

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.AddActionListener("VONDirect", EActionTrigger.DOWN, OnTalkDown);
		inputManager.AddActionListener("VONDirect", EActionTrigger.UP, OnTalkUp);

		m_bActive = true;
		Print("[LL_VoN] MenuVoN: activated", LogLevel.NORMAL);
	}

	// The body stays its player's controlled entity after death, which kept it on the squad
	// net two ways: capture left on one of its voice components (dying mid-transmission, seen
	// in vehicles, on a component the controller no longer holds) and powered radios that
	// keep receiving. The dead hear their voice rooms only.
	protected void SilenceCorpse(IEntity controlled)
	{
		ChimeraCharacter corpse = ChimeraCharacter.Cast(controlled);
		if (!corpse)
			return;

		CharacterControllerComponent characterController = corpse.GetCharacterController();
		if (!characterController || !characterController.IsDead())
			return;

		array<Managed> components = {};
		corpse.FindComponents(SCR_VoNComponent, components);
		foreach (Managed component : components)
		{
			SCR_VoNComponent vonComp = SCR_VoNComponent.Cast(component);
			if (vonComp)
				vonComp.SetCapture(false);
		}

		// The radio menu's own power toggle (SCR_VONEntryRadio.ToggleEntry), so the server
		// stops routing the net to this player; a looted radio comes up switched off.
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(corpse);
		if (!gadgetManager)
			return;

		array<SCR_GadgetComponent> radios = {};
		array<SCR_GadgetComponent> found = gadgetManager.GetGadgetsByType(EGadgetType.RADIO);
		if (found)
			radios.InsertAll(found);

		found = gadgetManager.GetGadgetsByType(EGadgetType.RADIO_BACKPACK);
		if (found)
			radios.InsertAll(found);

		foreach (SCR_GadgetComponent gadget : radios)
		{
			// The death handler stops this list following the inventory, so it still names
			// radios a teammate has since looted; the gadget's own carrier says who has it now
			// (GetRootParent would answer the vehicle for a seated body).
			SCR_RadioComponent radioGadget = SCR_RadioComponent.Cast(gadget);
			if (!radioGadget || radioGadget.GetCharacterOwner() != corpse)
				continue;

			BaseRadioComponent radio = radioGadget.GetRadioComponent();
			if (radio && radio.IsPowered())
				radio.SetPower(false);
		}
	}

	protected void Deactivate()
	{
		if (!m_bActive)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		inputManager.RemoveActionListener("VONDirect", EActionTrigger.DOWN, OnTalkDown);
		inputManager.RemoveActionListener("VONDirect", EActionTrigger.UP, OnTalkUp);

		if (m_VoNComp)
		{
			m_VoNComp.SetCapture(false);
			m_VoNComp.DisconnectEditorFromVoNSystem();
		}

		m_VoNComp = null;
		m_bActive = false;
		Print("[LL_VoN] MenuVoN: deactivated", LogLevel.NORMAL);
	}

	protected void OnTalkDown()
	{
		if (!m_bActive || !m_VoNComp)
			return;

		// Direct, not radio: an editor-connected component transmits only through the Game
		// Master pipeline, which ignores the transceiver; rooms are where the proxies stand.
		m_VoNComp.SetCommMethod(ECommMethod.DIRECT);
		m_VoNComp.SetTransmitRadio(null);
		m_VoNComp.SetCapture(true);
	}

	protected void OnTalkUp()
	{
		if (m_VoNComp)
			m_VoNComp.SetCapture(false);
	}
}