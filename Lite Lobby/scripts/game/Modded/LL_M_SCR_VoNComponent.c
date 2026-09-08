// Engine hooks for the menu talking device: the engine accepts a VoN component that is
// not on the controlled entity only when registered through ConnectEditorToVoNSystem,
// and its editor callbacks recognise only SCR_EditorManagerEntity, so these overrides
// present a player's VoN proxy as the sender while that player is a menu speaker. Also
// tracks per-player talking state for the voice panel and applies the listener's radio
// routing in OnReceive.

modded class SCR_VoNComponent
{
	// OnReceive/OnCapture fire continuously with no "ended" event.
	protected static const int LL_TALK_TIMEOUT_MS = 400;

	protected static ref map<int, float> s_mLLTalkUntil = new map<int, float>();

	protected static ref ScriptInvoker s_OnLLTalkingChanged = new ScriptInvoker();

	static ScriptInvoker LL_GetOnTalkingChanged()
	{
		return s_OnLLTalkingChanged;
	}

	static bool LL_IsTalking(int playerId)
	{
		float until;
		if (!s_mLLTalkUntil.Find(playerId, until))
			return false;

		return until > GetGame().GetWorld().GetWorldTime();
	}

	// No controlled entity, or a corpse (dead players keep it while spectating).
	// Answerable on every machine from replicated state.
	static bool LL_IsMenuSpeaker(int playerId)
	{
		IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!controlled)
			return true;

		ChimeraCharacter character = ChimeraCharacter.Cast(controlled);
		if (character)
		{
			CharacterControllerComponent characterController = character.GetCharacterController();
			if (characterController && characterController.IsDead())
				return true;
		}

		return false;
	}

	override protected event IEntity GetEditorEntity(int playerId)
	{
		// An opened editor always wins the sender slot.
		IEntity editor = super.GetEditorEntity(playerId);
		if (editor && super.IsEntityActiveEditor(editor))
			return editor;

		if (LL_IsMenuSpeaker(playerId))
		{
			// The proxy replicates everywhere, so the sender resolves on receivers too.
			IEntity proxy = LL_VoNProxyComponent.GetProxyEntity(playerId);
			if (proxy)
				return proxy;
		}

		return editor;
	}

	override protected event vector GetEditorWorldLocation(int playerId)
	{
		IEntity editor = super.GetEditorEntity(playerId);
		if (editor && super.IsEntityActiveEditor(editor))
			return super.GetEditorWorldLocation(playerId);

		// Since 1.8 the Game Master pipeline gates delivery by the distance between the
		// positions this callback reports, on the server and on clients alike.
		LL_VoNChannelsManager vonMgr = LL_VoNChannelsManager.GetInstance();
		if (vonMgr)
			return vonMgr.GetRoomPosition(playerId);

		IEntity proxy = LL_VoNProxyComponent.GetProxyEntity(playerId);
		if (proxy)
			return proxy.GetOrigin();

		return vector.Zero;
	}

	override bool IsEntityActiveEditor(IEntity entity)
	{
		if (super.IsEntityActiveEditor(entity))
			return true;

		// A proxy counts as an active editor while its player is a menu speaker.
		if (entity)
		{
			LL_VoNProxyComponent proxyComp = LL_VoNProxyComponent.Cast(entity.FindComponent(LL_VoNProxyComponent));
			if (proxyComp)
				return LL_IsMenuSpeaker(proxyComp.GetPlayerId());
		}

		return false;
	}

	override protected event void OnReceive(int playerId, bool isSenderEditor, BaseTransceiver receiver, int frequency, float quality)
	{
		// The listener's routing for the receiving transceiver, before super plays it.
		// Null receiver = direct speech, which never touches the radio bus; writing the
		// variables for it would clobber what the roger beep reads after the last packet.
		if (receiver)
			LL_RadioSettings.GetInstance().ApplyAudioVariables(receiver);

		super.OnReceive(playerId, isSenderEditor, receiver, frequency, quality);
		LL_MarkTalking(playerId);
	}

	override protected event void OnCapture(BaseTransceiver transmitter)
	{
		super.OnCapture(transmitter);

		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
			LL_MarkTalking(pc.GetPlayerId());
	}

	protected static void LL_MarkTalking(int playerId)
	{
		float now = GetGame().GetWorld().GetWorldTime();

		float until;
		bool wasTalking = s_mLLTalkUntil.Find(playerId, until) && until > now;

		s_mLLTalkUntil.Set(playerId, now + LL_TALK_TIMEOUT_MS);

		if (!wasTalking)
		{
			s_OnLLTalkingChanged.Invoke(playerId, true);
			GetGame().GetCallqueue().CallLater(LL_CheckTalkEnd, LL_TALK_TIMEOUT_MS, false, playerId);
		}
	}

	protected static void LL_CheckTalkEnd(int playerId)
	{
		float until;
		if (!s_mLLTalkUntil.Find(playerId, until))
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (until > now)
		{
			GetGame().GetCallqueue().CallLater(LL_CheckTalkEnd, until - now + 10, false, playerId);
			return;
		}

		s_mLLTalkUntil.Remove(playerId);
		s_OnLLTalkingChanged.Invoke(playerId, false);
	}
}