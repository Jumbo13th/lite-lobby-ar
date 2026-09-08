// Game Master context action on a dead lobby slot character, player-held or bot: spawns
// a fresh body for that squad where the old one fell and hands a player into it. Gated
// to dead slots; the authority re-checks. On a dedicated client the selection is the
// editor's per-player delegate, not the character, so player delegates resolve by player
// id and real entities by RplComponent id. m_bIsServer must be set on the action entry
// in the editor mode prefab.
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class LL_RevivePlayerContextAction : SCR_SelectedEntitiesContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		// The slot lookup is the gate: props, vehicles and non-lobby characters resolve to none.
		return ResolveReviveSlot(selectedEntity) != null;
	}

	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		LL_SlotData slot = ResolveReviveSlot(selectedEntity);
		if (!slot)
			return false;

		return slot.IsDestroyed();
	}

	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		// Runs on the authority; the manager re-validates.
		LL_SlotData slot = ResolveReviveSlot(selectedEntity);
		if (!slot)
			return;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		mgr.RespawnSlotCharacter_S(slot.m_iRplId);
	}

	// Works on a dedicated client (player delegate) and a listen host alike.
	protected LL_SlotData ResolveReviveSlot(SCR_EditableEntityComponent selectedEntity)
	{
		if (!selectedEntity)
			return null;

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return null;

		// Valid on every machine and independent of whether the character is streamed in.
		SCR_EditablePlayerDelegateComponent playerDelegate = SCR_EditablePlayerDelegateComponent.Cast(selectedEntity);
		if (playerDelegate)
		{
			int playerId = playerDelegate.GetPlayerID();
			if (playerId <= 0)
				return null;

			return mgr.FindSlotByPlayerId(playerId);
		}

		// A lobby slot is keyed by its character's RplComponent id.
		IEntity owner = selectedEntity.GetOwner();
		if (!owner)
			return null;

		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!rpl)
			return null;

		int rplId = rpl.Id();
		return mgr.FindSlotByRplId(rplId);
	}
};