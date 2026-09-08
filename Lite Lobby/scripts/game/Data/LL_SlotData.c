// One playable slot as replicated data. Never a RplProp: LL_LobbyManager owns the
// collection. Codec methods follow the vanilla SCR_MapMarkerBase pattern.

class LL_SlotData
{
	int m_iRplId;

	string m_sName;

	string m_sFactionKey;

	int m_iPlayerId;

	// RplId of the group; may convert to a negative int, used only as a key.
	int m_iGroupId;

	string m_sGroupName;

	// Imageset path + quad name, or a direct texture path with empty name.
	string m_sIconPath;
	string m_sIconName;

	// Feeds the client-side 3D preview. Carried by a dedicated follow-up RPC because the
	// registration RPC is at the 8-argument cap.
	string m_sPrefabName;

	int m_iDamageState;

	bool m_bLocked;

	// Within-squad ordering key: the slot's own RplId (editor placement order) for a
	// loadtime body, inherited from the replaced slot on respawn so the position is kept.
	int m_iSortKey;

	void LL_SlotData()
	{
		m_iRplId = -1;
		m_sName = "";
		m_sFactionKey = "";
		m_iPlayerId = -1;
		m_iGroupId = -1;
		m_sGroupName = "";
		m_sIconPath = "";
		m_sIconName = "";
		m_sPrefabName = "";
		m_iDamageState = 0;
		m_bLocked = false;
		m_iSortKey = 0;
	}

	bool IsEmpty()
	{
		return m_iPlayerId == -1;
	}

	bool IsDestroyed()
	{
		return m_iDamageState == EDamageState.DESTROYED;
	}

	bool IsAvailable()
	{
		return IsEmpty() && !m_bLocked && !IsDestroyed();
	}

	static bool Extract(LL_SlotData instance, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.SerializeInt(instance.m_iRplId);
		snapshot.SerializeInt(instance.m_iPlayerId);
		snapshot.SerializeInt(instance.m_iGroupId);
		snapshot.SerializeInt(instance.m_iDamageState);
		snapshot.SerializeInt(instance.m_iSortKey);
		snapshot.SerializeBool(instance.m_bLocked);
		snapshot.SerializeString(instance.m_sName);
		snapshot.SerializeString(instance.m_sFactionKey);
		snapshot.SerializeString(instance.m_sGroupName);
		snapshot.SerializeString(instance.m_sIconPath);
		snapshot.SerializeString(instance.m_sIconName);
		snapshot.SerializeString(instance.m_sPrefabName);
		return true;
	}

	static bool Inject(SSnapSerializerBase snapshot, ScriptCtx ctx, LL_SlotData instance)
	{
		snapshot.SerializeInt(instance.m_iRplId);
		snapshot.SerializeInt(instance.m_iPlayerId);
		snapshot.SerializeInt(instance.m_iGroupId);
		snapshot.SerializeInt(instance.m_iDamageState);
		snapshot.SerializeInt(instance.m_iSortKey);
		snapshot.SerializeBool(instance.m_bLocked);
		snapshot.SerializeString(instance.m_sName);
		snapshot.SerializeString(instance.m_sFactionKey);
		snapshot.SerializeString(instance.m_sGroupName);
		snapshot.SerializeString(instance.m_sIconPath);
		snapshot.SerializeString(instance.m_sIconName);
		snapshot.SerializeString(instance.m_sPrefabName);
		return true;
	}

	static void Encode(SSnapSerializerBase snapshot, ScriptCtx ctx, ScriptBitSerializer packet)
	{
		// 5 ints as a bulk block; the bool is encoded separately to match Extract's order.
		snapshot.Serialize(packet, 20);
		snapshot.EncodeBool(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
	}

	// Must mirror Encode exactly.

	static bool Decode(ScriptBitSerializer packet, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.Serialize(packet, 20);
		snapshot.DecodeBool(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		return true;
	}

	static bool SnapCompare(SSnapSerializerBase lhs, SSnapSerializerBase rhs, ScriptCtx ctx)
	{
		return lhs.CompareSnapshots(rhs, 20)
			&& lhs.CompareSnapshots(rhs, 1)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs);
	}

	static bool PropCompare(LL_SlotData instance, SSnapSerializerBase snapshot, ScriptCtx ctx)
	{
		return snapshot.CompareInt(instance.m_iRplId)
			&& snapshot.CompareInt(instance.m_iPlayerId)
			&& snapshot.CompareInt(instance.m_iGroupId)
			&& snapshot.CompareInt(instance.m_iDamageState)
			&& snapshot.CompareInt(instance.m_iSortKey)
			&& snapshot.CompareBool(instance.m_bLocked)
			&& snapshot.CompareString(instance.m_sName)
			&& snapshot.CompareString(instance.m_sFactionKey)
			&& snapshot.CompareString(instance.m_sGroupName)
			&& snapshot.CompareString(instance.m_sIconPath)
			&& snapshot.CompareString(instance.m_sIconName)
			&& snapshot.CompareString(instance.m_sPrefabName);
	}
}