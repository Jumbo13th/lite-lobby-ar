// One clickable row inside an LL_ContextMenu; carries an int payload and fires its own
// invoker on left-click.

void LL_ContextActionCallback(LL_ContextAction action);
typedef func LL_ContextActionCallback;
typedef ScriptInvokerBase<LL_ContextActionCallback> LL_ContextActionInvoker;

class LL_ContextAction : SCR_ButtonBaseComponent
{
	protected ImageWidget m_wIcon;
	protected TextWidget m_wText;

	protected int m_iData = -1;

	protected ref LL_ContextActionInvoker m_OnActivated = new LL_ContextActionInvoker();

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wIcon = ImageWidget.Cast(w.FindAnyWidget("Icon"));
		m_wText = TextWidget.Cast(w.FindAnyWidget("Text"));

		m_OnClicked.Insert(OnLeftClicked);
	}

	// Empty iconSet hides the icon.
	void Init(ResourceName iconSet, string iconQuad, string label, int data)
	{
		m_iData = data;

		if (m_wText)
			m_wText.SetText(label);

		if (m_wIcon)
		{
			if (iconSet != "" && iconQuad != "")
				m_wIcon.LoadImageFromSet(0, iconSet, iconQuad);
			else if (iconSet != "")
				m_wIcon.LoadImageTexture(0, iconSet);
			else
				m_wIcon.SetVisible(false);
		}
	}

	int GetActionData()
	{
		return m_iData;
	}

	LL_ContextActionInvoker GetOnActivated()
	{
		return m_OnActivated;
	}

	protected void OnLeftClicked(SCR_ButtonBaseComponent button)
	{
		m_OnActivated.Invoke(this);
	}
}