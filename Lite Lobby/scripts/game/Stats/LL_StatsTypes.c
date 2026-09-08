// Statistics data model. Units (website-registered clans) are the entity; players only
// generate events and are never displayed. Keyed by identity GUID. LL_StatsSnapshot is
// the full record written to $profile:LL_GameStats/ and never sent over the network;
// LL_StatsView is the display payload broadcast at publish, with no identities. The
// website recomputes scores from the raw events; `computed` is advisory.

// $profile:LL_GameStats.json; variable names are the JSON keys. A legible 1-point scale
// so players can re-derive the score from the visible counts.
class LL_StatsConfig
{
	bool Enabled = true;
	float FragPoints = 1;
	float ZoneFragMultiplier = 2;
	float AiKillWeight = 0;
	float TeamkillPoints = -2;
	float SurvivorPoints = 1;
	float SideWinMultiplier = 1.25;
	float CommanderWinMultiplier = 1.5;
	int AutosaveSeconds = 60;
}

// Strings, not an enum: the JSON contract with the website must never renumber.
class LL_StatsEventType
{
	static const string KILL = "kill";				// enemy player kill
	static const string ZONE_KILL = "zonekill";		// enemy player kill, victim died inside a stat zone
	static const string AI_KILL = "aikill";			// enemy AI kill by a player
	static const string TEAMKILL = "teamkill";		// friendly player kill
	static const string DEATH = "death";			// player death with no player killer (AI/GM/suicide/environment)
	static const string KEY_TARGET = "keytarget";	// destroy-trigger target destroyed
	static const string GROUP_KILL = "groupkill";
	static const string SURVIVOR = "survivor";		// alive at game end
	static const string CAPTURE = "capture";		// zone objective captured (timeline)
	static const string DEFENSE = "defense";		// zone objective held to game end (timeline)
	static const string ZONE_FLIP = "zoneflip";		// contested zone changed hands (timeline; carries the new owner's faction)
	static const string TRIGGER = "trigger";		// any other mission trigger fired (timeline)
}

// unitTag is the game-side attribution; the website's mapping by guid wins.
class LL_StatsPlayer
{
	string guid;			// identity GUID, or "name:<engine name>" where the backend gives none (workbench/peer play)
	string name;
	string callsign;
	string unitTag;
	string unitName;
	string faction;
	bool participated;
}

// `points` is the game-config valuation; the website recomputes from `type`.
class LL_StatsEvent
{
	float t;
	string type;
	string actor;
	string victim;
	string source;			// stat zone name (zonekill) / stable trigger key (keytarget)
	string detail;
	string faction;			// zoneflip only: the faction that just took the zone
	float points;
	float cap;				// keytarget only: the trigger's max-per-player (website re-clamps with its own mapping)
}

// Per player, not per unit, so the website can re-bucket with its own mapping.
class LL_StatsPresence
{
	string guid;
	float seconds;
}

// Advisory; the website recomputes from presence, pool and cap.
class LL_StatsUnitAward
{
	string unitTag;
	float seconds;
	int contributors;
	float points;
}

// One zone objective: pool, presence of both sides, resolution. A contested zone is never
// sealed: captured stays false and the pool follows holderFaction on every publish.
class LL_StatsZoneObjective
{
	string name;
	string entityName;
	float pool;
	float maxPerPlayer;
	string attackerFaction;
	string defenderFaction;
	bool captured;
	bool resolved;
	bool contested;
	string holderFaction;
	string awardedFaction;
	int flips;
	string announcedType;
	ref array<ref LL_StatsPresence> presence = {};
	ref array<ref LL_StatsUnitAward> awards = {};
}

class LL_StatsCommander
{
	string faction;
	string unitTag;
}

// Keyed (unitTag, faction): a unit fielding players on both sides gets one row per side.
class LL_StatsUnitRow
{
	string unitTag;
	string unitName;
	string faction;
	int kills;
	int zoneKills;
	int aiKills;
	int teamkills;
	int deaths;
	int survivors;
	int participants;
	float objectivePoints;
	float basePoints;
	float multiplier = 1;
	float finalPoints;
	bool isCommander;
	bool isWinnerSide;
}

// One file family per server session: -live (autosave), -final (DEBRIEFING), -approved (publish).
class LL_StatsSnapshot
{
	string schema = "ll-stats/1";
	string sessionId;
	string phase;
	string missionName;
	string world;
	string startedAt;
	string endedAt;
	string savedAt;
	string winner;
	ref LL_StatsConfig config;
	ref array<string> factions = {};
	ref array<ref LL_StatsCommander> commanders = {};
	ref array<ref LL_StatsPlayer> players = {};
	ref array<ref LL_StatsEvent> events = {};
	ref array<ref LL_StatsZoneObjective> zones = {};
	ref array<ref LL_StatsUnitRow> computed = {};
}

// Display payload broadcast at publish and streamed to JIP. JsonApiStruct rather than the
// snapshot's field reflection: the reflection writes nested classes but cannot read them
// back, and clients must parse this one.
class LL_StatsViewRow : JsonApiStruct
{
	string unitTag;
	string unitName;
	string faction;
	int kills;
	int zoneKills;
	int aiKills;
	int teamkills;
	int deaths;
	int survivors;
	int participants;
	float objectivePoints;
	float basePoints;
	float multiplier = 1;
	float finalPoints;
	bool isCommander;
	bool isWinnerSide;

	void LL_StatsViewRow()
	{
		RegV("unitTag");
		RegV("unitName");
		RegV("faction");
		RegV("kills");
		RegV("zoneKills");
		RegV("aiKills");
		RegV("teamkills");
		RegV("deaths");
		RegV("survivors");
		RegV("participants");
		RegV("objectivePoints");
		RegV("basePoints");
		RegV("multiplier");
		RegV("finalPoints");
		RegV("isCommander");
		RegV("isWinnerSide");
	}
}

class LL_StatsViewSide : JsonApiStruct
{
	string faction;
	string displayName;
	bool isWinner;
	string commanderTag;
	float totalPoints;

	void LL_StatsViewSide()
	{
		RegV("faction");
		RegV("displayName");
		RegV("isWinner");
		RegV("commanderTag");
		RegV("totalPoints");
	}
}

class LL_StatsViewEvent : JsonApiStruct
{
	float t;
	string type;
	string text;
	string faction;

	void LL_StatsViewEvent()
	{
		RegV("t");
		RegV("type");
		RegV("text");
		RegV("faction");
	}
}

class LL_StatsView : JsonApiStruct
{
	string schema = "ll-stats-view/1";
	string sessionId;
	string missionName;
	string winner;
	// Echoed so the panel can preview a winner or commander choice locally.
	float sideWinMultiplier = 1.25;
	float commanderWinMultiplier = 1.5;
	ref array<ref LL_StatsViewSide> sides = {};
	ref array<ref LL_StatsViewRow> rows = {};
	ref array<ref LL_StatsViewEvent> timeline = {};

	void LL_StatsView()
	{
		RegV("schema");
		RegV("sessionId");
		RegV("missionName");
		RegV("winner");
		RegV("sideWinMultiplier");
		RegV("commanderWinMultiplier");
		RegV("sides");
		RegV("rows");
		RegV("timeline");
	}
}

// Season-endpoint response. JsonApiStruct: unregistered scalars are skipped, arrays must
// be registered.
class LL_StatsSeasonInfo : JsonApiStruct
{
	string name;

	void LL_StatsSeasonInfo()
	{
		RegV("name");
	}
}

class LL_StatsSeasonRow : JsonApiStruct
{
	string tag;
	string name;
	int rank;
	float score;
	float rawPoints;
	int games;
	int wins;
	int commandWins;
	int kills;
	int deaths;
	int teamkills;
	float avgPlayers;

	void LL_StatsSeasonRow()
	{
		RegV("tag");
		RegV("name");
		RegV("rank");
		RegV("score");
		RegV("rawPoints");
		RegV("games");
		RegV("wins");
		RegV("commandWins");
		RegV("kills");
		RegV("deaths");
		RegV("teamkills");
		RegV("avgPlayers");
	}
}

class LL_StatsSeasonResponse : JsonApiStruct
{
	ref LL_StatsSeasonInfo season = new LL_StatsSeasonInfo();
	ref array<ref LL_StatsSeasonRow> standings = {};

	void LL_StatsSeasonResponse()
	{
		RegV("season");
		RegV("standings");
	}
}