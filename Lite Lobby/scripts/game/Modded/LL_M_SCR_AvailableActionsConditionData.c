// A controlled character without SCR_CharacterDamageManagerComponent makes vanilla
// FetchHealthData warn every frame; skip it silently when there is nothing to fetch.
modded class SCR_AvailableActionsConditionData
{
	override protected void FetchHealthData(float timeSlice)
	{
		if (!m_Character || !SCR_CharacterDamageManagerComponent.Cast(m_Character.GetDamageManager()))
			return;

		super.FetchHealthData(timeSlice);
	}
}