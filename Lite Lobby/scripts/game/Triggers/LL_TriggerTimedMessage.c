// Shows a HUD message a fixed number of seconds after the freeze ends, for scheduled
// announcements. Timing is inherited from LL_TriggerMissionEndTimer; the briefing gets
// a line only if the designer writes one, following the same audience as the message.
// One component per announcement.

class LL_TriggerTimedMessageClass : LL_TriggerMissionEndTimerClass
{
}

class LL_TriggerTimedMessage : LL_TriggerMissionEndTimer
{
	[Attribute("", UIWidgets.EditBoxMultiline, "Optional line for the map's Objectives panel (plain text or a #LL- key). Empty = this announcement is not listed in the briefing.", category: "Lite Lobby")]
	protected string m_sBriefingText;

	override string GetObjectiveMarkup()
	{
		if (m_sBriefingText == "")
			return "";

		// Listing a one-side restriction for both sides would leak the targeting.
		if (!MessageTargetsLocalPlayer())
			return "";

		return Header(m_sTitle) + WidgetManager.Translate(m_sBriefingText);
	}
}