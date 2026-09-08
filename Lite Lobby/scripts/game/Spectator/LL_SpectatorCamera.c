// Client-local free-fly spectator camera on the vanilla SCR_ManualCamera pipeline, with
// follow-a-character support. The prefab must have no RplComponent.

// Vanilla disables manual-camera input whenever any menu but the editor's is on top;
// the spectator menu must keep the camera flying underneath it.
modded class SCR_ManualCamera
{
	override protected bool IsDisabledByMenu()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return false;

		if (menuManager.IsAnyDialogOpen())
			return true;

		// The spectator map claims the same mouse: a drag would pan the map and swing
		// the camera.
		if (LL_SpectatorMenu.IsMapOpen())
			return true;

		MenuBase topMenu = menuManager.GetTopMenu();
		return topMenu && !topMenu.IsInherited(EditorMenuUI) && !topMenu.IsInherited(LL_SpectatorMenu);
	}
}

// The speed multiplier has no public setter. The decorator and base are repeated on
// purpose: manual-camera components are BaseContainerProps script objects, and a bare
// modded class drops the container registration, so prefabs fail to deserialise it and
// every later component in the list.
[BaseContainerProps(), SCR_BaseManualCameraComponentTitle()]
modded class SCR_AdjustSpeedManualCameraComponent : SCR_BaseManualCameraComponent
{
	void LL_ResetSpeed()
	{
		m_fMultiplier = 1;
		m_OnSpeedChange.Invoke(m_fMultiplier, true);
	}
}

class LL_SpectatorCameraClass : SCR_ManualCameraClass
{
}

class LL_SpectatorCamera : SCR_ManualCamera
{
	override protected void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Opacity 0: SCR_AttachManualCameraComponent re-shows its "to detach" hint every
		// frame while following, and deleting the widget would leave a dangling ref.
		Widget cameraWidget = GetWidget();
		if (cameraWidget)
		{
			Widget attachHint = cameraWidget.FindAnyWidget("ManualCameraFocus");
			if (attachHint)
				attachHint.SetOpacity(0);
		}

		// SCR_PostProcessCameraComponent re-creates its effects each time the camera
		// becomes current; the camera already activated during init.
		GetOnCameraActivate().Insert(DisableDepthOfField);
		DisableDepthOfField();
	}

	// The inherited prefab carries a depth-of-field post-process; a spectator wants a
	// sharp view.
	protected void DisableDepthOfField()
	{
		SCR_PostProcessCameraComponent postProcess = SCR_PostProcessCameraComponent.Cast(
			FindComponent(SCR_PostProcessCameraComponent));
		if (!postProcess)
			return;

		SCR_CameraPostProcessEffect effect = postProcess.FindEffect(PostProcessEffectType.DepthOfFieldBokeh);
		if (effect)
			effect.DeleteEffect();

		effect = postProcess.FindEffect(PostProcessEffectType.DepthOfField);
		if (effect)
			effect.DeleteEffect();
	}

	override protected void EOnPostFrame(IEntity owner, float timeSlice)
	{
		super.EOnPostFrame(owner, timeSlice);

		// When another camera takes the view this one stops processing frames, and
		// whatever its components last drew would freeze on screen.
		Widget cameraWidget = GetWidget();
		if (cameraWidget)
		{
			CameraManager cameraManager = GetGame().GetCameraManager();
			cameraWidget.SetVisible(!cameraManager || cameraManager.CurrentCamera() == this);
		}
	}

	// SCR_AttachManualCameraComponent follows position only, so the spectator keeps
	// orbiting and zooming while the target runs.
	void FollowEntity(IEntity entity)
	{
		if (!entity)
			return;

		vector entityTransform[4];
		entity.GetWorldTransform(entityTransform);

		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character && entity.GetAnimation())
		{
			vector boneTransform[4], boneWorldTransform[4];
			TNodeId boneIndex = entity.GetAnimation().GetBoneIndex("Head");
			entity.GetAnimation().GetBoneMatrix(boneIndex, boneTransform);
			Math3D.MatrixMultiply4(entityTransform, boneTransform, boneWorldTransform);

			// In the character's orientation: 1.5 m up, 3 m behind.
			vector offset = "0 1.5 -3";
			vector cameraPos = boneWorldTransform[3]
				+ (entityTransform[0] * offset[0])
				+ (entityTransform[1] * offset[1])
				+ (entityTransform[2] * offset[2]);

			vector cameraTransform[4];
			LookAt(cameraPos, boneWorldTransform[3], cameraTransform);
			SetTransform(cameraTransform);
		}
		else
		{
			SetTransform(entityTransform);
		}

		SCR_AttachManualCameraComponent attachComp = SCR_AttachManualCameraComponent.Cast(
			FindCameraComponent(SCR_AttachManualCameraComponent));
		if (attachComp)
			attachComp.AttachTo(entity);

		SetDirty(true);
	}

	// Height is preserved as height above terrain, or a jump from a hilltop to a valley
	// would leave the camera in orbit.
	void MoveTo(float worldX, float worldZ, float minHeight)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		StopFollowing();

		vector transform[4];
		GetWorldTransform(transform);

		float height = transform[3][1] - world.GetSurfaceY(transform[3][0], transform[3][2]);
		if (height < minHeight)
			height = minHeight;

		transform[3] = Vector(worldX, world.GetSurfaceY(worldX, worldZ) + height, worldZ);
		SetWorldTransform(transform);

		SetDirty(true);
	}

	void StopFollowing()
	{
		SCR_AttachManualCameraComponent attachComp = SCR_AttachManualCameraComponent.Cast(
			FindCameraComponent(SCR_AttachManualCameraComponent));
		if (attachComp)
			attachComp.Detach();
	}

	// Detached, levelled, default FOV, 1× speed.
	void ResetCamera()
	{
		StopFollowing();

		vector transform[4];
		GetWorldTransform(transform);
		vector position = transform[3];

		vector angles = Math3D.MatrixToAngles(transform);
		angles[1] = 0;
		angles[2] = 0;
		Math3D.AnglesToMatrix(angles, transform);
		transform[3] = position;
		SetWorldTransform(transform);

		SetVerticalFOV(GetDefaultFOV());

		SCR_AdjustSpeedManualCameraComponent speedComp = SCR_AdjustSpeedManualCameraComponent.Cast(
			FindCameraComponent(SCR_AdjustSpeedManualCameraComponent));
		if (speedComp)
			speedComp.LL_ResetSpeed();

		SetDirty(true);
	}

	// Also used by LL_SpectatorManager for the spawn position.
	static void LookAt(vector from, vector to, out vector result[4])
	{
		vector forward = to - from;
		forward.Normalize();

		vector right = vector.Up * forward;
		right.Normalize();

		vector up = forward * right;

		result[0] = right;
		result[1] = up;
		result[2] = forward;
		result[3] = from;
	}
}