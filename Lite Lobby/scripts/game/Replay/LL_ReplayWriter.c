// Spells and writes every line of a recording (specs/005-replay-recording/contracts/recording-format.md).
// Lines collect in memory and go to disk in bounded steps; the file is closed after each
// step because the file interface has no flush and data reaches the disk only on close.
class LL_ReplayWriter : Managed
{
	static const string FOLDER = "$profile:LL_Replays";
	static const string STATUS_PATH = "$profile:LL_Replays/status.txt";
	protected static const int LINES_PER_STEP = 300;

	protected string m_sSession;
	protected int m_iSegment;
	protected string m_sPath;
	protected string m_sIdentityPath;

	protected ref array<string> m_aLines = {};
	protected int m_iWritten;
	protected ref array<string> m_aIdentity = {};

	protected bool m_bSnapshot;
	protected ref array<int> m_aHeldT = {};
	protected ref array<string> m_aHeldLines = {};

	protected int m_iTime;
	protected int m_iLastWrittenT = -1;
	protected bool m_bClockHeld;
	protected int m_iLastAnchorUnix;

	protected bool m_bFailed;
	protected int m_iBytes;
	protected int m_iLastWriteUnix;
	protected int m_iFlushMsSum;
	protected int m_iFlushMsMax;
	protected ref map<string, int> m_mTypeCounts = new map<string, int>();

	void Begin(string session, int segment)
	{
		m_sSession = session;
		m_iSegment = segment;
		m_sPath = string.Format("%1/%2-%3.llr", FOLDER, session, segment);
		m_sIdentityPath = string.Format("%1/%2-identities.txt", FOLDER, session);
		m_aLines.Clear();
		m_iWritten = 0;
		m_iLastWrittenT = -1;
		FileIO.MakeDirectory(FOLDER);
	}

	void AddHeader(string line)
	{
		Push(line);
	}

	void SetTime(int t)
	{
		m_iTime = t;
	}

	int GetTime()
	{
		return m_iTime;
	}

	//! A record made while a snapshot is being built waits until SNAP-END, because nothing
	//! else may sit between SNAP and SNAP-END.
	void Add(string line)
	{
		if (m_bSnapshot)
		{
			m_aHeldT.Insert(m_iTime);
			m_aHeldLines.Insert(line);
			return;
		}

		Emit(m_iTime, line);
	}

	void BeginSnapshot()
	{
		Emit(m_iTime, "SNAP");
		m_bSnapshot = true;
	}

	void AddSnapshotLine(string line)
	{
		Push(line);
	}

	void EndSnapshot()
	{
		Push("SNAP-END");
		m_bSnapshot = false;

		int count = m_aHeldLines.Count();
		for (int i = 0; i < count; i++)
		{
			Emit(m_aHeldT[i], m_aHeldLines[i]);
		}

		m_aHeldT.Clear();
		m_aHeldLines.Clear();
	}

	//! While the mission clock stands still, records share one time; an anchor per new
	//! wall-clock second keeps their real time known within a second.
	void SetClockHeld(bool held)
	{
		m_bClockHeld = held;
	}

	void Anchor()
	{
		if (m_bSnapshot)
			return;

		m_iLastAnchorUnix = System.GetUnixTime();
		Push(string.Format("U %1 %2", m_iLastAnchorUnix, m_iTime));
	}

	//! Identities go to their own file and never into a segment, so a published recording names no account.
	void AddIdentity(int pref, string identity)
	{
		m_aIdentity.Insert(string.Format("I %1 %2", pref, identity));
	}

	//! True once everything buffered is on disk.
	bool FlushStep_S()
	{
		if (m_bFailed)
			return true;

		int count = m_aLines.Count();
		if (m_iWritten >= count)
		{
			m_aLines.Clear();
			m_iWritten = 0;
			return WriteIdentity_S();
		}

		int startTick = System.GetTickCount();

		FileHandle file;
		if (FileIO.FileExists(m_sPath))
			file = FileIO.OpenFile(m_sPath, FileMode.APPEND);
		else
			file = FileIO.OpenFile(m_sPath, FileMode.WRITE);

		if (!file)
		{
			Fail(m_sPath);
			return true;
		}

		int end = m_iWritten + LINES_PER_STEP;
		if (end > count)
			end = count;

		for (int i = m_iWritten; i < end; i++)
		{
			string line = m_aLines[i];
			file.Write(line + "\n");
			m_iBytes += line.Length() + 1;
		}

		file.Close();
		m_iWritten = end;
		m_iLastWriteUnix = System.GetUnixTime();

		int ms = System.GetTickCount() - startTick;
		m_iFlushMsSum += ms;
		if (ms > m_iFlushMsMax)
			m_iFlushMsMax = ms;

		if (m_iWritten < count)
			return false;

		m_aLines.Clear();
		m_iWritten = 0;
		return WriteIdentity_S();
	}

	static void WriteStatus_S(string state, string session, int segment)
	{
		FileIO.MakeDirectory(FOLDER);
		FileHandle file = FileIO.OpenFile(STATUS_PATH, FileMode.WRITE);
		if (!file)
			return;

		string sessionField = session;
		if (sessionField == "")
			sessionField = "-";

		string segmentField = "-";
		if (segment > 0)
			segmentField = segment.ToString();

		file.Write(string.Format("LLS 1 %1 %2 %3 %4\n", state, sessionField, segmentField, System.GetUnixTime()));
		file.Close();
	}

	bool HasFailed()
	{
		return m_bFailed;
	}

	int GetBytes()
	{
		return m_iBytes;
	}

	int GetLastWriteUnix()
	{
		return m_iLastWriteUnix;
	}

	//! Resets the per-period counters; returns the summary fields of the period.
	string TakeSummary(out int flushMsSum)
	{
		flushMsSum = m_iFlushMsSum;

		string types;
		int lines;
		foreach (string type, int count : m_mTypeCounts)
		{
			if (types != "")
				types += ",";
			types += type + ":" + count;
			lines += count;
		}

		string summary = string.Format("lines=%1 bytes=%2 maxFlushMs=%3 types=%4", lines, m_iBytes, m_iFlushMsMax, types);
		m_iFlushMsSum = 0;
		m_iFlushMsMax = 0;
		m_mTypeCounts.Clear();
		return summary;
	}

	//! JSON string literal; control characters other than tab and line breaks become spaces.
	static string Str(string value)
	{
		string text = value;
		for (int c = 1; c < 32; c++)
		{
			if (c == 9 || c == 10 || c == 13)
				continue;

			text.Replace(c.AsciiToString(), " ");
		}

		text.Replace("\\", "\\\\");
		text.Replace("\"", "\\\"");
		text.Replace("\n", "\\n");
		text.Replace("\r", "\\r");
		text.Replace("\t", "\\t");
		return "\"" + text + "\"";
	}

	static string Tok(string value)
	{
		if (value == "")
			return "-";

		string text = value;
		text.Replace(" ", "_");
		text.Replace("\"", "_");
		text.Replace("\t", "_");
		text.Replace("\n", "_");
		text.Replace("\r", "_");
		return text;
	}

	static string IntOr(int value, bool present)
	{
		if (!present)
			return "-";

		return value.ToString();
	}

	static string ColorHex(Color color, bool withAlpha)
	{
		if (!color)
			return "-";

		string hex = ByteHex(color.R()) + ByteHex(color.G()) + ByteHex(color.B());
		if (withAlpha)
			hex += ByteHex(color.A());

		return hex;
	}

	protected static string ByteHex(float channel)
	{
		int value = Math.Round(Math.Clamp(channel, 0, 1) * 255);
		string hex = value.ToString(2, true);
		hex.ToLower();
		return hex;
	}

	protected void Emit(int t, string line)
	{
		if (t != m_iLastWrittenT)
		{
			Push("T " + t);
			m_iLastWrittenT = t;
		}

		if (m_bClockHeld)
		{
			int unix = System.GetUnixTime();
			if (unix != m_iLastAnchorUnix)
			{
				m_iLastAnchorUnix = unix;
				Push(string.Format("U %1 %2", unix, t));
			}
		}

		Push(line);
	}

	protected void Push(string line)
	{
		m_aLines.Insert(line);

		string type = line;
		int space = line.IndexOf(" ");
		if (space > 0)
			type = line.Substring(0, space);

		int count;
		m_mTypeCounts.Find(type, count);
		m_mTypeCounts.Set(type, count + 1);
	}

	protected bool WriteIdentity_S()
	{
		if (m_aIdentity.IsEmpty())
			return true;

		bool create = !FileIO.FileExists(m_sIdentityPath);
		FileHandle file;
		if (create)
			file = FileIO.OpenFile(m_sIdentityPath, FileMode.WRITE);
		else
			file = FileIO.OpenFile(m_sIdentityPath, FileMode.APPEND);

		if (!file)
		{
			Fail(m_sIdentityPath);
			return true;
		}

		if (create)
		{
			file.Write("LLI 1\n");
			file.Write("H session " + m_sSession + "\n");
		}

		foreach (string line : m_aIdentity)
		{
			file.Write(line + "\n");
		}

		file.Close();
		m_aIdentity.Clear();
		return true;
	}

	protected void Fail(string path)
	{
		if (m_bFailed)
			return;

		m_bFailed = true;
		Print("[LL_Lobby] Replay: cannot write " + path + ", recording stopped", LogLevel.ERROR);
	}
}
