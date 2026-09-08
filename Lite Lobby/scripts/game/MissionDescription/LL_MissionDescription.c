// Briefing text entity, one folder entry in the mission description panel. Placed in
// the world by the mission maker. No replication: the attributes are baked into the
// world file.

class LL_MissionDescriptionClass : GenericEntityClass
{
}

class LL_MissionDescription : GenericEntity
{
	[Attribute("", UIWidgets.EditBox, "Folder title shown in the briefing list.", category: "Lite Lobby")]
	protected string m_sTitle;

	[Attribute(defvalue: "<h>Welcome</h>This is the mission briefing. It supports <b>rich text</b>, so you can format your notes clearly.<hr/><h>Formatting</h>Use <b>bold</b> for emphasis and <color=226,167,79,255>colour</color> to highlight key details. Separate sections with a divider.<hr/><h>Map links</h>Link any named world object, e.g. <link=ObjectiveZone>the objective</link> — click it and the map flies there.<hr/><h>Get started</h>Edit this text on the LL_MissionDescription entity and replace this example with your own briefing.", uiwidget: UIWidgets.EditBoxMultiline, desc: "Briefing text (rich text). Markup: <h>Header</h>, <b>bold</b>, <color=r,g,b,a>tint</color>, <link=EntityName>label</link> (clickable — flies the map to that world object), <hr/> divider, <br/> line break. The default value showcases every feature — replace it with your own.", category: "Lite Lobby")]
	protected string m_sTextData;

	[Attribute("{CB96D7FE5AA286DE}UI/Briefing/MissionDescriptionText.layout", UIWidgets.ResourceNamePicker, "Content layout. Default renders the text above; custom layouts must carry an LL_MissionDescriptionContentUI handler.", params: "layout", category: "Lite Lobby")]
	protected ResourceName m_sDescriptionLayout;

	[Attribute("0", UIWidgets.EditBox, "Sort order in the list (lower = higher).", category: "Lite Lobby")]
	protected int m_iOrder;

	[Attribute("1", UIWidgets.CheckBox, "Visible to everyone — all factions, and players who haven't picked a slot yet. Disable to restrict to the faction list below.", category: "Lite Lobby")]
	protected bool m_bShowForAnyFaction;

	[Attribute("", UIWidgets.Auto, "Faction keys that may read this description (used when 'show for all' is off). A player with no slot has no faction to match, so they never see a restricted entry.", category: "Lite Lobby")]
	protected ref array<FactionKey> m_aVisibleForFactions;

	// A static registry needs no game-mode prefab work.
	protected static ref array<LL_MissionDescription> s_aDescriptions = {};
	// Statics survive a scenario restart and destructors do not reliably fire on world
	// teardown; the first entry to init in a new world clears the leftovers.
	protected static BaseWorld s_RegistryWorld;

	string GetTitle()
	{
		return m_sTitle;
	}

	string GetTextData()
	{
		return m_sTextData;
	}

	ResourceName GetDescriptionLayout()
	{
		return m_sDescriptionLayout;
	}

	int GetOrder()
	{
		return m_iOrder;
	}

	// Show-for-all includes players without a slot; otherwise a player with no faction
	// never sees a restricted entry.
	bool IsVisibleFor(FactionKey factionKey)
	{
		if (m_bShowForAnyFaction)
			return true;

		if (factionKey == "")
			return false;

		return m_aVisibleForFactions && m_aVisibleForFactions.Contains(factionKey);
	}

	static void GetDescriptions(notnull out array<LL_MissionDescription> descriptionsOut)
	{
		foreach (LL_MissionDescription description : s_aDescriptions)
		{
			descriptionsOut.Insert(description);
		}

		for (int i = 1; i < descriptionsOut.Count(); i++)
		{
			LL_MissionDescription current = descriptionsOut[i];
			int j = i - 1;
			while (j >= 0 && descriptionsOut[j].GetOrder() > current.GetOrder())
			{
				descriptionsOut[j + 1] = descriptionsOut[j];
				j--;
			}
			descriptionsOut[j + 1] = current;
		}
	}

	void LL_MissionDescription(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	override protected void EOnInit(IEntity owner)
	{
		if (!m_aVisibleForFactions)
			m_aVisibleForFactions = new array<FactionKey>();

		BaseWorld world = owner.GetWorld();
		if (world != s_RegistryWorld)
		{
			s_aDescriptions.Clear();
			s_RegistryWorld = world;
		}

		if (!s_aDescriptions.Contains(this))
			s_aDescriptions.Insert(this);
	}

	void ~LL_MissionDescription()
	{
		if (s_aDescriptions)
			s_aDescriptions.RemoveItem(this);
	}
}