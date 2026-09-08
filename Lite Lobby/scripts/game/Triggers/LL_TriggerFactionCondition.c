// One comparison in a zone trigger's condition list: the faction's headcount inside
// the zone against another faction's headcount times a multiplier, or a fixed number.
// "USSR >= US * 2" = Faction USSR, GREATER_EQUAL, VersusFaction US, multiplier 2.

enum LL_ETriggerCompareOp
{
	EQUAL,
	LESS,
	GREATER,
	LESS_EQUAL,
	GREATER_EQUAL,
}

[BaseContainerProps()]
class LL_TriggerFactionCondition
{
	[Attribute("USSR", UIWidgets.EditBox, "LEFT side: the faction whose living players inside the zone are counted. Type the faction key, e.g. US, USSR, FIA. In 'USSR >= US x 2' this is USSR.", category: "Lite Lobby")]
	FactionKey m_sFaction;

	[Attribute("4", UIWidgets.ComboBox, "How the faction's count is compared to the right (versus) side. e.g. GREATER_EQUAL means 'faction count >= versus side'.", "", ParamEnumArray.FromEnum(LL_ETriggerCompareOp), category: "Lite Lobby")]
	LL_ETriggerCompareOp m_eOperator;

	[Attribute("", UIWidgets.EditBox, "RIGHT side, option A: compare against THIS faction's count (x Versus Multiplier). SET this and Versus Number is ignored. LEAVE EMPTY to use Versus Number instead. In 'USSR >= US x 2' this is US.", category: "Lite Lobby")]
	FactionKey m_sVersusFaction;

	[Attribute("1", UIWidgets.EditBox, "Scales the Versus Faction's count. Used ONLY when Versus Faction is set; ignored when it is empty. The '2' in 'USSR >= US x 2'.", category: "Lite Lobby")]
	float m_fVersusMultiplier;

	[Attribute("0", UIWidgets.EditBox, "RIGHT side, option B: a fixed number to compare against. Used ONLY when Versus Faction is EMPTY; ignored when it is set. The '5' in 'USSR >= 5'.", category: "Lite Lobby")]
	int m_iVersusNumber;

	bool Evaluate(map<string, int> counts)
	{
		int factionCount = 0;
		counts.Find(m_sFaction, factionCount);

		float versus;
		if (m_sVersusFaction != "")
		{
			int versusCount = 0;
			counts.Find(m_sVersusFaction, versusCount);
			versus = versusCount * m_fVersusMultiplier;
		}
		else
		{
			versus = m_iVersusNumber;
		}

		switch (m_eOperator)
		{
			case LL_ETriggerCompareOp.EQUAL:			return factionCount == versus;
			case LL_ETriggerCompareOp.LESS:				return factionCount <  versus;
			case LL_ETriggerCompareOp.GREATER:			return factionCount >  versus;
			case LL_ETriggerCompareOp.LESS_EQUAL:		return factionCount <= versus;
			case LL_ETriggerCompareOp.GREATER_EQUAL:	return factionCount >= versus;
		}
		return false;
	}

	// Plain-language form for the briefing.
	string Describe()
	{
		string faction = LL_TriggerComponent.FactionName(m_sFaction);

		// Whole-sentence keys with the operator and multiplier phrases translated separately.
		if (m_sVersusFaction != "")
		{
			string other = LL_TriggerComponent.FactionName(m_sVersusFaction);
			return WidgetManager.Translate("#LL-TriggerCond_VersusFaction", faction, ComparativeOp(), MultiplierPhrase(m_fVersusMultiplier), other);
		}

		if (m_iVersusNumber == 1)
			return WidgetManager.Translate("#LL-TriggerCond_VersusNumberOne", faction, ComparativeOp(), m_iVersusNumber.ToString());
		return WidgetManager.Translate("#LL-TriggerCond_VersusNumber", faction, ComparativeOp(), m_iVersusNumber.ToString());
	}

	protected string ComparativeOp()
	{
		switch (m_eOperator)
		{
			case LL_ETriggerCompareOp.EQUAL:			return WidgetManager.Translate("#LL-TriggerCond_OpExactly");
			case LL_ETriggerCompareOp.LESS:				return WidgetManager.Translate("#LL-TriggerCond_OpFewer");
			case LL_ETriggerCompareOp.GREATER:			return WidgetManager.Translate("#LL-TriggerCond_OpMore");
			case LL_ETriggerCompareOp.LESS_EQUAL:		return WidgetManager.Translate("#LL-TriggerCond_OpAtMost");
			case LL_ETriggerCompareOp.GREATER_EQUAL:	return WidgetManager.Translate("#LL-TriggerCond_OpAtLeast");
		}
		return "";
	}

	// Static so LL_TriggerSupremacy words its briefing the same way. x2 and x3 get their
	// own wording: Russian counts do not agree with a bare numeral.
	static string MultiplierPhrase(float mult)
	{
		if (mult == 1)
			return WidgetManager.Translate("#LL-TriggerCond_MultSame");
		if (mult == 2)
			return WidgetManager.Translate("#LL-TriggerCond_MultTwice");
		if (mult == 3)
			return WidgetManager.Translate("#LL-TriggerCond_MultThrice");

		int im = mult;
		string m = mult.ToString();
		if (im == mult)
			m = im.ToString();
		return WidgetManager.Translate("#LL-TriggerCond_MultN", m);
	}
}