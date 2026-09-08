// Photo mode ("Arma Vision") is always off. The editor's mode list cannot be trimmed in
// config, so the mode entity is allowed to spawn and is removed the next frame, before
// the player can enter it; the removal must be deferred because the mode is still being
// created when CreateEditorMode returns.
modded class SCR_EditorManagerEntity
{
	override SCR_EditorModeEntity CreateEditorMode(EEditorMode mode, bool isInit, ResourceName prefab = "")
	{
		SCR_EditorModeEntity editorModeEntity = super.CreateEditorMode(mode, isInit, prefab);

		if (mode == EEditorMode.PHOTO && editorModeEntity)
			GetGame().GetCallqueue().Call(RemoveMode, editorModeEntity, false);

		return editorModeEntity;
	}
}