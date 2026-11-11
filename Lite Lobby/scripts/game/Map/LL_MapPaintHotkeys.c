//! Owns keyboard listeners, context activation and temporary Ctrl/Shift modes.
class LL_MapPaintHotkeys
{
	protected LL_MapPaintUI m_Owner;
	protected LL_MapPaintToolState m_ToolState;
	protected LL_MapPaintPanel m_Panel;
	protected InputManager m_Input;
	protected bool m_bTemporaryEraser;
	protected bool m_bTemporaryAnchorTool;
	protected bool m_bInputContextWarningShown;
	protected bool m_bPreviousToolEnabled;
	protected LL_EMapPaintMode m_PreviousToolMode;

	void LL_MapPaintHotkeys(LL_MapPaintUI owner, LL_MapPaintToolState toolState, LL_MapPaintPanel panel)
	{
		m_Owner = owner;
		m_ToolState = toolState;
		m_Panel = panel;
	}

	void ~LL_MapPaintHotkeys()
	{
		Close();
	}

	void Open()
	{
		m_Input = GetGame().GetInputManager();
		m_bInputContextWarningShown = false;
		SetInputListeners(true);
		Update();
	}

	void Close()
	{
		ResetTemporaryTools();
		if (!m_Input)
			return;
		SetInputListeners(false);
		m_Input.ResetContext("LL_MapPaintContext");
		m_Input = null;
	}

	void ResetTemporaryTools()
	{
		m_bTemporaryEraser = false;
		m_bTemporaryAnchorTool = false;
	}

	protected void SetInputListeners(bool enabled)
	{
		InputManager input = m_Input;
		if (enabled)
		{
			input.AddActionListener("LL_MapPaintBrush", EActionTrigger.DOWN, OnBrushHotkey);
			input.AddActionListener("LL_MapPaintLine", EActionTrigger.DOWN, OnLineHotkey);
			input.AddActionListener("LL_MapPaintCircle", EActionTrigger.DOWN, OnCircleHotkey);
			input.AddActionListener("LL_MapPaintColorPicker", EActionTrigger.DOWN, OnColorPickerHotkey);
			input.AddActionListener("LL_MapPaintEraser", EActionTrigger.DOWN, OnEraserHotkeyPressed);
			input.AddActionListener("LL_MapPaintEraser", EActionTrigger.UP, OnEraserHotkeyReleased);
			input.AddActionListener("LL_MapPaintAnchor", EActionTrigger.DOWN, OnAnchorHotkeyPressed);
			input.AddActionListener("LL_MapPaintAnchor", EActionTrigger.UP, OnAnchorHotkeyReleased);
		}
		else
		{
			input.RemoveActionListener("LL_MapPaintBrush", EActionTrigger.DOWN, OnBrushHotkey);
			input.RemoveActionListener("LL_MapPaintLine", EActionTrigger.DOWN, OnLineHotkey);
			input.RemoveActionListener("LL_MapPaintCircle", EActionTrigger.DOWN, OnCircleHotkey);
			input.RemoveActionListener("LL_MapPaintColorPicker", EActionTrigger.DOWN, OnColorPickerHotkey);
			input.RemoveActionListener("LL_MapPaintEraser", EActionTrigger.DOWN, OnEraserHotkeyPressed);
			input.RemoveActionListener("LL_MapPaintEraser", EActionTrigger.UP, OnEraserHotkeyReleased);
			input.RemoveActionListener("LL_MapPaintAnchor", EActionTrigger.DOWN, OnAnchorHotkeyPressed);
			input.RemoveActionListener("LL_MapPaintAnchor", EActionTrigger.UP, OnAnchorHotkeyReleased);
		}
	}

	bool CanUse()
	{
		if (!m_Owner.LL_CanUseToolHotkeys())
			return false;

		// Do not switch tools while entering a marker label, coordinates or chat text.
		Widget focused = GetGame().GetWorkspace().GetFocusedWidget();
		while (focused)
		{
			if (focused.IsVisibleInHierarchy() && (EditBoxWidget.Cast(focused) || MultilineEditBoxWidget.Cast(focused)))
				return false;
			focused = focused.GetParent();
		}
		return true;
	}

	protected void SelectHotkeyMode(LL_EMapPaintMode mode)
	{
		if (!CanUse())
			return;

		m_Panel.SetColorPickerOpen(false);
		if (m_bTemporaryEraser || m_bTemporaryAnchorTool)
		{
			// Change the tool that will be restored without interrupting the held mode.
			bool wasSelected = m_bPreviousToolEnabled && m_PreviousToolMode == mode;
			m_PreviousToolMode = mode;
			m_bPreviousToolEnabled = !wasSelected;
			return;
		}
		if (m_ToolState.IsSelected(mode))
			m_Owner.Disable();
		else
			m_Owner.SetMode(mode);
	}

	protected void OnBrushHotkey(float value, EActionTrigger reason)
	{
		SelectHotkeyMode(LL_EMapPaintMode.BRUSH);
	}

	protected void OnLineHotkey(float value, EActionTrigger reason)
	{
		SelectHotkeyMode(LL_EMapPaintMode.LINE);
	}

	protected void OnCircleHotkey(float value, EActionTrigger reason)
	{
		SelectHotkeyMode(LL_EMapPaintMode.CIRCLE);
	}

	protected void OnColorPickerHotkey(float value, EActionTrigger reason)
	{
		if (!CanUse() || m_bTemporaryEraser || m_bTemporaryAnchorTool)
			return;

		m_Panel.LL_ToggleColorPickerAtCursor();
	}

	protected void OnEraserHotkeyPressed(float value, EActionTrigger reason)
	{
		if (!m_ToolState.m_bEnabled)
			return;

		SetTemporaryMode(LL_EMapPaintMode.ERASER, true);
	}

	protected void OnEraserHotkeyReleased(float value, EActionTrigger reason)
	{
		SetTemporaryMode(LL_EMapPaintMode.ERASER, false);
	}

	protected void OnAnchorHotkeyPressed(float value, EActionTrigger reason)
	{
		SetTemporaryMode(LL_EMapPaintMode.ANCHOR, true);
	}

	protected void OnAnchorHotkeyReleased(float value, EActionTrigger reason)
	{
		SetTemporaryMode(LL_EMapPaintMode.ANCHOR, false);
	}

	protected void SetTemporaryMode(LL_EMapPaintMode mode, bool pressed)
	{
		bool held = m_bTemporaryAnchorTool;
		if (mode == LL_EMapPaintMode.ERASER)
			held = m_bTemporaryEraser;
		if (held == pressed || (pressed && !CanUse()))
			return;
		if (pressed)
		{
			if (!m_bTemporaryEraser && !m_bTemporaryAnchorTool)
			{
				m_PreviousToolMode = m_ToolState.m_SelectedMode;
				m_bPreviousToolEnabled = m_ToolState.m_bEnabled;
			}
			m_Panel.SetColorPickerOpen(false);
		}
		if (mode == LL_EMapPaintMode.ERASER)
			m_bTemporaryEraser = pressed;
		else
			m_bTemporaryAnchorTool = pressed;
		ApplyTemporaryMode();
	}

	protected void ApplyTemporaryMode()
	{
		if (m_bTemporaryEraser)
		{
			m_Owner.SetMode(LL_EMapPaintMode.ERASER);
			return;
		}
		if (m_bTemporaryAnchorTool)
		{
			m_Owner.SetMode(LL_EMapPaintMode.ANCHOR);
			return;
		}

		m_Owner.LL_RestoreToolState(m_PreviousToolMode, m_bPreviousToolEnabled);
	}

	void Update()
	{
		if (!m_Input)
			return;

		bool allowed = CanUse();
		if (allowed)
		{
			bool activated = m_Input.ActivateContext("LL_MapPaintContext");
			if (!activated && !m_bInputContextWarningShown)
			{
				Print("[Lite Lobby] Drawing hotkey context is not loaded. Restart Workbench or the game; reloading scripts alone does not reload the input configuration.", LogLevel.WARNING);
				m_bInputContextWarningShown = true;
			}
		}
		// Also restore after permission/focus changes, when an UP callback may be lost.
		if (!allowed && (m_bTemporaryEraser || m_bTemporaryAnchorTool))
		{
			ResetTemporaryTools();
			ApplyTemporaryMode();
			return;
		}
		if (m_bTemporaryEraser && m_Input.GetActionValue("LL_MapPaintEraser") < 0.5)
			OnEraserHotkeyReleased(0, EActionTrigger.UP);
		if (m_bTemporaryAnchorTool && m_Input.GetActionValue("LL_MapPaintAnchor") < 0.5)
			OnAnchorHotkeyReleased(0, EActionTrigger.UP);
	}
}
