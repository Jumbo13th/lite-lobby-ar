// One squad-linked vehicle as replicated data. Never a RplProp: LL_LobbyManager owns
// the collection. Codec methods follow the vanilla SCR_MapMarkerBase pattern.

class LL_VehicleData
{
	int m_iRplId;

	string m_sName;

	string m_sFactionKey;

	// A direct texture path from the editable-vehicle UI info.
	string m_sIconPath;

	// Feeds the client-side 3D hover preview.
	string m_sPrefabName;

	int m_iGroupId;

	bool m_bLocked;

	// Server-assigned registration index, the only valid ordering key.
	int m_iOrderIndex;

	void LL_VehicleData()
	{
		m_iRplId = -1;
		m_sName = "";
		m_sFactionKey = "";
		m_sIconPath = "";
		m_sPrefabName = "";
		m_iGroupId = -1;
		m_bLocked = false;
		m_iOrderIndex = -1;
	}

	static bool Extract(LL_VehicleData instance, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.SerializeInt(instance.m_iRplId);
		snapshot.SerializeInt(instance.m_iGroupId);
		snapshot.SerializeInt(instance.m_iOrderIndex);
		snapshot.SerializeBool(instance.m_bLocked);
		snapshot.SerializeString(instance.m_sName);
		snapshot.SerializeString(instance.m_sFactionKey);
		snapshot.SerializeString(instance.m_sIconPath);
		snapshot.SerializeString(instance.m_sPrefabName);
		return true;
	}

	static bool Inject(SSnapSerializerBase snapshot, ScriptCtx ctx, LL_VehicleData instance)
	{
		snapshot.SerializeInt(instance.m_iRplId);
		snapshot.SerializeInt(instance.m_iGroupId);
		snapshot.SerializeInt(instance.m_iOrderIndex);
		snapshot.SerializeBool(instance.m_bLocked);
		snapshot.SerializeString(instance.m_sName);
		snapshot.SerializeString(instance.m_sFactionKey);
		snapshot.SerializeString(instance.m_sIconPath);
		snapshot.SerializeString(instance.m_sPrefabName);
		return true;
	}

	static void Encode(SSnapSerializerBase snapshot, ScriptCtx ctx, ScriptBitSerializer packet)
	{
		snapshot.Serialize(packet, 12);
		snapshot.EncodeBool(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
	}

	static bool Decode(ScriptBitSerializer packet, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.Serialize(packet, 12);
		snapshot.DecodeBool(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		return true;
	}

	static bool SnapCompare(SSnapSerializerBase lhs, SSnapSerializerBase rhs, ScriptCtx ctx)
	{
		return lhs.CompareSnapshots(rhs, 12)
			&& lhs.CompareSnapshots(rhs, 1)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs);
	}

	static bool PropCompare(LL_VehicleData instance, SSnapSerializerBase snapshot, ScriptCtx ctx)
	{
		return snapshot.CompareInt(instance.m_iRplId)
			&& snapshot.CompareInt(instance.m_iGroupId)
			&& snapshot.CompareInt(instance.m_iOrderIndex)
			&& snapshot.CompareBool(instance.m_bLocked)
			&& snapshot.CompareString(instance.m_sName)
			&& snapshot.CompareString(instance.m_sFactionKey)
			&& snapshot.CompareString(instance.m_sIconPath)
			&& snapshot.CompareString(instance.m_sPrefabName);
	}
}