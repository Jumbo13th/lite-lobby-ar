// The single wire and drawing layer for every LL_TrackerComponent. One game-mode
// component instead of per-entity replication: the target may never stream to a given
// client. Set changes send the full state coalesced per frame; positions are one batched
// unreliable RPC carrying only trackers that moved (quantized to whole metres and
// degrees); JIP gets the full state through RplSave. Faction visibility is decided on
// each client from its live faction, because server-side gating would mean one targeted
// message per player per interval. Beacon cues are one small broadcast per interval.

//! One tracker as clients see it: designer config + last known position.
class LL_TrackerData
{
	int m_iId;

	string m_sImageSet;
	string m_sQuad;
	string m_sLabel;
	float m_fColorR;
	float m_fColorG;
	float m_fColorB;
	float m_fColorA;
	bool m_bUseFactionColor;
	string m_sFactionKey;
	string m_sVisibleFactions;
	bool m_bAnyFaction;
	float m_fWorldSize;
	bool m_bUseWorldScale;
	float m_fMinSize;
	int m_iZOrder;
	bool m_bShowHeading;

	bool m_bBeacon;
	float m_fBeaconRange;

	vector m_vPos;
	float m_fYaw;
	bool m_bHasPos;
	bool m_bStale;

	bool m_bKeepLastPosition;
}

//! Authority-side bookkeeping for one live tracker.
class LL_TrackerRuntime
{
	LL_TrackerComponent m_Component;
	LL_TrackerData m_Data;
	float m_fInterval;
	float m_fAccum;
	int m_iSentX;
	int m_iSentZ;
	int m_iSentYaw;
	bool m_bEverSent;

	// Own clock: the beep must come from where the object is now, whatever the intel rate.
	float m_fBeaconInterval;
	float m_fBeaconAccum;
}

class LL_TrackerManagerClass : SCR_BaseGameModeComponentClass
{
}

class LL_TrackerManager : SCR_BaseGameModeComponent
{
	// Per-tracker intervals are multiples of this: one server pass per tick.
	protected const int BASE_TICK_MS = 250;
	protected const float BASE_TICK_S = 0.25;

	// Compared on the quantized wire values, so "nothing to send" is literal.
	protected const int POS_EPSILON = 1;
	protected const int YAW_EPSILON = 5;

	protected const ResourceName MARKER_LAYOUT = "{1C0F8A2B3C4D5E6F}UI/Map/ManualMapMarkerBase.layout";

	// Routed to the FinalMix SFX input, which makes it a positional world sound.
	protected const ResourceName BEEP_PROJECT = "{69F1A2B3C4D50200}Sounds/Tracker/ll_tracker_sounds.acp";
	protected const string BEEP_EVENT = "LL_TRACKER_BEEP";

	// A frozen "last known position" marker is dimmed to this, so it reads as old intel.
	protected const float STALE_OPACITY = 0.55;

	// Wire layout of one tracker in the flat PackState/ApplyState encoding.
	protected const int STRIDE_STRINGS = 5;
	protected const int STRIDE_INTS = 3;
	protected const int STRIDE_FLOATS = 10;

	// Shared by server and client: a hosted server renders straight from it, since a
	// broadcast never loops back to the authority.
	protected ref array<ref LL_TrackerData> m_aTrackers = {};
	protected int m_iVersion;

	protected ref array<ref LL_TrackerRuntime> m_aRuntime = {};
	protected bool m_bLive;
	protected int m_iNextId;
	protected bool m_bSyncQueued;

	protected bool m_bBeepProjectLoaded;

	// Own references: a resync replaces m_aTrackers wholesale while the widgets still
	// draw for one frame.
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<ref LL_TrackerData> m_aRenderData = {};
	protected int m_iBuiltVersion = -1;

	static LL_TrackerManager GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;
		return LL_TrackerManager.Cast(gameMode.FindComponent(LL_TrackerManager));
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// The game mode's state invoker is not reliably reachable during component init.
		GetGame().GetCallqueue().Call(SubscribeToState);
	}

	protected void SubscribeToState()
	{
		LL_GameModeCoop mode = LL_GameModeCoop.GetInstance();
		if (mode)
			mode.GetOnGameStateChanged().Insert(OnLobbyStateChanged);

		// Loaded up front or the first beep is swallowed. Dedicated servers have no audio.
		if (RplSession.Mode() != RplMode.Dedicated)
			PreloadBeep();
	}

	protected void OnLobbyStateChanged(int state)
	{
		if (!Replication.IsServer())
			return;

		SCR_EGameModeState s = state;
		if (s == SCR_EGameModeState.GAME)
			GoLive_S();
		else
			GoDark_S();
	}

	protected void GoLive_S()
	{
		if (m_bLive)
			return;
		m_bLive = true;

		array<LL_TrackerComponent> trackers = LL_TrackerComponent.GetAll();
		if (trackers)
		{
			foreach (LL_TrackerComponent comp : trackers)
			{
				if (comp)
					AddTracker_S(comp);
			}
		}

		// The sync flushes at the end of this frame and packs the positions sampled below.
		QueueSync_S();
		Sample_S();

		GetGame().GetCallqueue().CallLater(Sample_S, BASE_TICK_MS, true);
	}

	protected void GoDark_S()
	{
		if (!m_bLive)
			return;
		m_bLive = false;

		GetGame().GetCallqueue().Remove(Sample_S);
		m_aRuntime.Clear();
		m_aTrackers.Clear();
		m_iVersion++;
		QueueSync_S();
	}

	//! Before GAME the component waits in LL_TrackerComponent's registry.
	void OnTrackerAdded(notnull LL_TrackerComponent comp)
	{
		if (!Replication.IsServer() || !m_bLive)
			return;

		if (FindRuntime(comp))
			return;

		AddTracker_S(comp);
		QueueSync_S();

		// Only the newcomer is sampled so the running trackers keep their phase.
		LL_TrackerRuntime runtime = FindRuntime(comp);
		array<int> ids = {};
		array<int> posData = {};
		if (runtime && !SampleOne_S(runtime, ids, posData))
			RetireTracker_S(runtime);
	}

	void OnTrackerRemoved(notnull LL_TrackerComponent comp)
	{
		if (!Replication.IsServer())
			return;

		LL_TrackerRuntime runtime = FindRuntime(comp);
		if (!runtime)
			return;

		RetireTracker_S(runtime);
	}

	protected void AddTracker_S(notnull LL_TrackerComponent comp)
	{
		m_iNextId++;

		LL_TrackerData data = new LL_TrackerData();
		data.m_iId = m_iNextId;
		comp.ExportConfig(data);

		LL_TrackerRuntime runtime = new LL_TrackerRuntime();
		runtime.m_Component = comp;
		runtime.m_Data = data;
		runtime.m_fInterval = Math.Max(comp.GetUpdateInterval(), BASE_TICK_S);
		runtime.m_fBeaconInterval = Math.Max(comp.GetBeaconInterval(), BASE_TICK_S);

		m_aTrackers.Insert(data);
		m_aRuntime.Insert(runtime);
		m_iVersion++;
	}

	// Keep the last known position dimmed, or remove outright; sampling stops either way.
	protected void RetireTracker_S(notnull LL_TrackerRuntime runtime)
	{
		LL_TrackerData data = runtime.m_Data;
		m_aRuntime.RemoveItem(runtime);

		if (data.m_bKeepLastPosition && data.m_bHasPos)
			data.m_bStale = true;
		else
			m_aTrackers.RemoveItem(data);

		m_iVersion++;
		QueueSync_S();
	}

	protected LL_TrackerRuntime FindRuntime(LL_TrackerComponent comp)
	{
		foreach (LL_TrackerRuntime runtime : m_aRuntime)
		{
			if (runtime.m_Component == comp)
				return runtime;
		}
		return null;
	}

	protected void Sample_S()
	{
		if (!m_bLive)
			return;

		array<int> ids = {};
		array<int> posData = {};
		array<int> beepIds = {};
		array<int> beepPos = {};
		array<ref LL_TrackerRuntime> retire = {};

		foreach (LL_TrackerRuntime runtime : m_aRuntime)
		{
			// The beacon clock is usually the first to notice a lost target.
			if (!AccumulateBeacon_S(runtime, beepIds, beepPos))
			{
				retire.Insert(runtime);
				continue;
			}

			runtime.m_fAccum += BASE_TICK_S;

			// Sampled every tick until the first report, so the marker appears at round start.
			if (runtime.m_bEverSent && runtime.m_fAccum + 0.001 < runtime.m_fInterval)
				continue;
			runtime.m_fAccum = 0;

			if (!SampleOne_S(runtime, ids, posData))
				retire.Insert(runtime);
		}

		foreach (LL_TrackerRuntime dead : retire)
		{
			RetireTracker_S(dead);
		}

		// A sync queued this frame already carries these positions.
		if (!ids.IsEmpty() && !m_bSyncQueued)
			Rpc(RpcDo_SetPositions, ids, posData);

		if (!beepIds.IsEmpty())
		{
			Rpc(RpcDo_Beep, beepIds, beepPos);

			// A broadcast never loops back to the authority; the host must hear its own beacons.
			if (RplSession.Mode() != RplMode.Dedicated)
				RpcDo_Beep(beepIds, beepPos);
		}
	}

	//! Appends a due beep with a fresh 3D position; false when the target is gone. The
	//! position is re-read because the marker may refresh minutes apart, and the y goes
	//! too or the beep sounds from under the floor.
	protected bool AccumulateBeacon_S(notnull LL_TrackerRuntime runtime, notnull array<int> beepIds, notnull array<int> beepPos)
	{
		if (!runtime.m_Data.m_bBeacon)
			return true;

		runtime.m_fBeaconAccum += BASE_TICK_S;
		if (runtime.m_fBeaconAccum + 0.001 < runtime.m_fBeaconInterval)
			return true;
		runtime.m_fBeaconAccum = 0;

		LL_TrackerComponent comp = runtime.m_Component;
		vector pos;
		float yaw;
		if (!comp || !comp.SampleTarget(pos, yaw))
			return false;

		int x = Math.Round(pos[0]);
		int y = Math.Round(pos[1]);
		int z = Math.Round(pos[2]);

		beepIds.Insert(runtime.m_Data.m_iId);
		beepPos.Insert(x);
		beepPos.Insert(y);
		beepPos.Insert(z);
		return true;
	}

	//! Appends the tracker to the batch if it moved; false when tracking must end.
	protected bool SampleOne_S(notnull LL_TrackerRuntime runtime, notnull array<int> ids, notnull array<int> posData)
	{
		LL_TrackerComponent comp = runtime.m_Component;
		vector pos;
		float yaw;
		if (!comp || !comp.SampleTarget(pos, yaw))
			return false;

		// A re-affiliated target changes the marker colour: a config change, so resync.
		FactionKey factionKey = comp.GetTargetFactionKey();
		if (factionKey != runtime.m_Data.m_sFactionKey)
		{
			runtime.m_Data.m_sFactionKey = factionKey;
			m_iVersion++;
			QueueSync_S();
		}

		int x = Math.Round(pos[0]);
		int z = Math.Round(pos[2]);
		int yawI = Math.Round(yaw);

		// The authority's copy stays exact; the send decision uses the quantized values.
		runtime.m_Data.m_vPos = Vector(pos[0], 0, pos[2]);
		runtime.m_Data.m_fYaw = yaw;
		runtime.m_Data.m_bHasPos = true;

		if (runtime.m_bEverSent)
		{
			bool moved = Math.AbsInt(x - runtime.m_iSentX) >= POS_EPSILON || Math.AbsInt(z - runtime.m_iSentZ) >= POS_EPSILON;
			bool turned = runtime.m_Data.m_bShowHeading && Math.AbsInt(yawI - runtime.m_iSentYaw) >= YAW_EPSILON;
			if (!moved && !turned)
				return true;
		}

		runtime.m_iSentX = x;
		runtime.m_iSentZ = z;
		runtime.m_iSentYaw = yawI;
		runtime.m_bEverSent = true;

		ids.Insert(runtime.m_Data.m_iId);
		posData.Insert(x);
		posData.Insert(z);
		posData.Insert(yawI);
		return true;
	}

	// Mission start registers every tracker at once, and one explosion can retire several.
	protected void QueueSync_S()
	{
		if (m_bSyncQueued)
			return;
		m_bSyncQueued = true;
		GetGame().GetCallqueue().CallLater(FlushSync_S, 0, false);
	}

	protected void FlushSync_S()
	{
		m_bSyncQueued = false;

		// World teardown queues syncs from OnDelete; the game mode may be gone by the flush.
		if (!Replication.IsServer() || !GetOwner())
			return;

		array<string> strings = {};
		array<int> ints = {};
		array<float> floats = {};
		PackState(strings, ints, floats);
		Rpc(RpcDo_SyncTrackers, strings, ints, floats);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SyncTrackers(array<string> strings, array<int> ints, array<float> floats)
	{
		ApplyState(strings, ints, floats);
	}

	//! ids[i] moved to posData[i*3 .. i*3+2]. Unreliable: the next packet corrects a lost one.
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPositions(array<int> ids, array<int> posData)
	{
		for (int i = 0, count = ids.Count(); i < count; i++)
		{
			LL_TrackerData data = FindById(ids[i]);
			if (!data)
				continue;

			data.m_vPos = Vector(posData[i * 3], 0, posData[i * 3 + 1]);
			data.m_fYaw = posData[i * 3 + 2];
			data.m_bHasPos = true;
		}
	}

	//! Not faction-gated: a sound in the world is heard by whoever is near it. Unreliable.
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_Beep(array<int> ids, array<int> beepPos)
	{
		IEntity listener = GetLocalListener();
		if (!listener)
			return;

		vector listenerPos = listener.GetOrigin();

		for (int i = 0, count = ids.Count(); i < count; i++)
		{
			LL_TrackerData data = FindById(ids[i]);
			if (!data || data.m_fBeaconRange <= 0)
				continue;

			vector pos = Vector(beepPos[i * 3], beepPos[i * 3 + 1], beepPos[i * 3 + 2]);
			if (vector.DistanceSq(pos, listenerPos) > data.m_fBeaconRange * data.m_fBeaconRange)
				continue;

			PlayBeep(pos);
		}
	}

	protected void PlayBeep(vector pos)
	{
		PreloadBeep();

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = pos;
		AudioSystem.PlayEvent(BEEP_PROJECT, BEEP_EVENT, mat);
	}

	// PlayEvent on a cold project swallows the event.
	protected void PreloadBeep()
	{
		if (m_bBeepProjectLoaded)
			return;

		m_bBeepProjectLoaded = true;
		AudioSystem.PlayEventInitialize(BEEP_PROJECT);
	}

	// Without a controlled entity there is nothing to measure earshot from.
	protected IEntity GetLocalListener()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;

		return pc.GetControlledEntity();
	}

	protected LL_TrackerData FindById(int id)
	{
		foreach (LL_TrackerData data : m_aTrackers)
		{
			if (data.m_iId == id)
				return data;
		}
		return null;
	}

	// One flat encoding for the sync RPC and JIP, so they cannot drift. A new field means
	// bumping the stride and touching both PackState and ApplyState.
	protected void PackState(notnull array<string> strings, notnull array<int> ints, notnull array<float> floats)
	{
		foreach (LL_TrackerData data : m_aTrackers)
		{
			strings.Insert(data.m_sImageSet);
			strings.Insert(data.m_sQuad);
			strings.Insert(data.m_sLabel);
			strings.Insert(data.m_sFactionKey);
			strings.Insert(data.m_sVisibleFactions);

			ints.Insert(data.m_iId);
			ints.Insert(data.m_iZOrder);
			ints.Insert(PackFlags(data));

			floats.Insert(data.m_fColorR);
			floats.Insert(data.m_fColorG);
			floats.Insert(data.m_fColorB);
			floats.Insert(data.m_fColorA);
			floats.Insert(data.m_fWorldSize);
			floats.Insert(data.m_fMinSize);
			floats.Insert(data.m_fBeaconRange);
			floats.Insert(data.m_vPos[0]);
			floats.Insert(data.m_vPos[2]);
			floats.Insert(data.m_fYaw);
		}
	}

	protected void ApplyState(array<string> strings, array<int> ints, array<float> floats)
	{
		m_aTrackers = {};

		int count = ints.Count() / STRIDE_INTS;
		for (int i = 0; i < count; i++)
		{
			int s = i * STRIDE_STRINGS;
			int n = i * STRIDE_INTS;
			int f = i * STRIDE_FLOATS;

			LL_TrackerData data = new LL_TrackerData();
			data.m_sImageSet = strings[s];
			data.m_sQuad = strings[s + 1];
			data.m_sLabel = strings[s + 2];
			data.m_sFactionKey = strings[s + 3];
			data.m_sVisibleFactions = strings[s + 4];

			data.m_iId = ints[n];
			data.m_iZOrder = ints[n + 1];
			UnpackFlags(data, ints[n + 2]);

			data.m_fColorR = floats[f];
			data.m_fColorG = floats[f + 1];
			data.m_fColorB = floats[f + 2];
			data.m_fColorA = floats[f + 3];
			data.m_fWorldSize = floats[f + 4];
			data.m_fMinSize = floats[f + 5];
			data.m_fBeaconRange = floats[f + 6];
			data.m_vPos = Vector(floats[f + 7], 0, floats[f + 8]);
			data.m_fYaw = floats[f + 9];

			m_aTrackers.Insert(data);
		}

		m_iVersion++;
	}

	protected int PackFlags(notnull LL_TrackerData data)
	{
		int flags;
		if (data.m_bAnyFaction)
			flags |= 1;
		if (data.m_bUseWorldScale)
			flags |= 2;
		if (data.m_bShowHeading)
			flags |= 4;
		if (data.m_bUseFactionColor)
			flags |= 8;
		if (data.m_bStale)
			flags |= 16;
		if (data.m_bHasPos)
			flags |= 32;
		if (data.m_bBeacon)
			flags |= 64;
		return flags;
	}

	protected void UnpackFlags(notnull LL_TrackerData data, int flags)
	{
		data.m_bAnyFaction = (flags & 1) != 0;
		data.m_bUseWorldScale = (flags & 2) != 0;
		data.m_bShowHeading = (flags & 4) != 0;
		data.m_bUseFactionColor = (flags & 8) != 0;
		data.m_bStale = (flags & 16) != 0;
		data.m_bHasPos = (flags & 32) != 0;
		data.m_bBeacon = (flags & 64) != 0;
	}

	// Positions included, so the first map open is already correct.
	override bool RplSave(ScriptBitWriter writer)
	{
		array<string> strings = {};
		array<int> ints = {};
		array<float> floats = {};
		PackState(strings, ints, floats);

		writer.WriteInt(ints.Count() / STRIDE_INTS);
		foreach (string s : strings)
		{
			writer.WriteString(s);
		}
		foreach (int i : ints)
		{
			writer.WriteInt(i);
		}
		foreach (float f : floats)
		{
			writer.WriteFloat(f);
		}
		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		int count;
		reader.ReadInt(count);

		array<string> strings = {};
		array<int> ints = {};
		array<float> floats = {};

		for (int i = 0; i < count * STRIDE_STRINGS; i++)
		{
			string s;
			reader.ReadString(s);
			strings.Insert(s);
		}
		for (int i = 0; i < count * STRIDE_INTS; i++)
		{
			int v;
			reader.ReadInt(v);
			ints.Insert(v);
		}
		for (int i = 0; i < count * STRIDE_FLOATS; i++)
		{
			float f;
			reader.ReadFloat(f);
			floats.Insert(f);
		}

		ApplyState(strings, ints, floats);
		return true;
	}

	//! A widget even for trackers this player may not see: visibility is re-evaluated
	//! every frame from the live faction.
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

		SCR_FactionManager factionMgr = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		WorkspaceWidget workspace = GetGame().GetWorkspace();

		foreach (LL_TrackerData data : m_aTrackers)
		{
			Widget w = workspace.CreateWidgets(MARKER_LAYOUT, mapFrame);
			if (!w)
				continue;

			LL_ManualMarkerComponent handler = LL_ManualMarkerComponent.Cast(w.FindHandler(LL_ManualMarkerComponent));
			if (!handler)
			{
				w.RemoveFromHierarchy();
				continue;
			}

			Color color = new Color(data.m_fColorR, data.m_fColorG, data.m_fColorB, data.m_fColorA);
			if (data.m_bUseFactionColor && factionMgr && data.m_sFactionKey != "")
			{
				SCR_Faction faction = SCR_Faction.Cast(factionMgr.GetFactionByKey(data.m_sFactionKey));
				if (faction)
					color = faction.GetFactionColor();
			}

			// Hidden until visibility and projection are decided, or it flashes at the top-left.
			w.SetVisible(false);
			w.SetZOrder(data.m_iZOrder);
			handler.SetImage(data.m_sImageSet, data.m_sQuad);
			handler.SetDescription(data.m_sLabel);
			handler.SetColor(color);
			handler.SetOpacity(1.0);
			handler.OnMouseLeave(null, null, 0, 0);

			m_aWidgets.Insert(w);
			m_aRenderData.Insert(data);
		}

		m_iBuiltVersion = m_iVersion;
	}

	//! Re-project every visible tracker onto the live map view, once per frame.
	void UpdateOnMap(SCR_MapEntity mapEnt)
	{
		if (!mapEnt || !mapEnt.IsOpen())
			return;

		// Set changed under an open map: rebuild and position in the same frame.
		if (m_iVersion != m_iBuiltVersion)
			OpenOnMap(mapEnt);

		FactionKey localFaction = GetLocalFactionKey();

		for (int i = 0, count = m_aWidgets.Count(); i < count; i++)
		{
			Widget w = m_aWidgets[i];
			LL_TrackerData data = m_aRenderData[i];
			if (!w || !data)
				continue;

			bool show = data.m_bHasPos && IsVisibleFor(data, localFaction);
			w.SetVisible(show);
			if (!show)
				continue;

			LL_ManualMarkerComponent handler = LL_ManualMarkerComponent.Cast(w.FindHandler(LL_ManualMarkerComponent));
			if (!handler)
				continue;

			// SetSlotWorld offsets the icon by -90° (the art points right); Vector(90, 0, 0)
			// means no rotation.
			vector rotation = Vector(90, 0, 0);
			if (data.m_bShowHeading)
				rotation = Vector(data.m_fYaw, 0, 0);

			handler.SetSlotWorld(data.m_vPos, rotation, data.m_fWorldSize, data.m_bUseWorldScale, data.m_fMinSize);

			if (data.m_bStale)
				handler.SetOpacity(STALE_OPACITY);
			else
				handler.SetOpacity(1.0);
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
		m_iBuiltVersion = -1;
	}

	// Same rule as manual markers and descriptions: show-for-all includes unslotted
	// players; otherwise a player with no faction matches nothing.
	protected bool IsVisibleFor(notnull LL_TrackerData data, FactionKey factionKey)
	{
		if (data.m_bAnyFaction)
			return true;

		if (factionKey == "" || data.m_sVisibleFactions == "")
			return false;

		array<string> allowed = {};
		data.m_sVisibleFactions.Split(",", allowed, true);
		return allowed.Contains(factionKey);
	}

	protected FactionKey GetLocalFactionKey()
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