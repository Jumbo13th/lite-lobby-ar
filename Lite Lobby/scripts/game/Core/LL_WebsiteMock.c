// Local stand-in for a website implementing the verification protocol, for machines
// that cannot reach one (Workbench play first of all). Enabled by the checkbox on
// LL_PlayerVerificationComponent and only where the real config is not active. Answers
// every call with the same JSON shapes, so the production parsers run unchanged. Each
// player gets a random callsign and unit, cached for the session.
class LL_WebsiteMock
{
	protected static const int LATENCY_MS = 300;
	protected static const string SEASON_NAME = "Mock Season";

	protected static ref map<int, string> s_mCallsigns = new map<int, string>();
	protected static ref map<int, int> s_mUnitIdx = new map<int, int>();

	protected static void GetUnits(notnull array<string> outTags, notnull array<string> outNames)
	{
		outTags.Insert("ALP");	outNames.Insert("Alpha Unit");
		outTags.Insert("BRV");	outNames.Insert("Bravo Unit");
		outTags.Insert("CHR");	outNames.Insert("Charlie Unit");
		outTags.Insert("DLT");	outNames.Insert("Delta Unit");
	}

	protected static void GetCallsigns(notnull array<string> outNames)
	{
		outNames.Insert("Sokol");
		outNames.Insert("Grom");
		outNames.Insert("Viper");
		outNames.Insert("Tuman");
		outNames.Insert("Kobra");
		outNames.Insert("Shram");
		outNames.Insert("Fenix");
		outNames.Insert("Uragan");
		outNames.Insert("Bizon");
		outNames.Insert("Strizh");
		outNames.Insert("Kaban");
		outNames.Insert("Vektor");
		outNames.Insert("Malysh");
		outNames.Insert("Hunter");
		outNames.Insert("Saber");
		outNames.Insert("Wolf");
	}

	static int GetLatencyMs()
	{
		return LATENCY_MS;
	}

	static string BuildPlayerResponseJson(int playerId)
	{
		array<string> tags = {};
		array<string> names = {};
		GetUnits(tags, names);

		string callsign;
		if (!s_mCallsigns.Find(playerId, callsign))
		{
			callsign = PickCallsign(playerId);
			s_mCallsigns.Set(playerId, callsign);
		}

		int idx;
		if (!s_mUnitIdx.Find(playerId, idx))
		{
			idx = Math.RandomInt(0, tags.Count());
			s_mUnitIdx.Set(playerId, idx);
		}

		string unitJson = string.Format("{\"name\":\"%1\",\"tag\":\"%2\"}",
			LL_StatsManager.JsonEscape(names[idx]), LL_StatsManager.JsonEscape(tags[idx]));

		return string.Format("{\"callsign\":\"%1\",\"unit\":%2,\"active_bans\":[],\"badges\":[]}",
			LL_StatsManager.JsonEscape(callsign), unitJson);
	}

	// A name already taken gets the playerId appended.
	protected static string PickCallsign(int playerId)
	{
		array<string> pool = {};
		GetCallsigns(pool);

		string callsign = pool[Math.RandomInt(0, pool.Count())];

		foreach (int pid, string used : s_mCallsigns)
		{
			if (used == callsign)
				return callsign + "-" + playerId.ToString();
		}

		return callsign;
	}

	static string BuildUnitsJson()
	{
		array<string> tags = {};
		array<string> names = {};
		GetUnits(tags, names);

		string json = "{\"units\":[";
		for (int i = 0; i < tags.Count(); i++)
		{
			if (i > 0)
				json += ",";
			json += string.Format("{\"name\":\"%1\",\"tag\":\"%2\"}",
				LL_StatsManager.JsonEscape(names[i]), LL_StatsManager.JsonEscape(tags[i]));
		}
		json += "]}";
		return json;
	}

	// Deterministic descending scores, enough to exercise parsing and rendering.
	static string BuildSeasonJson()
	{
		array<string> tags = {};
		array<string> names = {};
		GetUnits(tags, names);

		string json = string.Format("{\"season\":{\"name\":\"%1\"},\"standings\":[", SEASON_NAME);

		for (int i = 0; i < tags.Count(); i++)
		{
			float score = 150.5 - 21.7 * i;
			if (score < 5)
				score = 5;
			int wins = 4 - i;
			if (wins < 0)
				wins = 0;
			int commandWins = 2 - i;
			if (commandWins < 0)
				commandWins = 0;
			float rawPoints = 340.0 - 52.0 * i;
			int kills = 64 - 13 * i;
			int deaths = 38 - 4 * i;
			float avgPlayers = 9.6 - 1.3 * i;

			if (i > 0)
				json += ",";
			json += string.Format("{\"tag\":\"%1\",\"name\":\"%2\",\"rank\":%3,\"score\":%4,\"rawPoints\":%5,\"games\":6,",
				LL_StatsManager.JsonEscape(tags[i]), LL_StatsManager.JsonEscape(names[i]), i + 1, score.ToString(-1, 1), rawPoints.ToString(-1, 1));
			json += string.Format("\"wins\":%1,\"commandWins\":%2,\"kills\":%3,\"deaths\":%4,\"teamkills\":%5,\"avgPlayers\":%6}",
				wins, commandWins, kills, deaths, i, avgPlayers.ToString(-1, 1));
		}

		json += "]}";
		return json;
	}
}