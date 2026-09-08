// Floor under the four video settings that decide how much of other players a client
// can see: near shadows, distant shadows, grass density and contact shadows. Local only:
// the server cannot read or verify them, so this puts honest players on the same
// footing. The switch and floors live on the game mode; without one nothing is touched.
class LL_GraphicsPolicy
{
	static bool IsEnabled()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		return mode && mode.EnforceMinGraphics();
	}

	//! Returns true when something moved, so a caller displaying the settings refreshes
	//! its widgets. The modules may be passed in by callers that already hold them.
	static bool ApplyMinimums(UserSettings videoSettings = null, UserSettings pipelineSettings = null)
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		if (!mode || !mode.EnforceMinGraphics())
			return false;

		UserSettings engineSettings = GetGame().GetEngineUserSettings();
		if (!engineSettings)
			return false;

		if (!pipelineSettings)
			pipelineSettings = engineSettings.GetModule("PipelineUserSettings");

		if (!videoSettings)
			videoSettings = engineSettings.GetModule("VideoUserSettings");

		bool changed = false;

		if (RaiseTo(pipelineSettings, "ShadowQuality", mode.GetMinShadowQuality()))
			changed = true;

		if (RaiseTo(videoSettings, "DistantShadowsQuality", mode.GetMinDistantShadows()))
			changed = true;

		if (RaiseTo(engineSettings.GetModule("GrassMaterialSettings"), "Lod", mode.GetMinGrassLod()))
			changed = true;

		// Contact shadows live in the display module's "PPQuality" object.
		BaseContainer displaySettings = engineSettings.GetModule("DisplayUserSettings");
		if (displaySettings && RaiseTo(displaySettings.GetObject("PPQuality"), "SSDO", mode.GetMinContactShadows()))
			changed = true;

		// Applies to the running renderer and writes the settings file.
		if (changed)
		{
			GetGame().UserSettingsChanged();
			Print("[LL_Lobby] Graphics policy raised video settings to the mission minimum", LogLevel.NORMAL);
		}

		return changed;
	}

	// Module names and value ranges are the vanilla video options menu's.
	protected static bool RaiseTo(BaseContainer settings, string settingName, int minValue)
	{
		if (!settings)
			return false;

		int value;
		settings.Get(settingName, value);
		if (value >= minValue)
			return false;

		settings.Set(settingName, minValue);
		return true;
	}
}