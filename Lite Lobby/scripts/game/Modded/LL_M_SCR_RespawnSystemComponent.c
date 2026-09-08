// The base game mode calls the respawn component without null checks, and disabling it
// in the prefab leaves the reference valid, so every method is a no-op: the lobby owns
// spawning and possession.

modded class SCR_RespawnSystemComponentClass : RespawnSystemComponentClass
{
}

modded class SCR_RespawnSystemComponent : RespawnSystemComponent
{
	override SCR_BaseSpawnPointRequestResultInfo GetSpawnPointRequestResultInfo(SCR_SpawnRequestComponent requestComponent, SCR_ESpawnResult response, SCR_SpawnData data) { return null; }
	static override SCR_RespawnSystemComponent GetInstance() { return null; }
	override RplComponent GetRplComponent() { return null; }
	static override MenuBase OpenRespawnMenu() { return null; }
	static override void CloseRespawnMenu() { return; }
	override void ServerSetEnableRespawn(bool enableSpawning) { return; }
	override bool IsRespawnEnabled() { return false; }
	override bool IsPauseMenuRespawnEnabled() { return false; }
	override bool IsFactionChangeAllowed() { return false; }
	override ScriptInvoker GetOnRespawnEnabledChanged() { return null; }
	override void OnPlayerRegistered_S(int playerId) { return; }
	override void OnPlayerAuditSuccess_S(int playerId) { return; }
	override void OnPlayerDisconnected_S(int playerId, KickCauseCode cause, int timeout) { return; }
	override void OnPlayerEntityCleanup_S(notnull IEntity playerEntity) { return; }
	override void OnPlayerKilled_S(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer) { return; }
	override void OnPlayerDeleted_S(int playerId) { return; }
	override void OnInit(IEntity owner) { return; }
}