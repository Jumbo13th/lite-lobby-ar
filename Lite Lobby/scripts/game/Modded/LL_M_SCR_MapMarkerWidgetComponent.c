// Marker label as a two-line block to the right of the icon (text, then author and age),
// always visible, plus an icon geometry setter the stock widget lacks. The stock layout is
// a vertical stack that cannot be rearranged from script, so the lines are TextWidgets
// created at runtime against the icon size, and the stock text widgets stay hidden.
// Typography is the stock marker's. Rejected: a backing plate (text cannot be measured
// on the frame it changes), a shadow or outline as the main separator, and tinting the
// text with the desaturated marker colour.

modded class SCR_MapMarkerWidgetComponent
{
	// The font the stock marker layout declares; a font named only from script is not
	// guaranteed to resolve, and this one is proven to.
	protected const ResourceName LL_LABEL_FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";

	protected const int LL_TITLE_FONT_SIZE = 16;
	protected const int LL_META_FONT_SIZE = 12;

	// The stock marker layout's own two colours, byte for byte.
	protected const int LL_TITLE_COLOR = 0xFF000000;
	protected const int LL_META_COLOR = 0xFF131313;

	// Runtime SetShadow uses a different scale from a layout's ShadowSize: below about
	// 10 nothing renders.
	protected const int LL_SHADOW_SIZE = 10;

	// Applied in each direction from the icon's centre line.
	protected const float LL_LINE_GAP = 2;

	protected const float LL_LABEL_GAP = 3;
	protected const float LL_DEFAULT_ICON_SIZE = 56;

	protected TextWidget m_wLLTitle;
	protected TextWidget m_wLLMeta;
	protected float m_fLLLabelX;
	protected float m_fLLLabelY;
	protected float m_fLLIconSize = LL_DEFAULT_ICON_SIZE;

	// Fraction of its square the glyph inks, per icon; defaults to the whole square.
	protected float m_fLLIconInk = 1;

	protected string m_sLLAuthor;
	protected string m_sLLText;
	protected string m_sLLTimeSuffix;

	// Tracked rather than read back: the zoom rule and the hover both write m_bShowText.
	protected bool m_bLLHovered;

	//! Set together: a caller that moved only one would place the label against a size
	//! the icon no longer has. pixels <= 0 keeps the stock size; inkFraction 0.1..1.
	void LL_SetIconGeometry(float pixels, float inkFraction)
	{
		if (pixels > 0)
		{
			if (m_wMarkerIcon)
				m_wMarkerIcon.SetSize(pixels, pixels);

			if (m_wMarkerGlowIcon)
				m_wMarkerGlowIcon.SetSize(pixels, pixels);

			m_fLLIconSize = pixels;
		}

		if (inkFraction > 0)
			m_fLLIconInk = Math.Clamp(inkFraction, 0.1, 1);
		else
			m_fLLIconInk = 1;

		LL_PlaceLabels();
	}

	//! "" clears. Driven by the map-wide info key.
	void LL_SetTimeSuffix(string suffix)
	{
		m_sLLTimeSuffix = suffix;
		LL_UpdateLabels();
	}

	protected TextWidget LL_CreateLine(int fontSize, int color)
	{
		// A label must never eat a click meant for the map or the marker under it.
		Widget created = GetGame().GetWorkspace().CreateWidget(
			WidgetType.TextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS,
			Color.White,
			0,
			m_wRoot);

		TextWidget line = TextWidget.Cast(created);
		if (!line)
			return null;

		line.SetFont(LL_LABEL_FONT);
		line.SetExactFontSize(fontSize);

		// Both bold: the lighter weight thins into a different-looking face beside the title.
		line.SetBold(true);
		line.SetColor(Color.FromInt(color));
		line.SetShadow(LL_SHADOW_SIZE);

		// The NOWRAP creation flag is not enough; this property decides it.
		line.SetTextWrapping(false);
		line.SetVisible(false);

		// Anchored to the frame's top-centre, where the icon begins once the stock
		// timestamp row is hidden. Vertical alignment is set per update.
		FrameSlot.SetAnchorMin(line, 0.5, 0);
		FrameSlot.SetAnchorMax(line, 0.5, 0);
		FrameSlot.SetSizeToContent(line, true);

		return line;
	}

	protected void LL_CreateLabels()
	{
		if (m_wLLTitle || m_wLLMeta || !m_wRoot)
			return;

		m_wLLTitle = LL_CreateLine(LL_TITLE_FONT_SIZE, LL_TITLE_COLOR);
		m_wLLMeta = LL_CreateLine(LL_META_FONT_SIZE, LL_META_COLOR);

		// The mode icon reports public/private for always-public markers, and the
		// timestamp row above the icon would push it down.
		if (m_wMarkerModeIcon)
			m_wMarkerModeIcon.SetVisible(false);

		if (m_wMarkerTimestamp)
			m_wMarkerTimestamp.SetVisible(false);

		LL_PlaceLabels();
	}

	//! One left edge past the glyph's ink; Y is the icon box's centre.
	protected void LL_PlaceLabels()
	{
		float half = m_fLLIconSize * 0.5;

		m_fLLLabelX = half * m_fLLIconInk + LL_LABEL_GAP;
		m_fLLLabelY = half;

		LL_UpdateLabels();
	}

	//! Re-asserted on every update because the stock hover handlers show them again.
	protected void LL_HideStockLabels()
	{
		if (m_wMarkerText)
			m_wMarkerText.SetVisible(false);

		if (m_wMarkerAuthor)
			m_wMarkerAuthor.SetVisible(false);

		if (m_wAuthorPlatformIcon)
			m_wAuthorPlatformIcon.SetVisible(false);
	}

	//! Single writer for content, visibility and stacking, so the last event always
	//! wins with the full picture.
	protected void LL_UpdateLabels()
	{
		LL_HideStockLabels();

		if (!m_wLLTitle || !m_wLLMeta)
			return;

		// A marker hidden under the cursor may never receive its mouse-leave.
		if (m_bLLHovered && m_wRoot && !m_wRoot.IsVisible())
			m_bLLHovered = false;

		// The age goes in brackets as an aside: "[TAG] Callsign (2 minutes ago)".
		string meta = m_sLLAuthor;

		if (!m_sLLTimeSuffix.IsEmpty())
		{
			if (meta.IsEmpty())
				meta = "(" + m_sLLTimeSuffix + ")";
			else
				meta = meta + " (" + m_sLLTimeSuffix + ")";
		}

		// Format parameters, never finished strings: SetText translates anything starting with '#'.
		m_wLLTitle.SetTextFormat("%1", m_sLLText);
		m_wLLMeta.SetTextFormat("%1", meta);

		// The zoom rule is the stock one.
		bool show = m_bShowText || m_bLLHovered;

		bool hasTitle = show && !m_sLLText.IsEmpty();
		bool hasMeta = show && !meta.IsEmpty();

		m_wLLTitle.SetVisible(hasTitle);
		m_wLLMeta.SetVisible(hasMeta);

		// Stacked by alignment rather than measurement: the title anchors by its bottom
		// edge and the metadata by its top, either side of the icon's centre line.
		if (hasTitle && hasMeta)
		{
			FrameSlot.SetAlignment(m_wLLTitle, 0, 1);
			FrameSlot.SetAlignment(m_wLLMeta, 0, 0);

			FrameSlot.SetPos(m_wLLTitle, m_fLLLabelX, m_fLLLabelY - LL_LINE_GAP);
			FrameSlot.SetPos(m_wLLMeta, m_fLLLabelX, m_fLLLabelY + LL_LINE_GAP);
		}
		else
		{
			FrameSlot.SetAlignment(m_wLLTitle, 0, 0.5);
			FrameSlot.SetAlignment(m_wLLMeta, 0, 0.5);

			FrameSlot.SetPos(m_wLLTitle, m_fLLLabelX, m_fLLLabelY);
			FrameSlot.SetPos(m_wLLMeta, m_fLLLabelX, m_fLLLabelY);
		}
	}

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		LL_CreateLabels();
	}

	//! Stock version records the flag and hides the widget unconditionally.
	override void SetTextVisible(bool state)
	{
		m_bShowText = state;
		LL_UpdateLabels();
	}

	//! Stock wraps the name in brackets, which doubled community clan tags; the stock
	//! author widget still gets it but stays hidden.
	override void SetAuthor(string text)
	{
		super.SetAuthor(text);

		m_sLLAuthor = text;
		LL_UpdateLabels();
	}

	//! Not redundant with OnFilteredCallback: stock SetText returns without invoking it
	//! when the marker manager is missing.
	override void SetText(string text, bool skipProfanityFilter = false)
	{
		super.SetText(text, skipProfanityFilter);

		m_sLLText = text;
		LL_UpdateLabels();
	}

	//! The filter answers asynchronously; the label must show the filtered string.
	override protected void OnFilteredCallback(array<string> text)
	{
		super.OnFilteredCallback(text);

		if (text && !text.IsEmpty() && !text[0].IsEmpty())
			m_sLLText = text[0];

		LL_UpdateLabels();
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		m_bLLHovered = true;

		bool handled = super.OnMouseEnter(w, x, y);

		LL_UpdateLabels();
		return handled;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		m_bLLHovered = false;

		bool handled = super.OnMouseLeave(w, enterW, x, y);

		LL_UpdateLabels();
		return handled;
	}
}