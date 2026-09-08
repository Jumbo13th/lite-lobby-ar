// Optional website registration gate, server-side only. On audit success the server asks
// an external site whether the player's identity GUID is registered:
//   GET <ApiUrl>/<PlayerEndpoint>?arma_id=<guid>&secret=<Secret>
//   200 = registered (a non-empty "active_bans" array kicks as banned when EnforceBans is
//   on), 404 = not registered, anything else = fail closed and kick.
// The secret rides in the query string because the runtime REST API has no custom-header
// support. Whitelisted GUIDs are never kicked; /verify off suspends checks during an
// outage and /verify on re-checks everyone. A 200 also carries callsign and unit, which
// become the player's display name "[TAG] Callsign" (stored plain, coloured at render).
// Configured through $profile:LL_PlayerVerification.json, never prefab attributes: the
// secret is operator data. Kicks use timeout 0 because every connect re-verifies.
// Dedicated servers only: elsewhere identity GUIDs are synthetic.

class LL_PlayerVerificationConfig : JsonApiStruct
{
	bool Enabled = false;
	string ApiUrl = "";
	string PlayerEndpoint = "api/gameserver/player";
	string UnitsEndpoint = "api/gameserver/units";
	string SeasonEndpoint = "api/gameserver/season";
	string StatsPageUrl = "";
	string Secret = "";
	int TimeoutSeconds = 10;
	bool EnforceBans = true;
	ref array<string> Whitelist = {};

	void LL_PlayerVerificationConfig()
	{
		RegV("Enabled");
		RegV("ApiUrl");
		RegV("PlayerEndpoint");
		RegV("UnitsEndpoint");
		RegV("SeasonEndpoint");
		RegV("StatsPageUrl");
		RegV("Secret");
		RegV("TimeoutSeconds");
		RegV("EnforceBans");
		RegV("Whitelist");
	}
}

// Unregistered JSON keys are ignored; expires_at is not registered because it can be null.
class LL_PlayerVerificationBan : JsonApiStruct
{
	string type;
	string reason;

	void LL_PlayerVerificationBan()
	{
		RegV("type");
		RegV("reason");
	}
}

class LL_PlayerVerificationUnit : JsonApiStruct
{
	string name;
	string tag;

	void LL_PlayerVerificationUnit()
	{
		RegV("name");
		RegV("tag");
	}
}

class LL_PlayerVerificationResponse : JsonApiStruct
{
	string callsign;
	ref LL_PlayerVerificationUnit unit = new LL_PlayerVerificationUnit();
	ref array<ref LL_PlayerVerificationBan> active_bans = {};
	// Registered only to silence the parser: every undeclared array warns on each parse.
	ref array<string> badges = {};

	void LL_PlayerVerificationResponse()
	{
		RegV("callsign");
		RegV("unit");
		RegV("active_bans");
		RegV("badges");
	}
}

// The engine deletes a RestCallback that is not strong-referenced before the response
// arrives, so the component holds these until the callback fires.
class LL_PlayerVerificationRequest : Managed
{
	int m_iPlayerId;
	string m_sUid;
	bool m_bInfoOnly;
	ref RestCallback m_Callback;
	LL_PlayerVerificationComponent m_Component;

	void Start(notnull LL_PlayerVerificationComponent component, int playerId, string uid, notnull RestContext context, string request)
	{
		m_Component = component;
		m_iPlayerId = playerId;
		m_sUid = uid;

		m_Callback = new RestCallback();
		m_Callback.SetOnSuccess(OnSuccess);
		m_Callback.SetOnError(OnError);
		context.GET(m_Callback, request);
	}

	protected void OnSuccess(RestCallback cb)
	{
		if (m_Component)
			m_Component.OnVerificationResponse(this, cb);
	}

	protected void OnError(RestCallback cb)
	{
		if (m_Component)
			m_Component.OnVerificationError(this, cb);
	}
}

class LL_PlayerVerificationComponentClass : SCR_BaseGameModeComponentClass
{
}

class LL_PlayerVerificationComponent : SCR_BaseGameModeComponent
{
	[Attribute("0", UIWidgets.CheckBox, "TESTING ONLY — answer all website calls (player verification, units list, season standings) from the built-in LL_WebsiteMock instead of HTTP. Nobody gets kicked and every player is assigned a mock unit. Ignored whenever the real verification config is active, but never ship a mission with this ticked.", category: "Lite Lobby")]
	protected bool m_bUseWebsiteMock;

	protected static const string CONFIG_PATH = "$profile:LL_PlayerVerification.json";

	static const int CMD_RESULT_SUSPENDED = 0;
	static const int CMD_RESULT_RESUMED = 1;
	static const int CMD_RESULT_NOT_ACTIVE = 2;

	protected static LL_PlayerVerificationComponent s_Instance;

	protected ref LL_PlayerVerificationConfig m_Config;
	protected RestContext m_RestContext;
	protected ref array<ref LL_PlayerVerificationRequest> m_aPending = {};
	protected bool m_bActive;
	protected bool m_bMock;
	protected bool m_bRealGateIntended;
	protected bool m_bSuspended;

	static LL_PlayerVerificationComponent GetInstance()
	{
		return s_Instance;
	}

	void ~LL_PlayerVerificationComponent()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		// Registered even when off, so /verify can answer "not enabled".
		s_Instance = this;

		if (!LoadConfig())
		{
			TryActivateMock();
			return;
		}

		if (RplSession.Mode() != RplMode.Dedicated)
		{
			Print("[LL_Lobby] PlayerVerification: enabled in config but not on a dedicated server — identity GUIDs are synthetic here, skipping all checks", LogLevel.WARNING);
			TryActivateMock();
			return;
		}

		m_RestContext = GetGame().GetRestApi().GetContext(m_Config.ApiUrl);
		if (!m_RestContext)
		{
			Print(string.Format("[LL_Lobby] PlayerVerification: could not create REST context for '%1' — verification disabled", m_Config.ApiUrl), LogLevel.ERROR);
			TryActivateMock();
			return;
		}

		m_RestContext.SetTimeout(m_Config.TimeoutSeconds);

		m_bActive = true;
		Print(string.Format("[LL_Lobby] PlayerVerification: active — %1/%2 (enforce-bans=%3, whitelist=%4)",
			m_Config.ApiUrl, m_Config.PlayerEndpoint, m_Config.EnforceBans, m_Config.Whitelist.Count()), LogLevel.NORMAL);

		if (m_Config.Whitelist.IsEmpty())
			Print("[LL_Lobby] PlayerVerification: Whitelist is EMPTY — with fail-closed checks a website outage will bounce every player, including admins. Add the admins' identity GUIDs to " + CONFIG_PATH, LogLevel.WARNING);

		// Against a secret-protected API an empty secret means 401 on every check.
		if (m_Config.Secret == "")
			Print("[LL_Lobby] PlayerVerification: Secret is EMPTY — if the API requires one, every check will fail with HTTP 401 and every non-whitelisted player will be kicked. Set the Secret in " + CONFIG_PATH, LogLevel.WARNING);
	}

	// The mock only steps in where the real gate cannot run; a working production config
	// always wins. LL_StatsManager follows the same decision through IsMockActive.
	protected void TryActivateMock()
	{
		if (!m_bUseWebsiteMock)
			return;

		// A dedicated server whose operator intended the real gate must stay a visible failure.
		if (m_bRealGateIntended && RplSession.Mode() == RplMode.Dedicated)
		{
			Print("[LL_Lobby] PlayerVerification: mock checkbox IGNORED — the real website gate is configured but failed to start. Fix " + CONFIG_PATH + "; verification stays disabled.", LogLevel.ERROR);
			return;
		}

		m_bMock = true;
		m_bActive = true;
		Print("[LL_Lobby] PlayerVerification: WEBSITE MOCK ACTIVE (checkbox on LL_PlayerVerificationComponent) — all website calls (player checks, units, season) are answered by the built-in LL_WebsiteMock. Nobody gets kicked. Never use on a production server.", LogLevel.WARNING);

		// Production-shaped on a dedicated box: nag the log.
		if (RplSession.Mode() == RplMode.Dedicated)
			GetGame().GetCallqueue().CallLater(PrintMockReminder, 300000, true);
	}

	protected void PrintMockReminder()
	{
		Print("[LL_Lobby] PlayerVerification: WEBSITE MOCK is still active on this DEDICATED server — untick 'Use website mock' before a real game.", LogLevel.WARNING);
	}

	bool IsMockActive()
	{
		return m_bActive && m_bMock;
	}

	// Workbench and peer play never fire OnPlayerAuditSuccess, so the mock hooks connect.
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		if (!m_bActive || !m_bMock || m_bSuspended)
			return;

		VerifyPlayer(playerId);
	}

	// A missing file gets a disabled template.
	protected bool LoadConfig()
	{
		m_Config = new LL_PlayerVerificationConfig();

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile(CONFIG_PATH))
		{
			CreateTemplateConfig();
			return false;
		}

		if (!ctx.ReadValue("", m_Config))
		{
			// A written config is intent even when unreadable; a typo must not hand the
			// server to the mock.
			m_bRealGateIntended = true;
			Print("[LL_Lobby] PlayerVerification: " + CONFIG_PATH + " is malformed — verification disabled", LogLevel.ERROR);
			return false;
		}

		m_bRealGateIntended = m_Config.Enabled;

		if (!m_Config.Enabled)
			return false;

		m_Config.ApiUrl = m_Config.ApiUrl.Trim();
		m_Config.PlayerEndpoint = m_Config.PlayerEndpoint.Trim();
		m_Config.Secret = m_Config.Secret.Trim();

		if (m_Config.ApiUrl == "")
		{
			Print("[LL_Lobby] PlayerVerification: Enabled but ApiUrl is empty — verification disabled", LogLevel.ERROR);
			return false;
		}

		if (m_Config.TimeoutSeconds < 1)
			m_Config.TimeoutSeconds = 10;
		if (m_Config.TimeoutSeconds > 120)
			m_Config.TimeoutSeconds = 120;

		// The engine concatenates context + request verbatim.
		if (m_Config.ApiUrl.Substring(m_Config.ApiUrl.Length() - 1, 1) != "/")
			m_Config.ApiUrl += "/";
		while (m_Config.PlayerEndpoint != "" && m_Config.PlayerEndpoint.Substring(0, 1) == "/")
			m_Config.PlayerEndpoint = m_Config.PlayerEndpoint.Substring(1, m_Config.PlayerEndpoint.Length() - 1);

		return true;
	}

	protected void CreateTemplateConfig()
	{
		FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.WRITE);
		if (!file)
			return;

		file.WriteLine("{");
		file.WriteLine("	\"_readme\": \"Lite Lobby website registration gate. Set Enabled=true and fill ApiUrl/Secret. Protocol: GET <ApiUrl>/<PlayerEndpoint>?arma_id=<guid>&secret=<Secret> must answer 200 (registered, optional active_bans array) or 404 (not registered). Checks FAIL CLOSED: put the admins' identity GUIDs in Whitelist so they can always join and suspend checks with /verify off in chat during a website outage.\",");
		file.WriteLine("	\"Enabled\": false,");
		file.WriteLine("	\"ApiUrl\": \"https://example.com/\",");
		file.WriteLine("	\"PlayerEndpoint\": \"api/gameserver/player\",");
		file.WriteLine("	\"UnitsEndpoint\": \"api/gameserver/units\",");
		file.WriteLine("	\"SeasonEndpoint\": \"api/gameserver/season\",");
		file.WriteLine("	\"StatsPageUrl\": \"\",");
		file.WriteLine("	\"Secret\": \"\",");
		file.WriteLine("	\"TimeoutSeconds\": 10,");
		file.WriteLine("	\"EnforceBans\": true,");
		file.WriteLine("	\"Whitelist\": []");
		file.WriteLine("}");
		file.Close();

		Print("[LL_Lobby] PlayerVerification: wrote disabled template config to " + CONFIG_PATH, LogLevel.NORMAL);
	}

	override void OnPlayerAuditSuccess(int playerId)
	{
		super.OnPlayerAuditSuccess(playerId);

		// Mock rounds run from OnPlayerConnected; a dedicated dev server also audits.
		if (m_bMock)
			return;

		if (!m_bActive)
			return;

		if (m_bSuspended)
		{
			Print(string.Format("[LL_Lobby] PlayerVerification: SUSPENDED (/verify off) — player %1 allowed without a check", playerId), LogLevel.WARNING);
			return;
		}

		VerifyPlayer(playerId);
	}

	// Runs on audit success and again for everyone when /verify on ends a suspension.
	protected void VerifyPlayer(int playerId)
	{
		if (HasPendingFor(playerId))
			return;

		if (m_bMock)
		{
			StartMockCheck(playerId);
			return;
		}

		UUID uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (uid.IsNull())
		{
			// Same policy as an unreachable API.
			FailClosed(playerId, "no identity GUID from backend");
			return;
		}

		// Whitelisted players are never kicked but still get their website name.
		bool infoOnly = IsWhitelisted(uid);
		if (infoOnly)
			Print(string.Format("[LL_Lobby] PlayerVerification: player %1 (%2) whitelisted — no kick check, fetching website name only", playerId, uid), LogLevel.NORMAL);

		string request = m_Config.PlayerEndpoint + "?arma_id=" + uid;
		if (m_Config.Secret != "")
			request += "&secret=" + EncodeQueryValue(m_Config.Secret);

		LL_PlayerVerificationRequest pending = new LL_PlayerVerificationRequest();
		pending.m_bInfoOnly = infoOnly;
		m_aPending.Insert(pending);
		pending.Start(this, playerId, uid, m_RestContext, request);
	}

	// A real pending entry (dedup and disconnect pruning work unchanged) with a delayed
	// local callback.
	protected void StartMockCheck(int playerId)
	{
		LL_PlayerVerificationRequest pending = new LL_PlayerVerificationRequest();
		pending.m_iPlayerId = playerId;
		pending.m_sUid = "mock:" + GetGame().GetPlayerManager().GetPlayerName(playerId);
		m_aPending.Insert(pending);

		GetGame().GetCallqueue().CallLater(FinishMockCheck, LL_WebsiteMock.GetLatencyMs(), false, pending);
	}

	// ReleasePending is bypassed: its identity re-check needs a backend GUID.
	protected void FinishMockCheck(LL_PlayerVerificationRequest request)
	{
		int idx = m_aPending.Find(request);
		if (idx == -1)
			return;
		m_aPending.Remove(idx);

		HandleRegistered(request, LL_WebsiteMock.BuildPlayerResponseJson(request.m_iPlayerId));
	}

	// A response must not act on a playerId that no longer means that player.
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		for (int i = m_aPending.Count() - 1; i >= 0; i--)
		{
			if (m_aPending[i].m_iPlayerId == playerId)
				m_aPending.Remove(i);
		}
	}

	// A completed request with a non-2xx status may arrive through either callback.
	void OnVerificationResponse(LL_PlayerVerificationRequest request, RestCallback cb)
	{
		if (!ReleasePending(request))
			return;

		HandleHttpResult(request, cb);
	}

	void OnVerificationError(LL_PlayerVerificationRequest request, RestCallback cb)
	{
		if (!ReleasePending(request))
			return;

		// A 404/401 can surface here as a "server error"; judge it by the HTTP code.
		int httpCode = cb.GetHttpCode();
		if (httpCode > 0)
		{
			HandleHttpResult(request, cb);
			return;
		}

		if (request.m_bInfoOnly)
		{
			Print(string.Format("[LL_Lobby] PlayerVerification: name lookup for whitelisted player %1 failed (rest result %2) — keeping engine name", request.m_iPlayerId, cb.GetRestResult()), LogLevel.WARNING);
			return;
		}

		FailClosed(request.m_iPlayerId, string.Format("request failed (rest result %1)", cb.GetRestResult()));
	}

	protected void HandleHttpResult(LL_PlayerVerificationRequest request, RestCallback cb)
	{
		int playerId = request.m_iPlayerId;
		int httpCode = cb.GetHttpCode();

		// A whitelisted lookup can only rename.
		if (request.m_bInfoOnly && httpCode != 200)
		{
			Print(string.Format("[LL_Lobby] PlayerVerification: name lookup for whitelisted player %1 returned HTTP %2 — keeping engine name", playerId, httpCode), LogLevel.NORMAL);
			return;
		}

		switch (httpCode)
		{
			case 200:
			{
				HandleRegistered(request, cb.GetData());
				break;
			}
			case 404:
			{
				Print(string.Format("[LL_Lobby] PlayerVerification: player %1 (%2) is not registered — kicking", playerId, request.m_sUid), LogLevel.NORMAL);
				Kick(playerId, PlayerManagerKickReason.KICK);
				break;
			}
			case 401:
			{
				FailClosed(playerId, "API rejected our secret (HTTP 401) — check the Secret in " + CONFIG_PATH);
				break;
			}
			default:
			{
				FailClosed(playerId, string.Format("unexpected HTTP %1", httpCode));
				break;
			}
		}
	}

	protected void HandleRegistered(LL_PlayerVerificationRequest request, string json)
	{
		int playerId = request.m_iPlayerId;

		LL_PlayerVerificationResponse response = new LL_PlayerVerificationResponse();
		response.ExpandFromRAW(json);

		if (m_Config.EnforceBans && !response.active_bans.IsEmpty())
		{
			string reason = response.active_bans[0].reason;
			if (!request.m_bInfoOnly)
			{
				Print(string.Format("[LL_Lobby] PlayerVerification: player %1 (%2) has an active website ban (\"%3\") — kicking", playerId, request.m_sUid, reason), LogLevel.NORMAL);
				Kick(playerId, PlayerManagerKickReason.BAN);
				return;
			}
			Print(string.Format("[LL_Lobby] PlayerVerification: whitelisted player %1 (%2) has an active website ban (\"%3\") — allowed by whitelist", playerId, request.m_sUid, reason), LogLevel.WARNING);
		}

		ApplyWebsiteName(playerId, response);

		// The response also carries the unit: the only game-side source of unit attribution.
		LL_StatsManager stats = LL_StatsManager.GetInstance();
		if (stats)
		{
			string unitTag = "";
			string unitName = "";
			if (response.unit)
			{
				unitTag = response.unit.tag.Trim();
				unitName = response.unit.name.Trim();
			}
			stats.RegisterVerifiedIdentity(playerId, response.callsign.Trim(), unitTag, unitName);
		}

		Print(string.Format("[LL_Lobby] PlayerVerification: player %1 verified as \"%2\"", playerId, response.callsign), LogLevel.NORMAL);
	}

	// Stored plain: markup would leak into non-rich renderers and fight the state tints.
	protected void ApplyWebsiteName(int playerId, LL_PlayerVerificationResponse response)
	{
		string display = SanitizeNamePart(response.callsign);
		if (display == "")
			return;

		string tag = "";
		if (response.unit)
		{
			tag = SanitizeNamePart(response.unit.tag);
			if (tag == "")
				tag = SanitizeNamePart(response.unit.name);
		}
		if (tag != "")
			display = string.Format("[%1]%2", tag, display);

		// Same ballpark as the engine's own name length limit.
		if (display.Length() > 40)
			display = display.Substring(0, 40);

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (mgr)
			mgr.SetPlayerName_S(playerId, display);
	}

	// Callsigns are untrusted: strip what the vanilla name filter strips plus brackets,
	// the unit-tag delimiter.
	protected static string SanitizeNamePart(string value)
	{
		string s = value.Trim();
		s.Replace("<", "");
		s.Replace(">", "");
		s.Replace("#", "");
		s.Replace("[", "");
		s.Replace("]", "");
		return s;
	}

	// Fail closed so an outage is loud instead of silently opening the gate.
	protected void FailClosed(int playerId, string why)
	{
		Print(string.Format("[LL_Lobby] PlayerVerification: %1 — fail-closed, kicking player %2 (an admin can suspend checks with /verify off)", why, playerId), LogLevel.WARNING);
		Kick(playerId, PlayerManagerKickReason.KICK);
	}

	// The statistics fetches ride this component's config and REST context.

	RestContext GetWebsiteRestContext()
	{
		return m_RestContext;
	}

	// StatsPageUrl verbatim: deriving it from ApiUrl guesses wrong when the public site
	// differs from the API host.
	string GetSiteDisplayUrl()
	{
		if (!m_Config)
			return "";

		return m_Config.StatsPageUrl.Trim();
	}

	string BuildUnitsRequest()
	{
		if (!m_bActive)
			return "";
		return BuildSecretRequest(m_Config.UnitsEndpoint);
	}

	string BuildSeasonRequest()
	{
		if (!m_bActive)
			return "";
		return BuildSecretRequest(m_Config.SeasonEndpoint);
	}

	protected string BuildSecretRequest(string endpoint)
	{
		endpoint = endpoint.Trim();
		while (endpoint != "" && endpoint.Substring(0, 1) == "/")
			endpoint = endpoint.Substring(1, endpoint.Length() - 1);

		if (endpoint == "")
			return "";

		string request = endpoint;
		if (m_Config.Secret != "")
			request += "?secret=" + EncodeQueryValue(m_Config.Secret);
		return request;
	}

	// Suspending drops in-flight checks so a queued timeout cannot kick after the admin
	// opened the gate. Runtime-only.
	int SetVerificationSuspended_S(bool suspend)
	{
		if (!m_bActive)
			return CMD_RESULT_NOT_ACTIVE;

		m_bSuspended = suspend;
		if (suspend)
		{
			m_aPending.Clear();
			Print("[LL_Lobby] PlayerVerification: checks SUSPENDED by admin (/verify off)", LogLevel.WARNING);
			return CMD_RESULT_SUSPENDED;
		}

		Print("[LL_Lobby] PlayerVerification: checks resumed by admin (/verify on) — re-checking all connected players", LogLevel.NORMAL);
		RecheckConnectedPlayers();
		return CMD_RESULT_RESUMED;
	}

	// Players who joined while suspended were never verified.
	protected void RecheckConnectedPlayers()
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);
		foreach (int pid : playerIds)
			VerifyPlayer(pid);
	}

	protected bool HasPendingFor(int playerId)
	{
		foreach (LL_PlayerVerificationRequest pending : m_aPending)
		{
			if (pending.m_iPlayerId == playerId)
				return true;
		}
		return false;
	}

	// Only the secret needs this; identity GUIDs are URL-safe.
	protected static string EncodeQueryValue(string value)
	{
		string s = value;
		s.Replace("%", "%25");
		s.Replace(" ", "%20");
		s.Replace("&", "%26");
		s.Replace("+", "%2B");
		s.Replace("#", "%23");
		s.Replace("?", "%3F");
		return s;
	}

	protected void Kick(int playerId, PlayerManagerKickReason reason)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
			pm.KickPlayer(playerId, reason, 0);
	}

	protected bool IsWhitelisted(string uid)
	{
		string lowerUid = uid;
		lowerUid.ToLower();
		foreach (string entry : m_Config.Whitelist)
		{
			string lowerEntry = entry.Trim();
			lowerEntry.ToLower();
			if (lowerEntry == lowerUid)
				return true;
		}
		return false;
	}

	// False if the player disconnected since; also releases the callback's strong ref.
	protected bool ReleasePending(LL_PlayerVerificationRequest request)
	{
		int idx = m_aPending.Find(request);
		if (idx == -1)
			return false;

		m_aPending.Remove(idx);

		// A slot reassignment can race a slow response.
		UUID currentUid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(request.m_iPlayerId);
		if (currentUid.IsNull() || currentUid != request.m_sUid)
			return false;

		return true;
	}
}