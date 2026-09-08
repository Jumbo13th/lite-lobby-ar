// Direct-speech voice modes: whisper (~5 m), normal (~30 m, quieter than vanilla's
// ~68 m so squads lean on their radios) and shout (~90 m). Voice range lives in the
// audio project assigned to a VoN component and there is no runtime range API, so each
// mode is its own component on the character prefab with its own project. Sender-side
// only: a receiver plays a transmission through its replica of the sending component,
// which is why the components must live on the prefab. AI hearing is unaffected.
enum LL_EVoNMode
{
	WHISPER,
	NORMAL,
	SHOUT
}

class LL_VoNModes
{
	static LL_EVoNMode GetNext(LL_EVoNMode mode)
	{
		switch (mode)
		{
			case LL_EVoNMode.NORMAL: return LL_EVoNMode.SHOUT;
			case LL_EVoNMode.SHOUT:  return LL_EVoNMode.WHISPER;
		}

		return LL_EVoNMode.NORMAL;
	}

	static typename GetComponentType(LL_EVoNMode mode)
	{
		switch (mode)
		{
			case LL_EVoNMode.WHISPER: return LL_VoNWhisperComponent;
			case LL_EVoNMode.SHOUT:   return LL_VoNShoutComponent;
		}

		return LL_VoNNormalComponent;
	}

	static string GetDisplayName(LL_EVoNMode mode)
	{
		switch (mode)
		{
			case LL_EVoNMode.WHISPER: return "#LL-VoN_ModeWhisper";
			case LL_EVoNMode.SHOUT:   return "#LL-VoN_ModeShout";
		}

		return "#LL-VoN_ModeNormal";
	}
}

class LL_VoNWhisperComponentClass : SCR_VoNComponentClass
{
}

class LL_VoNWhisperComponent : SCR_VoNComponent
{
}

class LL_VoNNormalComponentClass : SCR_VoNComponentClass
{
}

class LL_VoNNormalComponent : SCR_VoNComponent
{
}

class LL_VoNShoutComponentClass : SCR_VoNComponentClass
{
}

class LL_VoNShoutComponent : SCR_VoNComponent
{
}