// Bridge between the lobby's slot list and a website's slotting JSON (sides → squads →
// slots and assets). Export builds the template from the lobby; import parses the filled
// slotting into slotRplId → label pairs. Both run on the admin client; only the compact
// label map touches the network. JsonApiStruct binds by member name: unregistered
// scalars are skipped, every array the site sends must be registered.

class LL_WebJsonOccupant : JsonApiStruct
{
	string type;
	string label;
	string callsign;

	void LL_WebJsonOccupant()
	{
		RegV("type");
		RegV("label");
		RegV("callsign");
	}

	// "occupant": null leaves the pre-created instance untouched.
	string GetDisplayLabel()
	{
		if (type == "user")
			return callsign;
		if (type == "placeholder")
			return label;
		return "";
	}
}

class LL_WebJsonSlot : JsonApiStruct
{
	string role;
	ref LL_WebJsonOccupant occupant = new LL_WebJsonOccupant();

	void LL_WebJsonSlot()
	{
		RegV("role");
		RegV("occupant");
	}
}

class LL_WebJsonAsset : JsonApiStruct
{
	string name;

	void LL_WebJsonAsset()
	{
		RegV("name");
	}
}

class LL_WebJsonSquad : JsonApiStruct
{
	string name;
	ref array<ref LL_WebJsonSlot> slots = {};
	ref array<ref LL_WebJsonAsset> assets = {};

	void LL_WebJsonSquad()
	{
		RegV("name");
		RegV("slots");
		RegV("assets");
	}
}

class LL_WebJsonSide : JsonApiStruct
{
	string name;
	ref array<ref LL_WebJsonSquad> squads = {};

	void LL_WebJsonSide()
	{
		RegV("name");
		RegV("squads");
	}
}

class LL_WebJsonSlotting : JsonApiStruct
{
	ref array<ref LL_WebJsonSide> sides = {};

	void LL_WebJsonSlotting()
	{
		RegV("sides");
	}
}

// Slots grouped by faction and group in the manager's replicated sort order, so export
// order and index-based import matching agree on every machine.

class LL_WebsiteSquadIndex : Managed
{
	string m_sFactionKey;
	int m_iGroupId;
	string m_sNameEn;
	ref array<LL_SlotData> m_aSlots = {};
}

class LL_WebsiteSlottingImportResult : Managed
{
	bool m_bParsed;
	int m_iJsonSlots;
	int m_iMatchedSlots;
	ref map<int, string> m_mLabels = new map<int, string>();
	ref array<string> m_aWarnings = {};
}

class LL_WebsiteSlotting
{
	// Labels end up inside rich text and chunked payloads.
	static string SanitizeLabel(string label)
	{
		label.Replace("<", "");
		label.Replace(">", "");
		label.Replace("#", "");
		label.Replace("\t", " ");
		label.Replace("\n", " ");
		label.Replace("\r", " ");
		label = label.Trim();

		if (label.Length() > 32)
			label = label.Substring(0, 32);

		return label;
	}

	//! "" when the lobby has no slots yet.
	static string BuildExportJson()
	{
		array<ref LL_WebsiteSquadIndex> squads = BuildSquadIndex();
		if (squads.IsEmpty())
			return "";

		// The website is English-only: the same temporary language flip vanilla's
		// ScenarioFramework uses. Restored before returning.
		string language;
		WidgetManager.GetLanguage(language);
		bool switched = language != "" && language != "en_us";
		if (switched)
			WidgetManager.SetLanguage("en_us");

		string json = BuildExportJsonEnglish(squads);

		if (switched)
			WidgetManager.SetLanguage(language);

		return json;
	}

	protected static string BuildExportJsonEnglish(array<ref LL_WebsiteSquadIndex> squads)
	{
		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());

		string json = "{\n  \"sides\": [";

		string currentFaction = "-";
		int sideIndex = 0;
		int squadIndex = 0;
		bool firstSide = true;

		for (int i = 0; i < squads.Count(); i++)
		{
			LL_WebsiteSquadIndex squad = squads[i];

			if (squad.m_sFactionKey != currentFaction)
			{
				if (!firstSide)
					json += "\n      ]\n    },";

				currentFaction = squad.m_sFactionKey;
				sideIndex++;
				squadIndex = 0;
				firstSide = false;

				string displayName = currentFaction;
				string color = "#808080";
				if (factionManager)
				{
					Faction faction = factionManager.GetFactionByKey(currentFaction);
					if (faction)
					{
						displayName = WidgetManager.Translate(faction.GetFactionName());
						Color factionColor = faction.GetFactionColor();
						if (factionColor)
							color = ColorToHex(factionColor);
					}
				}

				json += string.Format("\n    {\n      \"id\": \"side-%1-%2\",\n      \"name\": \"%3\",\n      \"displayName\": \"%4\",\n      \"color\": \"%5\",\n      \"squads\": [",
					sideIndex, Slug(currentFaction), JsonEscape(currentFaction), JsonEscape(displayName), color);
			}

			squadIndex++;
			if (squadIndex > 1)
				json += ",";

			string squadName = WidgetManager.Translate(squad.m_sNameEn);
			string squadId = string.Format("side-%1-squad-%2-%3", sideIndex, squadIndex, Slug(squadName));

			json += string.Format("\n        {\n          \"id\": \"%1\",\n          \"name\": \"%2\",\n          \"slots\": [", squadId, JsonEscape(squadName));

			for (int s = 0; s < squad.m_aSlots.Count(); s++)
			{
				if (s > 0)
					json += ",";

				string role = WidgetManager.Translate(squad.m_aSlots[s].m_sName);
				json += string.Format("\n            {\n              \"id\": \"%1-slot-%2-%3\",\n              \"role\": \"%4\",\n              \"access\": \"unit\",\n              \"occupant\": null\n            }",
					squadId, s + 1, Slug(role), JsonEscape(role));
			}

			json += "\n          ]";

			array<ref LL_VehicleData> vehicles = mgr.GetVehiclesForGroup(squad.m_iGroupId);
			if (!vehicles.IsEmpty())
			{
				json += ",\n          \"assets\": [";
				for (int v = 0; v < vehicles.Count(); v++)
				{
					if (v > 0)
						json += ",";

					string vehicleName = WidgetManager.Translate(vehicles[v].m_sName);
					json += string.Format("\n            {\n              \"id\": \"%1-asset-%2-%3\",\n              \"name\": \"%4\"\n            }",
						squadId, v + 1, Slug(vehicleName), JsonEscape(vehicleName));
				}
				json += "\n          ]";
			}

			json += "\n        }";
		}

		json += "\n      ]\n    }\n  ]\n}";
		return json;
	}

	//! Warnings are already-translated display lines.
	static LL_WebsiteSlottingImportResult MatchImportJson(string json)
	{
		LL_WebsiteSlottingImportResult result = new LL_WebsiteSlottingImportResult();

		LL_WebJsonSlotting root = new LL_WebJsonSlotting();
		root.ExpandFromRAW(json);

		// ExpandFromRAW has no error return; an unparseable paste leaves the structure empty.
		if (root.sides.IsEmpty())
			return result;

		result.m_bParsed = true;

		array<ref LL_WebsiteSquadIndex> gameSquads = BuildSquadIndex();
		// Same forced-English window as the export.
		string language;
		WidgetManager.GetLanguage(language);
		bool switched = language != "" && language != "en_us";
		if (switched)
			WidgetManager.SetLanguage("en_us");

		MatchEnglish(root, gameSquads, result);

		if (switched)
			WidgetManager.SetLanguage(language);

		return result;
	}

	protected static void MatchEnglish(LL_WebJsonSlotting root, array<ref LL_WebsiteSquadIndex> gameSquads, LL_WebsiteSlottingImportResult result)
	{
		foreach (LL_WebJsonSide side : root.sides)
		{
			bool sideMatched = false;
			foreach (LL_WebsiteSquadIndex probe : gameSquads)
			{
				if (EqualsNoCase(probe.m_sFactionKey, side.name))
				{
					sideMatched = true;
					break;
				}
			}

			if (!sideMatched)
			{
				result.m_aWarnings.Insert(WidgetManager.Translate("#LL-SlottingImport_WarnSide", side.name));
				foreach (LL_WebJsonSquad skipped : side.squads)
					result.m_iJsonSlots += skipped.slots.Count();
				continue;
			}

			foreach (LL_WebJsonSquad squad : side.squads)
			{
				result.m_iJsonSlots += squad.slots.Count();

				LL_WebsiteSquadIndex gameSquad = null;
				foreach (LL_WebsiteSquadIndex candidate : gameSquads)
				{
					if (!EqualsNoCase(candidate.m_sFactionKey, side.name))
						continue;

					if (EqualsNoCase(WidgetManager.Translate(candidate.m_sNameEn).Trim(), squad.name.Trim()))
					{
						gameSquad = candidate;
						break;
					}
				}

				if (!gameSquad)
				{
					result.m_aWarnings.Insert(WidgetManager.Translate("#LL-SlottingImport_WarnSquad", squad.name, side.name));
					continue;
				}

				int jsonCount = squad.slots.Count();
				int gameCount = gameSquad.m_aSlots.Count();
				if (jsonCount != gameCount)
					result.m_aWarnings.Insert(WidgetManager.Translate("#LL-SlottingImport_WarnCount", squad.name, jsonCount.ToString(), gameCount.ToString()));

				// Index-based pairing from the same export order; role text is only sanity-checked.
				int pairCount = Math.Min(jsonCount, gameCount);
				int roleMismatches = 0;

				for (int i = 0; i < pairCount; i++)
				{
					LL_WebJsonSlot jsonSlot = squad.slots[i];
					LL_SlotData gameSlot = gameSquad.m_aSlots[i];

					result.m_iMatchedSlots++;

					if (!EqualsNoCase(WidgetManager.Translate(gameSlot.m_sName).Trim(), jsonSlot.role.Trim()))
						roleMismatches++;

					string displayLabel = "";
					if (jsonSlot.occupant)
						displayLabel = SanitizeLabel(jsonSlot.occupant.GetDisplayLabel());

					if (displayLabel != "")
						result.m_mLabels.Set(gameSlot.m_iRplId, displayLabel);
				}

				if (roleMismatches > 0)
					result.m_aWarnings.Insert(WidgetManager.Translate("#LL-SlottingImport_WarnRoles", squad.name, roleMismatches.ToString()));
			}
		}
	}

	// One reliable RPC per chunk of "rplId\tlabel" lines, so a 127-slot import stays a
	// handful of messages.

	protected static const int CHUNK_MAX_CHARS = 800;

	static array<string> EncodeLabelChunks(notnull map<int, string> labels)
	{
		array<string> chunks = {};
		string current = "";

		for (int i = 0; i < labels.Count(); i++)
		{
			string entry = labels.GetKey(i).ToString() + "\t" + labels.GetElement(i);

			if (current != "" && current.Length() + entry.Length() + 1 > CHUNK_MAX_CHARS)
			{
				chunks.Insert(current);
				current = "";
			}

			if (current != "")
				current += "\n";
			current += entry;
		}

		if (current != "")
			chunks.Insert(current);

		return chunks;
	}

	static void DecodeLabelChunk(string chunk, notnull map<int, string> intoLabels)
	{
		array<string> lines = {};
		chunk.Split("\n", lines, true);

		foreach (string line : lines)
		{
			int tab = line.IndexOf("\t");
			if (tab < 1)
				continue;

			int rplId = line.Substring(0, tab).ToInt();
			string label = SanitizeLabel(line.Substring(tab + 1, line.Length() - tab - 1));

			if (rplId != 0 && label != "")
				intoLabels.Set(rplId, label);
		}
	}

	// Group names are kept raw; callers translate inside their own forced-English window.
	static array<ref LL_WebsiteSquadIndex> BuildSquadIndex()
	{
		array<ref LL_WebsiteSquadIndex> squads = {};

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return squads;

		LL_WebsiteSquadIndex current = null;
		foreach (LL_SlotData slot : mgr.GetSlots())
		{
			if (!current || current.m_sFactionKey != slot.m_sFactionKey || current.m_iGroupId != slot.m_iGroupId)
			{
				current = new LL_WebsiteSquadIndex();
				current.m_sFactionKey = slot.m_sFactionKey;
				current.m_iGroupId = slot.m_iGroupId;
				current.m_sNameEn = slot.m_sGroupName;
				squads.Insert(current);
			}

			current.m_aSlots.Insert(slot);
		}

		return squads;
	}

	protected static bool EqualsNoCase(string a, string b)
	{
		a.ToLower();
		b.ToLower();
		return a == b;
	}

	// System.ExportToClipboard hands UTF-8 bytes to the OS as single-byte text, so
	// non-ASCII arrives as mojibake; JSON \uXXXX escapes survive the hop. Runs of ASCII
	// are appended whole and capped because Substring truncates at 8191 characters.
	static string EscapeNonAscii(string s)
	{
		const int RUN_MAX = 4096;

		int len = s.Length();
		string result = "";
		int i = 0;
		int runStart = 0;

		while (i < len)
		{
			int lead = ByteAt(s, i);

			if (lead < 0x80)
			{
				i++;
				if (i - runStart >= RUN_MAX)
				{
					result += s.Substring(runStart, i - runStart);
					runStart = i;
				}
				continue;
			}

			if (i > runStart)
				result += s.Substring(runStart, i - runStart);

			int codePoint;
			int continuations;
			if (lead >= 0xF0)
			{
				codePoint = lead & 0x07;
				continuations = 3;
			}
			else if (lead >= 0xE0)
			{
				codePoint = lead & 0x0F;
				continuations = 2;
			}
			else if (lead >= 0xC0)
			{
				codePoint = lead & 0x1F;
				continuations = 1;
			}
			else
			{
				// Stray continuation byte: carried through rather than dropped.
				codePoint = lead;
				continuations = 0;
			}

			if (i + continuations >= len)
				continuations = len - i - 1;

			for (int c = 1; c <= continuations; c++)
				codePoint = (codePoint << 6) | (ByteAt(s, i + c) & 0x3F);

			i += continuations + 1;
			runStart = i;

			if (codePoint > 0xFFFF)
			{
				int astral = codePoint - 0x10000;
				result += "\\u" + Hex4(0xD800 + (astral >> 10)) + "\\u" + Hex4(0xDC00 + (astral & 0x3FF));
			}
			else
			{
				result += "\\u" + Hex4(codePoint);
			}
		}

		if (len > runStart)
			result += s.Substring(runStart, len - runStart);

		return result;
	}

	// ToAscii may hand the byte back signed.
	protected static int ByteAt(string s, int index)
	{
		int b = s.ToAscii(index);
		if (b < 0)
			b += 256;

		return b;
	}

	protected static string Hex4(int value)
	{
		return ByteToHex((value >> 8) & 0xFF) + ByteToHex(value & 0xFF);
	}

	protected static string JsonEscape(string s)
	{
		s.Replace("\\", "\\\\");
		s.Replace("\"", "\\\"");
		s.Replace("\n", "\\n");
		s.Replace("\r", "\\r");
		s.Replace("\t", "\\t");
		return s;
	}

	// Lowercase alphanumerics with runs of anything else collapsed to one dash.
	protected static string Slug(string s)
	{
		string slug = "";
		bool lastDash = true;
		int len = s.Length();

		for (int i = 0; i < len; i++)
		{
			int c = s.ToAscii(i);
			bool alnum = (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122);

			if (alnum)
			{
				slug += s.Substring(i, 1);
				lastDash = false;
			}
			else if (!lastDash)
			{
				slug += "-";
				lastDash = true;
			}
		}

		slug.ToLower();

		while (slug.Length() > 0 && slug.Substring(slug.Length() - 1, 1) == "-")
			slug = slug.Substring(0, slug.Length() - 1);

		if (slug == "")
			slug = "x";

		return slug;
	}

	protected static string ColorToHex(Color color)
	{
		int r = Math.Round(color.R() * 255);
		int g = Math.Round(color.G() * 255);
		int b = Math.Round(color.B() * 255);
		return "#" + ByteToHex(r) + ByteToHex(g) + ByteToHex(b);
	}

	protected static string ByteToHex(int value)
	{
		value = Math.ClampInt(value, 0, 255);
		string digits = "0123456789ABCDEF";
		return digits.Substring(value / 16, 1) + digits.Substring(value % 16, 1);
	}
}