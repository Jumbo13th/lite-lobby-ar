// Holds airborne helicopters in place for the length of a hard freeze. Physics authority
// follows the replication owner, so the server takes ownership of every aircraft it pins
// and keeps it for the whole hold: the engine hands a vehicle to its pilot's machine when
// the pilot's body is possessed, which would silently move the simulation to a body that
// is not pinned. The release gives the aircraft back to whoever sits in the pilot seat.

class LL_PinnedVehicle
{
	IEntity m_Entity;
	vector m_vOrigin;
	float m_fLinearDamping;
	float m_fAngularDamping;
	float m_fMaxDrift;
}

class LL_StoppedVehicle
{
	IEntity m_Entity;
	vector m_vOrigin;
}

class LL_VehicleHold
{
	// Skids on the ground read a few centimetres; anything above this is flying.
	protected static const float AIRBORNE_AGL_M = 2;
	protected static const float DRIFT_LOG_M = 1;
	protected static const float TRACE_DOWN_M = 1000;

	// The engine-start gate refuses every start during a hold except the pin's own.
	protected static bool s_bStartingHeldEngine;

	static bool IsStartingHeldEngine()
	{
		return s_bStartingHeldEngine;
	}

	protected ref array<ref LL_PinnedVehicle> m_aPinned = {};
	protected ref array<ref LL_StoppedVehicle> m_aStopped = {};
	protected int m_iStoppedCars;
	protected int m_iStoppedTracked;
	protected int m_iStoppedAircraft;

	// Measured with a trace, not read from the flight model: a body restored by a save
	// sleeps until something touches it, and the flight model reports nothing meanwhile.
	// The trace starts under the hull so the crew and the airframe cannot block it.
	static float MeasureAltitudeAGL(notnull IEntity vehicle)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		vector mins, maxs;
		vehicle.GetWorldBounds(mins, maxs);
		vector origin = vehicle.GetOrigin();
		vector start = Vector(origin[0], mins[1] - 0.5, origin[2]);

		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = Vector(start[0], start[1] - TRACE_DOWN_M, start[2]);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = vehicle;

		float coef = world.TraceMove(trace, null);
		if (coef < 1)
			return TRACE_DOWN_M * coef;

		return start[1] - world.GetSurfaceY(start[0], start[2]);
	}

	// The physics interface has setters but no getters: the values to put back are the
	// prefab's, and the engine's zero where the prefab sets none.
	protected static void ReadPrefabDamping(IEntity entity, out float linear, out float angular)
	{
		linear = 0;
		angular = 0;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return;

		IEntitySource source = IEntitySource.Cast(prefabData.GetPrefab());
		IEntityComponentSource rigidBody = SCR_BaseContainerTools.FindComponentSource(source, "RigidBody");
		if (!rigidBody)
			return;

		rigidBody.Get("LinearDamping", linear);
		rigidBody.Get("AngularDamping", angular);
	}

	protected static string Describe(notnull IEntity entity)
	{
		string name = entity.GetName();
		if (name != "")
			return name;

		return SCR_ResourceNameUtils.GetPrefabName(entity);
	}

	protected LL_PinnedVehicle Find(IEntity entity)
	{
		foreach (LL_PinnedVehicle pinned : m_aPinned)
		{
			if (pinned.m_Entity == entity)
				return pinned;
		}
		return null;
	}

	//! Server: every vehicle in the world; airborne helicopters are pinned, the rest stopped.
	void PinAll_S()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_iStoppedCars = 0;
		m_iStoppedTracked = 0;
		m_iStoppedAircraft = 0;

		vector mins, maxs;
		world.GetBoundBox(mins, maxs);
		world.QueryEntitiesByAABB(mins, maxs, HoldVehicle_S, null, EQueryEntitiesFlags.DYNAMIC);

		Print(string.Format("[LL_Lobby] Hold: stopped %1 car(s), %2 tracked, %3 grounded aircraft; %4 pinned",
			m_iStoppedCars, m_iStoppedTracked, m_iStoppedAircraft, m_aPinned.Count()), LogLevel.NORMAL);
	}

	protected bool HoldVehicle_S(IEntity entity)
	{
		if (!Vehicle.Cast(entity))
			return true;

		HelicopterControllerComponent heli = HelicopterControllerComponent.Cast(entity.FindComponent(HelicopterControllerComponent));
		if (heli)
		{
			float agl = MeasureAltitudeAGL(entity);
			if (agl > AIRBORNE_AGL_M)
			{
				Pin_S(entity, heli, agl);
				return true;
			}
		}

		Stop_S(entity, heli);
		return true;
	}

	// The game's own recipe for a vehicle whose pilot dropped, plus the engine off on a
	// grounded aircraft: its pilot is still seated, and only the engine-start gate holds
	// them down. A tracked vehicle has no persistent brake, so on a slope it may creep;
	// the release logs it.
	protected void Stop_S(notnull IEntity vehicle, HelicopterControllerComponent heli)
	{
		if (heli)
		{
			heli.SetPersistentWheelBrake(true);
			heli.SetAutohoverEnabled(true);
			heli.StopEngine(false);
			m_iStoppedAircraft++;
		}
		else
		{
			CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
			TrackedControllerComponent tracked = TrackedControllerComponent.Cast(vehicle.FindComponent(TrackedControllerComponent));
			if (car)
			{
				car.Shutdown();
				car.StopEngine(false);
				car.SetPersistentHandBrake(true);
				m_iStoppedCars++;
			}
			else if (tracked)
			{
				tracked.Shutdown();
				tracked.StopEngine(false);
				m_iStoppedTracked++;
			}
			else
				return;
		}

		LL_StoppedVehicle stopped = new LL_StoppedVehicle();
		stopped.m_Entity = vehicle;
		stopped.m_vOrigin = vehicle.GetOrigin();
		m_aStopped.Insert(stopped);
	}

	protected void Pin_S(notnull IEntity vehicle, notnull HelicopterControllerComponent heli, float agl)
	{
		if (Find(vehicle))
			return;

		Physics physics = vehicle.GetPhysics();
		if (!physics)
			return;

		LL_PinnedVehicle pinned = new LL_PinnedVehicle();
		pinned.m_Entity = vehicle;
		pinned.m_vOrigin = vehicle.GetOrigin();
		ReadPrefabDamping(vehicle, pinned.m_fLinearDamping, pinned.m_fAngularDamping);
		m_aPinned.Insert(pinned);

		TakeOwnership_S(vehicle);

		physics.SetLinearFactor(vector.Zero);
		physics.SetDamping(1000, 1000);
		physics.SetVelocity(vector.Zero);
		physics.SetAngularVelocity(vector.Zero);

		// The native save keeps no engine state, and an aircraft restored in the air comes
		// back with its engine off; autohover cannot hold it after the release without one.
		// Airborne means the engine was running when the snapshot was taken.
		if (!heli.IsEngineOn())
		{
			ForceStartEngineParams params = new ForceStartEngineParams();
			params.m_bAirborne = true;
			s_bStartingHeldEngine = true;
			heli.ForceStartEngine(params);
			s_bStartingHeldEngine = false;
		}

		Print(string.Format("[LL_Lobby] Hold: pinned %1 at %2 m AGL", Describe(vehicle), Math.Round(agl)), LogLevel.NORMAL);
	}

	protected void TakeOwnership_S(notnull IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || rpl.IsOwner())
			return;

		rpl.GiveExt(RplIdentity.Local(), false);
	}

	//! Server, per frame while held: keep ownership (a possession mid-hold hands the vehicle to
	//! the pilot again) and measure how far each body moved from where it was pinned.
	void Tick_S()
	{
		foreach (LL_PinnedVehicle pinned : m_aPinned)
		{
			IEntity vehicle = pinned.m_Entity;
			if (!vehicle)
				continue;

			TakeOwnership_S(vehicle);

			float drift = vector.Distance(vehicle.GetOrigin(), pinned.m_vOrigin);
			if (drift > pinned.m_fMaxDrift)
				pinned.m_fMaxDrift = drift;
		}
	}

	// The brakes stay on, as the game leaves a dropped vehicle; only the drift is reported.
	protected void ReleaseStopped_S()
	{
		foreach (LL_StoppedVehicle stopped : m_aStopped)
		{
			IEntity vehicle = stopped.m_Entity;
			if (!vehicle)
				continue;

			float drift = vector.Distance(vehicle.GetOrigin(), stopped.m_vOrigin);
			if (drift > DRIFT_LOG_M)
				Print(string.Format("[LL_Lobby] Hold: %1 moved %2 m while stopped", Describe(vehicle), Math.Round(drift)), LogLevel.WARNING);
		}
		m_aStopped.Clear();
	}

	//! Server: physics back to the prefab, autohover on (the game's own recipe for an aircraft
	//! whose pilot dropped), then the aircraft goes back to whoever holds the pilot seat.
	void Release_S()
	{
		ReleaseStopped_S();

		foreach (LL_PinnedVehicle pinned : m_aPinned)
		{
			IEntity vehicle = pinned.m_Entity;
			if (!vehicle)
				continue;

			Physics physics = vehicle.GetPhysics();
			if (physics)
			{
				physics.SetLinearFactor(Vector(1, 1, 1));
				physics.SetDamping(pinned.m_fLinearDamping, pinned.m_fAngularDamping);
				// A body that slept through the load would otherwise hang until touched.
				physics.SetActive(ActiveState.ACTIVE);
			}

			HelicopterControllerComponent heli = HelicopterControllerComponent.Cast(vehicle.FindComponent(HelicopterControllerComponent));
			if (heli)
			{
				heli.SetAutohoverEnabled(true);
				GiveToPilot_S(vehicle, heli);
			}

			if (pinned.m_fMaxDrift > DRIFT_LOG_M)
				Print(string.Format("[LL_Lobby] Hold: %1 drifted %2 m while pinned", Describe(vehicle), pinned.m_fMaxDrift), LogLevel.WARNING);
			else
				Print(string.Format("[LL_Lobby] Hold: released %1", Describe(vehicle)), LogLevel.NORMAL);
		}

		m_aPinned.Clear();
	}

	protected void GiveToPilot_S(notnull IEntity vehicle, notnull HelicopterControllerComponent heli)
	{
		PilotCompartmentSlot pilotSlot = heli.GetPilotCompartmentSlot();
		if (!pilotSlot)
			return;

		IEntity pilot = pilotSlot.GetOccupant();
		if (!pilot)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		PlayerController playerController = playerManager.GetPlayerController(playerManager.GetPlayerIdFromControlledEntity(pilot));
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (playerController && rpl)
			rpl.GiveExt(playerController.GetRplIdentity(), false);
	}
}
