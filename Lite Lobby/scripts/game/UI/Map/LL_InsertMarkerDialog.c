// Compact insert-marker dialog at the cursor: description, type and colour with a live
// preview. Pure UI: it reports (icon entry, colour entry, text) through GetOnConfirm and
// GetOnCancel; construction and networking stay in LL_MapMarkersUI. Entries are indices
// into the shared SCR_MapMarkerEntryPlaced config. Dropdown row icons are applied from
// m_OnOpened because the combo rebuilds its elements on every open.

void LL_InsertMarkerConfirmInvoker(int iconEntry, int colorEntry, string text);
typedef func LL_InsertMarkerConfirmInvoker;

class LL_InsertMarkerDialog : SCR_ScriptedWidgetComponent
{
	// Fixed size: content-driven sizing mismeasured the late-sizing WLib buttons.
	protected const int DIALOG_WIDTH = 360;
	protected const int DIALOG_HEIGHT = 338;

	// Dropdown lists open downward past the dialog's bottom edge.
	protected const int DROPDOWN_RESERVE = 210;

	protected SCR_MapMarkerEntryPlaced m_Config;

	protected ImageWidget m_wPreviewIcon;
	protected TextWidget m_wPreviewText;
	protected SCR_EditBoxComponent m_EditBox;
	protected SCR_ComboBoxComponent m_TypeCombo;
	protected SCR_ComboBoxComponent m_ColorCombo;
	protected SCR_ButtonTextComponent m_ButtonOK;
	protected SCR_ButtonTextComponent m_ButtonCancel;

	protected ref ScriptInvokerBase<LL_InsertMarkerConfirmInvoker> m_OnConfirm = new ScriptInvokerBase<LL_InsertMarkerConfirmInvoker>();
	protected ref ScriptInvokerVoid m_OnCancel = new ScriptInvokerVoid();

	// A dropdown or the edit box consumes Escape in the same frame the cancel listener
	// fires, and listener order is not guaranteed.
	protected const float ESCAPE_CONSUMED_GRACE_MS = 150;
	protected float m_fEscapeConsumedTime = -float.MAX;

	ScriptInvokerBase<LL_InsertMarkerConfirmInvoker> GetOnConfirm()
	{
		return m_OnConfirm;
	}

	ScriptInvokerVoid GetOnCancel()
	{
		return m_OnCancel;
	}

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wPreviewIcon = ImageWidget.Cast(w.FindAnyWidget("PreviewIcon"));
		m_wPreviewText = TextWidget.Cast(w.FindAnyWidget("PreviewText"));

		Widget editBoxRoot = w.FindAnyWidget("EditBoxRoot");
		if (editBoxRoot)
		{
			m_EditBox = SCR_EditBoxComponent.Cast(editBoxRoot.FindHandler(SCR_EditBoxComponent));
			if (m_EditBox)
			{
				m_EditBox.m_OnTextChange.Insert(OnTextChanged);
				m_EditBox.m_OnCancel.Insert(MarkEscapeConsumed);
				m_EditBox.m_OnWriteModeLeave.Insert(OnDescriptionWriteModeLeave);
			}
		}

		Widget combo = w.FindAnyWidget("TypeCombo");
		if (combo)
		{
			m_TypeCombo = SCR_ComboBoxComponent.Cast(combo.FindHandler(SCR_ComboBoxComponent));
			if (m_TypeCombo)
			{
				m_TypeCombo.m_OnChanged.Insert(OnSelectionChanged);
				m_TypeCombo.m_OnOpened.Insert(OnTypeComboOpened);
				m_TypeCombo.m_OnClosed.Insert(OnComboClosed);
			}
		}

		combo = w.FindAnyWidget("ColorCombo");
		if (combo)
		{
			m_ColorCombo = SCR_ComboBoxComponent.Cast(combo.FindHandler(SCR_ComboBoxComponent));
			if (m_ColorCombo)
			{
				m_ColorCombo.m_OnChanged.Insert(OnSelectionChanged);
				m_ColorCombo.m_OnOpened.Insert(OnColorComboOpened);
				m_ColorCombo.m_OnClosed.Insert(OnComboClosed);
			}
		}

		Widget button = w.FindAnyWidget("ButtonOK");
		if (button)
		{
			m_ButtonOK = SCR_ButtonTextComponent.Cast(button.FindHandler(SCR_ButtonTextComponent));
			if (m_ButtonOK)
				m_ButtonOK.m_OnClicked.Insert(OnOKClicked);
		}

		button = w.FindAnyWidget("ButtonCancel");
		if (button)
		{
			m_ButtonCancel = SCR_ButtonTextComponent.Cast(button.FindHandler(SCR_ButtonTextComponent));
			if (m_ButtonCancel)
				m_ButtonCancel.m_OnClicked.Insert(OnCancelClicked);
		}
	}

	//! Preselects the given entries (0/0/"" for a fresh marker).
	void Init(notnull SCR_MapMarkerEntryPlaced config, int iconEntry, int colorEntry, string text)
	{
		m_Config = config;

		if (m_TypeCombo)
		{
			int iconCount;
			LL_MapMarkerEntryPlaced llConfig = LL_MapMarkerEntryPlaced.Cast(config);
			if (llConfig && llConfig.GetLLIcons())
			{
				array<ref LL_MarkerIconEntry> icons = llConfig.GetLLIcons();
				iconCount = icons.Count();
				foreach (LL_MarkerIconEntry icon : icons)
				{
					m_TypeCombo.AddItem(icon.GetDisplayName());
				}
			}

			if (iconEntry >= 0 && iconEntry < iconCount)
				m_TypeCombo.SetCurrentItem(iconEntry);
			else
				m_TypeCombo.SetCurrentItem(0);
		}

		if (m_ColorCombo)
		{
			array<ref SCR_MarkerColorEntry> colors = config.GetColorEntries();
			foreach (SCR_MarkerColorEntry color : colors)
			{
				m_ColorCombo.AddItem(color.GetName());
			}

			if (colorEntry >= 0 && colorEntry < colors.Count())
				m_ColorCombo.SetCurrentItem(colorEntry);
			else
				m_ColorCombo.SetCurrentItem(0);
		}

		if (m_EditBox)
			m_EditBox.SetValue(text);

		UpdatePreview();

		// First controller focus lands on the type dropdown, as in the stock dialog.
		if (m_TypeCombo)
			GetGame().GetWorkspace().SetFocusedWidget(m_TypeCombo.GetRootWidget());

		GetGame().GetCallqueue().Call(ClampOnScreen);
	}

	//! Console users without the UGC privilege may not share content.
	void SetConfirmEnabled(bool enabled)
	{
		if (m_ButtonOK)
			m_ButtonOK.SetEnabled(enabled);
	}

	int GetSelectedIconEntry()
	{
		if (!m_TypeCombo)
			return 0;

		return m_TypeCombo.GetCurrentIndex();
	}

	int GetSelectedColorEntry()
	{
		if (!m_ColorCombo)
			return 0;

		return m_ColorCombo.GetCurrentIndex();
	}

	string GetDescription()
	{
		if (!m_EditBox)
			return string.Empty;

		return m_EditBox.GetValue();
	}

	bool IsComboOpened()
	{
		return (m_TypeCombo && m_TypeCombo.IsOpened()) || (m_ColorCombo && m_ColorCombo.IsOpened());
	}

	//! Escape exits typing; it must not close the dialog.
	bool IsEditingText()
	{
		return m_EditBox && m_EditBox.IsInWriteMode();
	}

	//! True if a dropdown or the edit box swallowed Escape within the grace window.
	bool WasEscapeJustConsumed()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		return world.GetWorldTime() - m_fEscapeConsumedTime < ESCAPE_CONSUMED_GRACE_MS;
	}

	//! Fixed geometry, so a rect test.
	bool IsCursorInside()
	{
		if (!m_wRoot)
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);

		float x = workspace.DPIUnscale(mouseX);
		float y = workspace.DPIUnscale(mouseY);
		float posX = FrameSlot.GetPosX(m_wRoot);
		float posY = FrameSlot.GetPosY(m_wRoot);

		return x >= posX && x <= posX + DIALOG_WIDTH && y >= posY && y <= posY + DIALOG_HEIGHT;
	}

	bool IsComboFocused()
	{
		Widget focused = GetGame().GetWorkspace().GetFocusedWidget();
		if (!focused)
			return false;

		if (m_TypeCombo && m_TypeCombo.GetRootWidget().FindAnyWidget("ComboButton") == focused)
			return true;

		if (m_ColorCombo && m_ColorCombo.GetRootWidget().FindAnyWidget("ComboButton") == focused)
			return true;

		return false;
	}

	void Confirm()
	{
		m_OnConfirm.Invoke(GetSelectedIconEntry(), GetSelectedColorEntry(), GetDescription());
	}

	void Cancel()
	{
		m_OnCancel.Invoke();
	}

	protected void OnOKClicked(SCR_ButtonTextComponent button)
	{
		Confirm();
	}

	protected void OnCancelClicked(SCR_ButtonTextComponent button)
	{
		Cancel();
	}

	protected void OnTextChanged(string text)
	{
		UpdatePreview();
	}

	protected void MarkEscapeConsumed()
	{
		// The edit box fires m_OnWriteModeLeave once more while the dialog widget is being
		// destroyed, when the world can already be gone.
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_fEscapeConsumedTime = world.GetWorldTime();
	}

	// Not named OnWriteModeLeave: that clashes with the built-in widget event.
	protected void OnDescriptionWriteModeLeave(string text)
	{
		MarkEscapeConsumed();
	}

	protected void OnComboClosed(SCR_ComboBoxComponent combo)
	{
		MarkEscapeConsumed();
	}

	protected void OnSelectionChanged(SCR_ComboBoxComponent combo, int index)
	{
		UpdatePreview();
	}

	//! Paint each type row with its glyph, tinted in the currently selected color.
	protected void OnTypeComboOpened(SCR_ComboBoxComponent combo)
	{
		if (!m_Config)
			return;

		Color color = m_Config.GetColorEntry(GetSelectedColorEntry());

		array<Widget> elements = {};
		combo.GetElementWidgets(elements);

		foreach (int i, Widget element : elements)
		{
			ImageWidget icon = ImageWidget.Cast(element.FindAnyWidget("Icon"));
			if (!icon)
				continue;

			ResourceName imageset, glow;
			string quad;
			if (!m_Config.GetIconEntry(i, imageset, glow, quad))
				continue;

			icon.LoadImageFromSet(0, imageset, quad);
			icon.SetColor(color);
		}
	}

	//! Each colour row shows the selected glyph in that colour.
	protected void OnColorComboOpened(SCR_ComboBoxComponent combo)
	{
		if (!m_Config)
			return;

		ResourceName imageset, glow;
		string quad;
		if (!m_Config.GetIconEntry(GetSelectedIconEntry(), imageset, glow, quad))
			return;

		array<Widget> elements = {};
		combo.GetElementWidgets(elements);

		foreach (int i, Widget element : elements)
		{
			ImageWidget icon = ImageWidget.Cast(element.FindAnyWidget("Icon"));
			if (!icon)
				continue;

			icon.LoadImageFromSet(0, imageset, quad);
			icon.SetColor(m_Config.GetColorEntry(i));
		}
	}

	protected void UpdatePreview()
	{
		if (!m_Config)
			return;

		if (m_wPreviewIcon)
		{
			ResourceName imageset, glow;
			string quad;
			if (m_Config.GetIconEntry(GetSelectedIconEntry(), imageset, glow, quad))
			{
				m_wPreviewIcon.LoadImageFromSet(0, imageset, quad);
				m_wPreviewIcon.SetColor(m_Config.GetColorEntry(GetSelectedColorEntry()));
			}
		}

		if (m_wPreviewText)
		{
			string text = GetDescription();
			m_wPreviewText.SetText(text);
			m_wPreviewText.SetVisible(text != string.Empty);
		}
	}

	//! Runs one frame after creation, before real bounds are measured; the fixed size
	//! gives a reliable bound.
	protected void ClampOnScreen()
	{
		if (!m_wRoot)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		int x = FrameSlot.GetPosX(m_wRoot);
		int y = FrameSlot.GetPosY(m_wRoot);

		float sw, sh;
		workspace.GetScreenSize(sw, sh);
		sw = workspace.DPIUnscale(sw);
		sh = workspace.DPIUnscale(sh);

		if (x + DIALOG_WIDTH > sw)
			x = sw - DIALOG_WIDTH;
		if (y + DIALOG_HEIGHT + DROPDOWN_RESERVE > sh)
			y = sh - DIALOG_HEIGHT - DROPDOWN_RESERVE;
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;

		FrameSlot.SetPos(m_wRoot, x, y);
	}
}