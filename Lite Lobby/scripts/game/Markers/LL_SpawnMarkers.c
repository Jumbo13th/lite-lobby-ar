// Squad spawn markers on the briefing and in-game maps: a snapshot re-taken when
// BRIEFING and GAME begin, never live tracking. Server-captured because squads past
// networkViewDistance never stream to a given client. Broadcast RPC plus RplSave for JIP.

class LL_SpawnMarkerData
{
	vector m_vPos;
	string m_sCallsign;
	string m_sLeader;
	int m_iOccupied;
	int m_iTotal;
	string m_sFaction;
	bool m_bIsVehicle;
}

class LL_SpawnMarkersClass : SCR_BaseGameModeComponentClass
{
}

class LL_SpawnMarkers : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Show squad spawn markers (on the briefing and in-game maps).", category: "Spawn Markers")]
	protected bool m_bShowSquadMarkers;

	[Attribute("1", UIWidgets.CheckBox, "Show a marker for each vehicle, colored by the vehicle's faction.", category: "Spawn Markers")]
	protected bool m_bShowVehicleMarkers;

	[Attribute("0", UIWidgets.CheckBox, "Visible to everyone — all players and spectators see every squad. Off = each player sees only their own faction's squads, and spectators see none.", category: "Spawn Markers")]
	protected bool m_bVisibleToEveryone;

	// Appearance mirrors LL_ManualMarker.
	[Attribute("{CCED36C0D903191D}UI/Textures/GroupManagement/FlagIcons/LiteIcons.imageset", UIWidgets.ResourceNamePicker, "Marker icon imageset (or a direct .edds).", category: "Spawn Markers")]
	protected ResourceName m_sImageSet;

	[Attribute("squad", UIWidgets.EditBox, "Quad (sprite) name for SQUAD markers.", category: "Spawn Markers")]
	protected string m_sQuadName;

	[Attribute("vehicle", UIWidgets.EditBox, "Quad (sprite) name for VEHICLE markers.", category: "Spawn Markers")]
	protected string m_sVehicleQuad;

	[Attribute("5", UIWidgets.EditBox, "Marker size. Meters (scales with zoom) when world scale is on, pixels otherwise.", category: "Spawn Markers")]
	protected float m_fWorldSize;

	[Attribute("1", UIWidgets.CheckBox, "Size in meters that scales with map zoom (on) vs a fixed pixel size (off).", category: "Spawn Markers")]
	protected bool m_bUseWorldScale;

	[Attribute("30", UIWidgets.EditBox, "Minimum on-screen pixel size, so world-scaled markers never shrink to nothing when zoomed out.", category: "Spawn Markers")]
	protected float m_fMinSize;

	[Attribute("1", UIWidgets.CheckBox, "Tint the icon with the squad's faction color.", category: "Spawn Markers")]
	protected bool m_bUseFactionColor;

	[Attribute("1 1 1 1", UIWidgets.ColorPicker, "Icon tint used when faction color is off.", category: "Spawn Markers")]
	protected ref Color m_MarkerColor;

	protected const ResourceName MARKER_LAYOUT = "{1C0F8A2B3C4D5E6F}UI/Map/ManualMapMarkerBase.layout";

	protected ref array<ref LL_SpawnMarkerData> m_aMarkers = {};

	protected ref array<Widget> m_aWidgets = {};
	protected ref array<ref LL_SpawnMarkerData> m_aRenderData = {};

	static LL_SpawnMarkers GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;
		return LL_SpawnMarkers.Cast(gameMode.FindComponent(LL_SpawnMarkers));
	}

	// The squad's captured spawn position, for the roster's clickable squad names. False
	// before briefing.
	bool GetSquadSpawnPos(string callsign, FactionKey factionKey, out vector pos)
	{
		foreach (LL_SpawnMarkerData m : m_aMarkers)
		{
			if (m.m_bIsVehicle)
				continue;
			if (m.m_sCallsign == callsign && m.m_sFaction == factionKey)
			{
				pos = m.m_vPos;
				return true;
			}
		}
		return false;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_MarkerColor)
			m_MarkerColor = new Color(1, 1, 1, 1);

		// The game mode's state invoker is not reliably reachable during component init.
		GetGame().GetCallqueue().Call(SubscribeToState);
	}

	protected void SubscribeToState()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		if (mode)
			mode.GetOnGameStateChanged().Insert(OnLobbyStateChanged);
	}

	protected void OnLobbyStateChanged(int state)
	{
		SCR_EGameModeState s = state;
		if (s == SCR_EGameModeState.BRIEFING || s == SCR_EGameModeState.GAME)
			BuildMarkers_S();
	}

	void BuildMarkers_S()
	{
		if (!Replication.IsServer())
			return;

		m_aMarkers.Clear();

		LL_LobbyManager mgr = LL_LobbyManager.GetInstance();
		if (!mgr)
			return;

		if (m_bShowSquadMarkers)
			BuildSquadMarkers_S(mgr);

		if (m_bShowVehicleMarkers)
			BuildVehicleMarkers_S();

		BroadcastMarkers_S();
	}

	protected void BuildSquadMarkers_S(LL_LobbyManager mgr)
	{
		array<ref LL_SlotData> slots = mgr.GetSlots();
		array<int> doneGroups = {};

		foreach (LL_SlotData seed : slots)
		{
			if (seed.m_iGroupId == -1 || doneGroups.Contains(seed.m_iGroupId))
				continue;
			doneGroups.Insert(seed.m_iGroupId);

			int occupied = 0;
			int total = 0;
			int posCount = 0;
			vector sum = vector.Zero;
			LL_SlotData leaderSlot = null;

			foreach (LL_SlotData s : slots)
			{
				if (s.m_iGroupId != seed.m_iGroupId || s.m_sFactionKey != seed.m_sFactionKey)
					continue;

				total++;
				if (s.m_iPlayerId >= 0)
					occupied++;
				if (!leaderSlot)
					leaderSlot = s;

				IEntity ent = mgr.GetSlotEntity_S(s.m_iRplId);
				if (ent)
				{
					sum += ent.GetOrigin();
					posCount++;
				}
			}

			if (posCount == 0)
				continue;

			LL_SpawnMarkerData m = new LL_SpawnMarkerData();
			vector centre = sum / posCount;
			m.m_vPos = Vector(centre[0], 0, centre[2]);
			m.m_sCallsign = seed.m_sGroupName;
			m.m_sFaction = seed.m_sFactionKey;
			m.m_iOccupied = occupied;
			m.m_iTotal = total;
			if (leaderSlot && leaderSlot.m_iPlayerId >= 0)
				m.m_sLeader = mgr.GetPlayerName(leaderSlot.m_iPlayerId);
			else
				m.m_sLeader = "";

			m_aMarkers.Insert(m);
		}
	}

	// The lobby vehicle registry holds only squad-linked vehicles, so the world's dynamic
	// entities are scanned once per snapshot, server-side.
	protected void BuildVehicleMarkers_S()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		vector mins, maxs;
		world.GetBoundBox(mins, maxs);
		world.QueryEntitiesByAABB(mins, maxs, AddVehicleMarker_S, null, EQueryEntitiesFlags.DYNAMIC);
	}

	protected bool AddVehicleMarker_S(IEntity ent)
	{
		Vehicle vehicle = Vehicle.Cast(ent);
		if (!vehicle)
			return true;

		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(
			vehicle.FindComponent(FactionAffiliationComponent));
		if (!factionComp)
			return true;

		// A placed vehicle usually has no affiliated faction; its default key is the faction.
		string factionKey = factionComp.GetAffiliatedFactionKey();
		if (factionKey == "")
			factionKey = factionComp.GetDefaultFactionKey();
		if (factionKey == "")
			return true;

		// The prefab UIInfo name is a key the client widget resolves on display.
		string name = "";
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.GetEditableEntity(vehicle);
		if (editable)
			name = editable.GetDisplayName();

		vector p = vehicle.GetOrigin();
		LL_SpawnMarkerData m = new LL_SpawnMarkerData();
		m.m_vPos = Vector(p[0], 0, p[2]);
		m.m_sCallsign = name;
		m.m_sFaction = factionKey;
		m.m_sLeader = "";
		m.m_iOccupied = 0;
		m.m_iTotal = 0;
		m.m_bIsVehicle = true;
		m_aMarkers.Insert(m);
		return true;
	}

	protected void BroadcastMarkers_S()
	{
		array<float> posXZ = {};
		array<string> callsigns = {};
		array<string> leaders = {};
		array<int> countsPacked = {};
		array<string> factions = {};
		array<int> isVehicle = {};

		foreach (LL_SpawnMarkerData m : m_aMarkers)
		{
			posXZ.Insert(m.m_vPos[0]);
			posXZ.Insert(m.m_vPos[2]);
			callsigns.Insert(m.m_sCallsign);
			leaders.Insert(m.m_sLeader);
			countsPacked.Insert(m.m_iOccupied * 10000 + m.m_iTotal);
			factions.Insert(m.m_sFaction);
			isVehicle.Insert(m.m_bIsVehicle);
		}

		Rpc(RpcDo_SetMarkers, posXZ, callsigns, leaders, countsPacked, factions, isVehicle);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetMarkers(array<float> posXZ, array<string> callsigns, array<string> leaders, array<int> countsPacked, array<string> factions, array<int> isVehicle)
	{
		m_aMarkers = {};
		int count = callsigns.Count();
		for (int i = 0; i < count; i++)
		{
			LL_SpawnMarkerData m = new LL_SpawnMarkerData();
			m.m_vPos = Vector(posXZ[i * 2], 0, posXZ[i * 2 + 1]);
			m.m_sCallsign = callsigns[i];
			m.m_sLeader = leaders[i];
			m.m_iOccupied = countsPacked[i] / 10000;
			m.m_iTotal = countsPacked[i] % 10000;
			m.m_sFaction = factions[i];
			m.m_bIsVehicle = isVehicle[i] != 0;
			m_aMarkers.Insert(m);
		}
	}

	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(m_aMarkers.Count());
		foreach (LL_SpawnMarkerData m : m_aMarkers)
		{
			writer.WriteFloat(m.m_vPos[0]);
			writer.WriteFloat(m.m_vPos[2]);
			writer.WriteString(m.m_sCallsign);
			writer.WriteString(m.m_sLeader);
			writer.WriteInt(m.m_iOccupied);
			writer.WriteInt(m.m_iTotal);
			writer.WriteString(m.m_sFaction);
			writer.WriteBool(m.m_bIsVehicle);
		}
		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		int count;
		reader.ReadInt(count);

		m_aMarkers = {};
		for (int i = 0; i < count; i++)
		{
			float x, z;
			LL_SpawnMarkerData m = new LL_SpawnMarkerData();
			reader.ReadFloat(x);
			reader.ReadFloat(z);
			m.m_vPos = Vector(x, 0, z);
			reader.ReadString(m.m_sCallsign);
			reader.ReadString(m.m_sLeader);
			reader.ReadInt(m.m_iOccupied);
			reader.ReadInt(m.m_iTotal);
			reader.ReadString(m.m_sFaction);
			reader.ReadBool(m.m_bIsVehicle);
			m_aMarkers.Insert(m);
		}
		return true;
	}

	// One widget per squad the local player may see; UpdateOnMap positions them per frame.
	void OpenOnMap(SCR_MapEntity mapEnt)
	{
		CloseOnMap();
		if (!mapEnt)
			return;

		Widget mapFrame = mapEnt.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = mapEnt.GetMapMenuRoot();
		if (!mapFrame)
			return;

		string localFaction = GetLocalFactionKey();
		SCR_FactionManager factionMgr = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		WorkspaceWidget workspace = GetGame().GetWorkspace();

		foreach (LL_SpawnMarkerData data : m_aMarkers)
		{
			// Only the own faction's squads, and a spectator sees none, unless visible to everyone.
			if (!m_bVisibleToEveryone && (localFaction == "" || data.m_sFaction != localFaction))
				continue;

			Widget w = workspace.CreateWidgets(MARKER_LAYOUT, mapFrame);
			if (!w)
				continue;

			LL_ManualMarkerComponent handler = LL_ManualMarkerComponent.Cast(w.FindHandler(LL_ManualMarkerComponent));
			if (!handler)
			{
				w.RemoveFromHierarchy();
				continue;
			}

			Color color = m_MarkerColor;
			if (m_bUseFactionColor && factionMgr)
			{
				SCR_Faction faction = SCR_Faction.Cast(factionMgr.GetFactionByKey(data.m_sFaction));
				if (faction)
					color = faction.GetFactionColor();
			}

			string quad = m_sQuadName;
			if (data.m_bIsVehicle)
				quad = m_sVehicleQuad;

			handler.SetImage(m_sImageSet, quad);
			handler.SetDescription(FormatSquadLabel(data));
			handler.SetColor(color);
			handler.OnMouseLeave(null, null, 0, 0);

			m_aWidgets.Insert(w);
			m_aRenderData.Insert(data);
		}
	}

	// Re-projects the fixed positions per frame and applies the freeze visibility rule.
	void UpdateOnMap(SCR_MapEntity mapEnt)
	{
		if (!mapEnt || !mapEnt.IsOpen())
			return;

		bool show = ShouldShowNow();

		for (int i = 0, count = m_aWidgets.Count(); i < count; i++)
		{
			Widget w = m_aWidgets[i];
			LL_SpawnMarkerData data = m_aRenderData[i];
			if (!w || !data)
				continue;

			w.SetVisible(show);
			if (!show)
				continue;

			LL_ManualMarkerComponent handler = LL_ManualMarkerComponent.Cast(w.FindHandler(LL_ManualMarkerComponent));
			if (handler)
				handler.SetSlotWorld(data.m_vPos, Vector(90, 0, 0), m_fWorldSize, m_bUseWorldScale, m_fMinSize);
		}
	}

	void CloseOnMap()
	{
		foreach (Widget w : m_aWidgets)
		{
			if (w)
				w.RemoveFromHierarchy();
		}
		m_aWidgets.Clear();
		m_aRenderData.Clear();
	}

	// Shown during briefing and the post-start freeze only; once players move, spawn
	// positions are stale intel. The roster's squad links follow the same rule.
	bool ShouldShowNow()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		if (!mode)
			return true;

		SCR_EGameModeState state = mode.GetLobbyState();
		if (state == SCR_EGameModeState.BRIEFING)
			return true;

		if (state == SCR_EGameModeState.GAME)
			return mode.GetFreezeTimeRemaining() > 0;

		return false;
	}

	// Squad: "callsign | leader | occupied/total"; vehicle: name only.
	protected string FormatSquadLabel(LL_SpawnMarkerData data)
	{
		if (data.m_bIsVehicle)
			return data.m_sCallsign;

		string text = data.m_sCallsign;
		if (data.m_sLeader != "")
			text += " | " + data.m_sLeader;
		text += string.Format(" | %1/%2", data.m_iOccupied, data.m_iTotal);
		return text;
	}

	protected string GetLocalFactionKey()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return "";

		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(
			pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation)
			return "";

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			return "";

		return faction.GetFactionKey();
	}
}