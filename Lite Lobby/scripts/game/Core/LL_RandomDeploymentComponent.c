// Sends a whole squad to one of its spawn points, drawn by the server: when the game
// phase starts or when freeze time ends, on the ground or at a height. Lives on a world
// entity the mission maker places once per squad. No replication: the draw and every
// placement are the server's, and the component does nothing on a client.

class LL_RandomDeploymentComponentClass : ScriptComponentClass
{
}

class LL_RandomDeploymentComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Entity name of the squad (SCR_AIGroup) this deployment moves.", category: "Lite Lobby")]
	protected string m_sGroupEntityName;

	[Attribute("", UIWidgets.EditBox, "Entity names of the spawn points. One is drawn at random each game; only the position of a point is used.", category: "Lite Lobby")]
	protected ref array<string> m_aPointNames;

	[Attribute("1", UIWidgets.CheckBox, "Deploy when freeze time ends. Off = when the game starts, before freeze. A squad that deploys at the end of freeze has no squad spawn marker.", category: "Lite Lobby")]
	protected bool m_bAtFreezeEnd;

	[Attribute("0", UIWidgets.EditBox, "Height above the ground (m) at which bodies with a player arrive. 0 = on the ground. The squad needs its own parachute; bodies without a player always arrive on the ground.", category: "Lite Lobby")]
	protected float m_fHeight;

	[Attribute("10", UIWidgets.EditBox, "Dispersion radius (m): the squad is spread around the drawn point within this distance.", category: "Lite Lobby")]
	protected float m_fRadius;

	protected static const int POSITION_TRIES = 10;
	protected static const float BODY_SPACING = 1.5;
	protected static const float CLEARANCE_RADIUS = 2;
	protected static const int SEAT_EXIT_TRIES = 10;
	protected static const int SEAT_EXIT_RETRY_MS = 500;

	protected static ref array<LL_RandomDeploymentComponent> s_aAll = {};
	protected static BaseWorld s_RegistryWorld;

	// Placed bodies whose fresh replacement is created at another position; parallel arrays.
	protected static ref array<IEntity> s_aPendingBodies = {};
	protected static ref array<vector> s_aPendingPositions = {};

	protected int m_iGroupId = -1;
	protected bool m_bDrawn;
	protected bool m_bPlaced;
	protected vector m_vPoint;
	protected string m_sPointName;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		// Statics survive a scenario restart.
		BaseWorld world = owner.GetWorld();
		if (world != s_RegistryWorld)
		{
			s_aAll.Clear();
			s_aPendingBodies.Clear();
			s_aPendingPositions.Clear();
			s_RegistryWorld = world;
		}

		if (!m_aPointNames)
			m_aPointNames = {};

		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
	}

	void ~LL_RandomDeploymentComponent()
	{
		if (s_aAll)
			s_aAll.RemoveItem(this);
	}

	// One log line per configuration fault, at the start of the game phase. A deployment
	// with a fault that leaves it no squad or no point is inert: its squad stays put.
	static void CheckAll_S()
	{
		BaseWorld world = GetGame().GetWorld();

		foreach (int i, LL_RandomDeploymentComponent deployment : s_aAll)
		{
			if (deployment.ResolveGroupId() == -1)
				Print(string.Format("[LL_Lobby] Deployment: squad '%1' not found, deployment inert", deployment.m_sGroupEntityName), LogLevel.WARNING);

			int found = 0;
			foreach (string pointName : deployment.m_aPointNames)
			{
				if (pointName == "")
					continue;

				if (!world.FindEntityByName(pointName))
				{
					Print(string.Format("[LL_Lobby] Deployment: spawn point '%1' of squad '%2' not found", pointName, deployment.m_sGroupEntityName), LogLevel.WARNING);
					continue;
				}

				found++;

				for (int j = i + 1; j < s_aAll.Count(); j++)
				{
					if (s_aAll[j].m_aPointNames.Contains(pointName))
						Print(string.Format("[LL_Lobby] Deployment: spawn point '%1' is named by squads '%2' and '%3'", pointName, deployment.m_sGroupEntityName, s_aAll[j].m_sGroupEntityName), LogLevel.WARNING);
				}
			}

			if (found == 0)
				Print(string.Format("[LL_Lobby] Deployment: squad '%1' has no spawn point, deployment inert", deployment.m_sGroupEntityName), LogLevel.WARNING);
		}
	}

	// Called before the state turns GAME: the state change itself triggers the spawn-marker
	// snapshot, which reads the drawn point. Also starts a new game's draws from nothing.
	static void DrawStart_S(bool includeFreezeEnd)
	{
		s_aPendingBodies.Clear();
		s_aPendingPositions.Clear();

		foreach (LL_RandomDeploymentComponent deployment : s_aAll)
		{
			deployment.m_bDrawn = false;
			deployment.m_bPlaced = false;

			if (!deployment.m_bAtFreezeEnd || includeFreezeEnd)
				deployment.Draw_S();
		}
	}

	// Called before the game-start body loop. A mission with no freeze time has no freeze
	// end, so its end-of-freeze deployments are placed here too.
	static void PlaceStart_S(bool includeFreezeEnd)
	{
		foreach (LL_RandomDeploymentComponent deployment : s_aAll)
		{
			if (!deployment.m_bAtFreezeEnd || includeFreezeEnd)
				deployment.PlaceAtStart_S();
		}
	}

	static void PlaceFreezeEnd_S()
	{
		foreach (LL_RandomDeploymentComponent deployment : s_aAll)
		{
			if (deployment.m_bAtFreezeEnd)
				deployment.PlaceAtFreezeEnd_S();
		}
	}

	// The fresh body of a slotted player is created at the point rather than moved there:
	// a freeze zone counts a body that leaves it. Replaces the translation only.
	static bool TakeSpawnTransform_S(IEntity placedBody, out vector transform[4])
	{
		int index = s_aPendingBodies.Find(placedBody);
		if (index == -1)
			return false;

		transform[3] = s_aPendingPositions[index];
		s_aPendingBodies.Remove(index);
		s_aPendingPositions.Remove(index);
		return true;
	}

	static void ClearPending_S()
	{
		s_aPendingBodies.Clear();
		s_aPendingPositions.Clear();
	}

	// True for the squad of a working end-of-freeze deployment: its marker is never sent.
	static bool HidesSpawnMarker_S(int groupId)
	{
		foreach (LL_RandomDeploymentComponent deployment : s_aAll)
		{
			if (deployment.m_bAtFreezeEnd && deployment.ResolveGroupId() == groupId && deployment.HasPoint())
				return true;
		}

		return false;
	}

	// The point a start-of-game deployment of this squad has drawn. The bodies are replaced
	// after the game-phase marker snapshot, so their own positions are not yet the answer.
	static bool GetDrawnPoint_S(int groupId, out vector point)
	{
		foreach (LL_RandomDeploymentComponent deployment : s_aAll)
		{
			if (!deployment.m_bAtFreezeEnd && deployment.m_bDrawn && deployment.ResolveGroupId() == groupId)
			{
				point = deployment.m_vPoint;
				return true;
			}
		}

		return false;
	}

	protected int ResolveGroupId()
	{
		if (m_iGroupId != -1)
			return m_iGroupId;

		if (m_sGroupEntityName != "")
			m_iGroupId = LL_TriggerComponent.ResolveGroupRplId(m_sGroupEntityName);

		return m_iGroupId;
	}

	protected bool HasPoint()
	{
		BaseWorld world = GetGame().GetWorld();
		foreach (string pointName : m_aPointNames)
		{
			if (pointName != "" && world.FindEntityByName(pointName))
				return true;
		}

		return false;
	}

	protected bool Draw_S()
	{
		if (m_bDrawn)
			return true;

		if (ResolveGroupId() == -1)
			return false;

		BaseWorld world = GetGame().GetWorld();
		array<vector> positions = {};
		array<string> names = {};
		foreach (string pointName : m_aPointNames)
		{
			if (pointName == "")
				continue;

			IEntity point = world.FindEntityByName(pointName);
			if (!point)
				continue;

			positions.Insert(point.GetOrigin());
			names.Insert(pointName);
		}

		if (positions.IsEmpty())
			return false;

		int index = Math.RandomInt(0, positions.Count());
		m_vPoint = positions[index];
		m_sPointName = names[index];
		m_bDrawn = true;
		return true;
	}

	protected void PlaceAtStart_S()
	{
		if (m_bPlaced || !Draw_S())
			return;

		m_bPlaced = true;

		array<IEntity> bodies = {};
		array<int> playerIds = {};
		CollectBodies_S(bodies, playerIds);

		array<vector> ground = {};
		BuildPositions_S(bodies.Count(), ground);

		foreach (int i, IEntity body : bodies)
		{
			MoveUnowned_S(body, ground[i], 0);

			if (playerIds[i] < 0)
				continue;

			vector spawn = ground[i];
			spawn[1] = spawn[1] + m_fHeight;
			s_aPendingBodies.Insert(body);
			s_aPendingPositions.Insert(spawn);
		}

		LogPlaced(bodies.Count());
	}

	protected void PlaceAtFreezeEnd_S()
	{
		if (m_bPlaced || !Draw_S())
			return;

		m_bPlaced = true;

		array<IEntity> bodies = {};
		array<int> playerIds = {};
		CollectBodies_S(bodies, playerIds);

		array<vector> ground = {};
		BuildPositions_S(bodies.Count(), ground);

		PlayerManager playerManager = GetGame().GetPlayerManager();
		foreach (int i, IEntity body : bodies)
		{
			LL_LobbyPlayerComponent playerComponent = null;
			int playerId = playerIds[i];
			if (playerId >= 0 && playerManager.GetPlayerControlledEntity(playerId) == body)
				playerComponent = LL_LobbyPlayerComponent.GetByPlayerId(playerId);

			if (!playerComponent)
			{
				MoveUnowned_S(body, ground[i], 0);
				continue;
			}

			vector target = ground[i];
			target[1] = target[1] + m_fHeight;
			playerComponent.Deploy_S(target);
		}

		LogPlaced(bodies.Count());
	}

	// The living slot bodies of the squad. The player id is -1 for a slot nobody holds and
	// for a holder who is away: without a controller the body is placed like an empty one.
	protected void CollectBodies_S(notnull array<IEntity> outBodies, notnull array<int> outPlayerIds)
	{
		LL_LobbyManager manager = LL_LobbyManager.GetInstance();
		if (!manager)
			return;

		int groupId = ResolveGroupId();
		PlayerManager playerManager = GetGame().GetPlayerManager();

		foreach (LL_SlotData slot : manager.GetSlots())
		{
			if (slot.m_iGroupId != groupId || slot.IsDestroyed())
				continue;

			IEntity body = manager.GetSlotEntity_S(slot.m_iRplId);
			if (!body)
				continue;

			int playerId = -1;
			if (slot.m_iPlayerId >= 0 && playerManager.GetPlayerController(slot.m_iPlayerId))
				playerId = slot.m_iPlayerId;

			outBodies.Insert(body);
			outPlayerIds.Insert(playerId);
		}
	}

	// One ground position per body. The clearance search only sees what is in the world
	// now, not the other bodies of this deployment, so the spacing is checked here.
	protected void BuildPositions_S(int count, notnull array<vector> outGround)
	{
		BaseWorld world = GetGame().GetWorld();
		float radius = Math.Max(m_fRadius, 0);
		float spacingSq = BODY_SPACING * BODY_SPACING;
		bool crowded = false;

		for (int i = 0; i < count; i++)
		{
			vector position = m_vPoint;
			bool accepted = false;

			for (int attempt = 0; attempt < POSITION_TRIES && !accepted; attempt++)
			{
				vector candidate = SCR_Math2D.GenerateRandomPointInRadius(0, radius, m_vPoint);
				candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);

				// Fills the candidate itself when it finds no free place around it.
				SCR_WorldTools.FindEmptyTerrainPosition(position, candidate, CLEARANCE_RADIUS);

				accepted = true;
				foreach (vector taken : outGround)
				{
					if (vector.DistanceSqXZ(taken, position) < spacingSq)
					{
						accepted = false;
						break;
					}
				}
			}

			if (!accepted)
				crowded = true;

			outGround.Insert(position);
		}

		if (crowded)
			Print(string.Format("[LL_Lobby] Deployment: squad '%1' does not fit %2 m around '%3'; some bodies stand closer than %4 m", m_sGroupEntityName, radius, m_sPointName, BODY_SPACING), LogLevel.WARNING);
	}

	// A body no connected player controls. The game's own entity move teleports it where it
	// is owned and tells the replication scheduler. A seated body leaves its seat first:
	// teleporting a seated character would move the vehicle.
	protected void MoveUnowned_S(IEntity body, vector position, int attempt)
	{
		if (!body)
			return;

		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (character)
		{
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (access && access.IsInCompartment())
			{
				if (attempt >= SEAT_EXIT_TRIES)
				{
					Print(string.Format("[LL_Lobby] Deployment: a body of squad '%1' could not leave its seat and was not moved", m_sGroupEntityName), LogLevel.WARNING);
					return;
				}

				access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false);
				GetGame().GetCallqueue().CallLater(MoveUnowned_S, SEAT_EXIT_RETRY_MS, false, body, position, attempt + 1);
				return;
			}
		}

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.GetEditableEntity(body);
		if (!editable)
		{
			Print(string.Format("[LL_Lobby] Deployment: a body of squad '%1' has no editable entity component and was not moved", m_sGroupEntityName), LogLevel.WARNING);
			return;
		}

		vector transform[4];
		body.GetWorldTransform(transform);
		transform[3] = position;
		editable.SetTransform(transform);
	}

	protected void LogPlaced(int bodies)
	{
		Print(string.Format("[LL_Lobby] Deployment: squad '%1' to point '%2', %3 bodies, height %4 m", m_sGroupEntityName, m_sPointName, bodies, m_fHeight), LogLevel.NORMAL);
	}
}
