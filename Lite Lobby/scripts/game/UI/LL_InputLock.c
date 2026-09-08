// Freezes the local character's controls for statistics windows (refcounted) and the
// hard freeze (replicated state, passed to EnforceView). A one-shot SetDisable*Controls
// cannot work: SCR_BaseGameMode rewrites the weapon and movement flags every frame and
// SCR_PlayerController re-enables all three when the last menu closes, so the per-frame
// writer in LL_GameModeCoop.SetLocalControls consults this class. Refcounted because a
// publish force-opens the stats screen while the admin panel is still up.
class LL_InputLock
{
	protected static int s_iLocks;
	protected static IEntity s_ViewFrozenEntity;

	// Statics survive scenario restarts, and a menu open at world teardown may never see
	// OnMenuClose.
	static void ResetStatic()
	{
		s_iLocks = 0;
		s_ViewFrozenEntity = null;
	}

	static void Acquire()
	{
		s_iLocks++;
	}

	static void Release()
	{
		if (s_iLocks > 0)
			s_iLocks--;
	}

	static bool IsLocked()
	{
		return s_iLocks > 0;
	}

	// Per frame from LL_GameModeCoop.SetLocalControls: the view flag gets the same
	// treatment as vanilla's movement and weapons, including the release edge. The hard
	// freeze is passed in rather than Acquired: replicated state can change while a
	// client loads, and a missed Release would freeze that player for the session.
	static void EnforceView(bool alsoLocked = false)
	{
		IEntity target;
		if (s_iLocks > 0 || alsoLocked)
		{
			PlayerController playerController = GetGame().GetPlayerController();
			if (playerController)
				target = playerController.GetControlledEntity();
		}

		if (s_ViewFrozenEntity && s_ViewFrozenEntity != target)
			SetViewDisabled(s_ViewFrozenEntity, false);

		s_ViewFrozenEntity = target;
		if (target)
			SetViewDisabled(target, true);
	}

	protected static void SetViewDisabled(IEntity entity, bool disabled)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return;

		CharacterControllerComponent charController = character.GetCharacterController();
		if (!charController)
			return;

		charController.SetDisableViewControls(disabled);
	}
}