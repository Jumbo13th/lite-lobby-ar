// Minimal rich-text renderer with clickable links: the engine's RichTextWidget cannot
// do links and there is no flow layout mixing text and buttons. One TextWidget per word,
// laid out manually in two passes: pass 1 creates the words with SizeToContent, pass 2
// (deferred, polled) reads each realised box back through GetScreenSize, because
// GetTextSize under-measures words ending in some glyphs and a fresh widget reads 0.
// Markup: <h>, <b>, <color=r,g,b[,a]>, <link=Name>, <br/>, <hr/>, <gap/>; unknown tags
// are stripped. Client-side UI only.

class LL_RichTextRun
{
	string m_sText;
	bool m_bBreak;
	bool m_bRule;
	bool m_bGap;
	string m_sFocusName;
	bool m_bBold;
	int m_iColor;
	int m_iFontSize;
}

class LL_RichTextWord
{
	Widget m_wText;
	Widget m_wButton;
	Widget m_wUnderline;
	bool m_bBreak;
	bool m_bRule;
	bool m_bGap;
	bool m_bLinkCont;
	int m_iLeadingSpaces;
}

class LL_RichTextLinkHandler : ScriptedWidgetComponent
{
	protected string m_sFocusName;

	void Setup(string focusName)
	{
		m_sFocusName = focusName;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		// "@x,z" is a fixed world position; anything else a named entity.
		if (m_sFocusName.StartsWith("@"))
		{
			string coords = m_sFocusName.Substring(1, m_sFocusName.Length() - 1);
			array<string> parts = {};
			coords.Split(",", parts, true);
			if (parts.Count() == 2)
				LL_MapFocus.ToPos(parts[0].ToFloat(), parts[1].ToFloat());
		}
		else
		{
			LL_MapFocus.To(m_sFocusName);
		}
		return true;
	}
}

class LL_RichTextUI
{
	protected static const ResourceName FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int FONT_SIZE = 20;
	protected static const int HEADER_FONT_SIZE = 23;
	protected static const float SPACE = 6;
	protected static const float FALLBACK_LINE_HEIGHT = 24;
	protected static const float UNDERLINE_THICKNESS = 2;
	protected static const float UNDERLINE_GAP = 3;

	protected static const float DIVIDER_THICKNESS = 1;
	protected static const float DIVIDER_ROW_HEIGHT = 28;
	protected static const float DIVIDER_PAD = 14;
	protected static const float GAP_HEIGHT = 8;

	// ARGB, rendered through Color.FromSRGBA so widget colours match RichText's
	// <color rgba> path; FromInt skips the sRGB curve and washes mid tones.
	protected static const int COL_TEXT = 0xFFE6E6E6;
	protected static const int COL_HEADER = 0xFFFFFFFF;
	protected static const int COL_LINK = 0xFFE2A74F;
	protected static const int COL_DIVIDER = 0x3CE2A74F;

	// Keeps each pending instance alive until its deferred layout runs.
	protected static ref array<ref LL_RichTextUI> s_Pending = {};

	protected static const int MAX_LAYOUT_TRIES = 30;

	protected Widget m_wFrame;
	protected float m_fMaxWidth;
	protected ref array<ref LL_RichTextWord> m_aWords = {};
	protected int m_iLayoutTries;
	protected float m_fLineHeight = FALLBACK_LINE_HEIGHT;

	static void Render(notnull Widget container, string markup, float maxWidth)
	{
		LL_RichTextUI inst = new LL_RichTextUI();
		s_Pending.Insert(inst);
		inst.Build(container, markup, maxWidth);
	}

	protected static Color SRGB(int argb)
	{
		int a = (argb >> 24) & 0xFF;
		int r = (argb >> 16) & 0xFF;
		int g = (argb >> 8) & 0xFF;
		int b = argb & 0xFF;
		return Color.FromSRGBA(r, g, b, a);
	}

	protected void Build(Widget container, string markup, float maxWidth)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return;

		// INHERIT_CLIPPING so a scrollable container's CLIPCHILDREN clips the subtree.
		m_wFrame = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.INHERIT_CLIPPING, Color.FromInt(0x00000000), 0, container);
		m_fMaxWidth = maxWidth;
		if (!m_wFrame)
			return;

		// A ScrollLayoutWidget centres its child by default, which shifted short-lined
		// content right. The slot is an AlignableSlot, not a FrameSlot.
		AlignableSlot.SetHorizontalAlign(m_wFrame, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(m_wFrame, LayoutVerticalAlign.Top);

		CreateWords(ws, markup);

		GetGame().GetCallqueue().CallLater(DoLayout, 0, false);
	}

	protected void CreateWords(WorkspaceWidget ws, string markup)
	{
		array<ref LL_RichTextRun> runs = {};
		Parse(markup, runs);

		int pendingSpaces = 0;
		foreach (LL_RichTextRun run : runs)
		{
			if (run.m_bBreak)
			{
				LL_RichTextWord br = new LL_RichTextWord();
				br.m_bBreak = true;
				m_aWords.Insert(br);
				pendingSpaces = 0;
				continue;
			}

			if (run.m_bRule)
			{
				LL_RichTextWord rw = new LL_RichTextWord();
				rw.m_bRule = true;
				rw.m_wText = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.INHERIT_CLIPPING, SRGB(COL_DIVIDER), 0, m_wFrame);
				m_aWords.Insert(rw);
				pendingSpaces = 0;
				continue;
			}

			if (run.m_bGap)
			{
				LL_RichTextWord gap = new LL_RichTextWord();
				gap.m_bGap = true;
				m_aWords.Insert(gap);
				pendingSpaces = 0;
				continue;
			}

			bool isLink = run.m_sFocusName != "";
			bool firstWordOfRun = true;

			string txt = run.m_sText;
			int len = txt.Length();
			int i = 0;
			while (i < len)
			{
				// A newline (author pressed Enter) becomes a line break; a carriage return is ignored.
				while (i < len)
				{
					string c = txt.Get(i);
					if (c == " ")
						pendingSpaces++;
					else if (c == "\n")
					{
						LL_RichTextWord nl = new LL_RichTextWord();
						nl.m_bBreak = true;
						m_aWords.Insert(nl);
						pendingSpaces = 0;
						firstWordOfRun = true;
					}
					else if (c != "\r")
						break;
					i++;
				}
				if (i >= len)
					break;

				int start = i;
				while (i < len && txt.Get(i) != " " && txt.Get(i) != "\n" && txt.Get(i) != "\r")
					i++;
				string word = txt.Substring(start, i - start);

				LL_RichTextWord lw = new LL_RichTextWord();
				lw.m_iLeadingSpaces = pendingSpaces;
				lw.m_bLinkCont = isLink && !firstWordOfRun;
				firstWordOfRun = false;
				pendingSpaces = 0;

				Widget t = ws.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.INHERIT_CLIPPING, SRGB(run.m_iColor), 0, m_wFrame);
				TextWidget tw = TextWidget.Cast(t);
				if (tw)
				{
					tw.SetFont(FONT);
					tw.SetExactFontSize(run.m_iFontSize);
					tw.SetBold(run.m_bBold);
					tw.SetText(word);
				}
				FrameSlot.SetSizeToContent(t, true);
				lw.m_wText = t;

				if (isLink)
				{
					Widget ul = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.INHERIT_CLIPPING, SRGB(COL_LINK), 1, m_wFrame);
					lw.m_wUnderline = ul;

					Widget btn = ws.CreateWidget(WidgetType.ButtonWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.INHERIT_CLIPPING, Color.FromInt(0x00000000), 2, m_wFrame);
					LL_RichTextLinkHandler h = new LL_RichTextLinkHandler();
					h.Setup(run.m_sFocusName);
					btn.AddHandler(h);
					lw.m_wButton = btn;
				}

				m_aWords.Insert(lw);
			}
		}
	}

	// True once the first real word reports a rendered width.
	protected bool WordsMeasurable()
	{
		foreach (LL_RichTextWord lw : m_aWords)
		{
			if (lw.m_bBreak || lw.m_bRule || !lw.m_wText)
				continue;
			float sx = 0;
			float sy = 0;
			lw.m_wText.GetScreenSize(sx, sy);
			return sx > 0;
		}
		return true;
	}

	protected void DoLayout()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws || !m_wFrame)
		{
			s_Pending.RemoveItem(this);
			return;
		}

		if (!WordsMeasurable() && m_iLayoutTries < MAX_LAYOUT_TRIES)
		{
			m_iLayoutTries++;
			GetGame().GetCallqueue().CallLater(DoLayout, 0, false);
			return;
		}

		s_Pending.RemoveItem(this);

		float x = 0;
		float y = 0;
		float curLineH = 0;

		foreach (LL_RichTextWord lw : m_aWords)
		{
			if (lw.m_bBreak)
			{
				y += LineAdvance(curLineH);
				curLineH = 0;
				x = 0;
				continue;
			}

			if (lw.m_bGap)
			{
				// Closes the current line (0 if empty, no phantom blank) and adds the gap.
				y += curLineH + GAP_HEIGHT;
				curLineH = 0;
				x = 0;
				continue;
			}

			if (lw.m_bRule)
			{
				// The divider provides its own spacing, so no phantom blank line before it.
				y += curLineH;
				curLineH = 0;
				x = 0;
				if (lw.m_wText)
				{
					FrameSlot.SetAlignment(lw.m_wText, 0, 0);
					FrameSlot.SetPos(lw.m_wText, 0, y + DIVIDER_PAD);
					FrameSlot.SetSize(lw.m_wText, m_fMaxWidth, DIVIDER_THICKNESS);
				}
				y += DIVIDER_ROW_HEIGHT;
				continue;
			}

			if (!lw.m_wText)
				continue;

			// GetTextSize fallback only if the widget never realised, so words never all
			// stack at x=0.
			float sx = 0;
			float sy = 0;
			lw.m_wText.GetScreenSize(sx, sy);
			if (sx <= 0)
			{
				TextWidget tw = TextWidget.Cast(lw.m_wText);
				if (tw)
					tw.GetTextSize(sx, sy);
			}
			float w = ws.DPIUnscale(sx);
			float h = ws.DPIUnscale(sy);
			if (h <= 0)
				h = FALLBACK_LINE_HEIGHT;

			if (sy > 0 && h <= FONT_SIZE + 6)
				m_fLineHeight = h;

			// Leading spaces become inter-word gaps and, at a line start, indentation. A
			// wrap discards them.
			float adv = lw.m_iLeadingSpaces * SPACE;

			bool wrapped = false;
			if (x > 0 && x + adv + w > m_fMaxWidth)
			{
				y += LineAdvance(curLineH);
				curLineH = 0;
				x = 0;
				wrapped = true;
			}
			else
			{
				x += adv;
			}

			FrameSlot.SetAlignment(lw.m_wText, 0, 0);
			FrameSlot.SetPos(lw.m_wText, x, y);

			if (lw.m_wUnderline)
			{
				// One continuous underline per link: a continuation word extends its bar over the gap.
				float ulX = x;
				float ulW = w;
				if (lw.m_bLinkCont && !wrapped)
				{
					ulX = x - adv;
					ulW = w + adv;
				}
				FrameSlot.SetAlignment(lw.m_wUnderline, 0, 0);
				FrameSlot.SetPos(lw.m_wUnderline, ulX, y + h - UNDERLINE_GAP);
				FrameSlot.SetSize(lw.m_wUnderline, ulW, UNDERLINE_THICKNESS);
			}
			if (lw.m_wButton)
			{
				FrameSlot.SetAlignment(lw.m_wButton, 0, 0);
				FrameSlot.SetPos(lw.m_wButton, x, y);
				FrameSlot.SetSize(lw.m_wButton, w, h);
			}

			if (h > curLineH)
				curLineH = h;
			x += w;
		}

		y += LineAdvance(curLineH);
		FrameSlot.SetSize(m_wFrame, m_fMaxWidth, y);
	}

	protected float LineAdvance(float curLineH)
	{
		if (curLineH > 0)
			return curLineH;
		return m_fLineHeight;
	}

	void Parse(string markup, notnull array<ref LL_RichTextRun> runs)
	{
		string s = markup;
		while (s.Length() > 0)
		{
			int lt = s.IndexOf("<");
			if (lt < 0)
			{
				AddText(runs, s);
				return;
			}

			if (lt > 0)
				AddText(runs, s.Substring(0, lt));

			string rest = s.Substring(lt, s.Length() - lt);
			int gt = rest.IndexOf(">");
			if (gt < 0)
			{
				AddText(runs, rest);
				return;
			}

			string tag = rest.Substring(1, gt - 1);
			string after = rest.Substring(gt + 1, rest.Length() - gt - 1);

			if (tag == "br/" || tag == "br")
			{
				LL_RichTextRun br = new LL_RichTextRun();
				br.m_bBreak = true;
				runs.Insert(br);
				s = after;
			}
			else if (tag == "hr/" || tag == "hr")
			{
				LL_RichTextRun rule = new LL_RichTextRun();
				rule.m_bRule = true;
				runs.Insert(rule);
				// A tag written on its own source line must not add a blank line.
				s = ConsumeLeadingNewline(after);
			}
			else if (tag == "gap/" || tag == "gap")
			{
				LL_RichTextRun gap = new LL_RichTextRun();
				gap.m_bGap = true;
				runs.Insert(gap);
				s = ConsumeLeadingNewline(after);
			}
			else if (tag.StartsWith("link="))
			{
				string focus = tag.Substring(5, tag.Length() - 5);
				s = AddSpan(runs, after, "</link>", focus, false, COL_LINK, FONT_SIZE);
			}
			else if (tag == "h")
			{
				// Block-level: own line, following text on the next.
				EnsureLineStart(runs);
				s = AddSpan(runs, after, "</h>", "", true, COL_HEADER, HEADER_FONT_SIZE);
				AppendBreak(runs);
				s = ConsumeLeadingNewline(s);
			}
			else if (tag == "b")
			{
				s = AddSpan(runs, after, "</b>", "", true, COL_TEXT, FONT_SIZE);
			}
			else if (tag.StartsWith("color="))
			{
				int col = ParseColorSpec(tag.Substring(6, tag.Length() - 6));
				s = AddSpan(runs, after, "</color>", "", false, col, FONT_SIZE);
			}
			else
			{
				s = after;
			}
		}
	}

	// Drops one leading line ending, so a block tag on its own source line does not stack
	// the author's Enter on the break it already provides.
	protected string ConsumeLeadingNewline(string s)
	{
		int len = s.Length();
		int i = 0;
		if (i < len && s.Get(i) == "\r")
			i++;
		if (i < len && s.Get(i) == "\n")
			return s.Substring(i + 1, len - i - 1);
		return s;
	}

	// A break unless already at a line start.
	protected void EnsureLineStart(array<ref LL_RichTextRun> runs)
	{
		if (runs.IsEmpty())
			return;
		LL_RichTextRun last = runs[runs.Count() - 1];
		if (last.m_bBreak || last.m_bRule)
			return;
		AppendBreak(runs);
	}

	protected void AppendBreak(array<ref LL_RichTextRun> runs)
	{
		LL_RichTextRun br = new LL_RichTextRun();
		br.m_bBreak = true;
		runs.Insert(br);
	}

	// "r,g,b" or "r,g,b,a" (0-255) -> packed ARGB int. Missing alpha = opaque.
	protected int ParseColorSpec(string spec)
	{
		array<string> parts = {};
		spec.Split(",", parts, true);
		int r = 255, g = 255, b = 255, a = 255;
		if (parts.Count() >= 3)
		{
			r = parts[0].ToInt();
			g = parts[1].ToInt();
			b = parts[2].ToInt();
		}
		if (parts.Count() >= 4)
			a = parts[3].ToInt();
		return (a << 24) | (r << 16) | (g << 8) | b;
	}

	protected string AddSpan(array<ref LL_RichTextRun> runs, string after, string closeTag, string focus, bool bold, int color, int fontSize)
	{
		int close = after.IndexOf(closeTag);
		string inner = after;
		string remaining = "";
		if (close >= 0)
		{
			inner = after.Substring(0, close);
			remaining = after.Substring(close + closeTag.Length(), after.Length() - close - closeTag.Length());
		}

		LL_RichTextRun run = new LL_RichTextRun();
		run.m_sText = inner;
		run.m_sFocusName = focus;
		run.m_bBold = bold;
		run.m_iColor = color;
		run.m_iFontSize = fontSize;
		runs.Insert(run);
		return remaining;
	}

	protected void AddText(notnull array<ref LL_RichTextRun> runs, string text)
	{
		if (text == "")
			return;
		LL_RichTextRun run = new LL_RichTextRun();
		run.m_sText = text;
		run.m_iColor = COL_TEXT;
		run.m_iFontSize = FONT_SIZE;
		runs.Insert(run);
	}
}