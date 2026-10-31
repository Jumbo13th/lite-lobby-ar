class LL_ReplayShotCounter : Managed
{
	int m_iCount;
	int m_iYaw;
	int m_iWid;
}

class LL_ReplayFollowed : Managed
{
	IEntity m_Entity;
	vector m_vLast;
	vector m_vVelocity;
	string m_sKind;
	int m_iShooter;
	int m_iWid;
	float m_fShotTime;
}

//! Knock-outs, deaths, shot summaries and detonations, all observed from the game's own
//! events; no weapon or ammunition prefab is changed.
class LL_ReplayCombat : Managed
{
	//! Grenades and some rockets raise no detonation event and are followed every frame
	//! until they disappear; the cap bounds that cost during a barrage.
	protected static const int MAX_FOLLOWED = 32;
	protected static const float WEAPON_MEMORY_MS = 10000;
	protected static const float SAME_DETONATION_MS = 500;

	protected LL_ReplayWriter m_Writer;
	protected LL_ReplayEntities m_Entities;

	protected int m_iNextWid = 1;
	protected ref map<string, int> m_mWidByKey = new map<string, int>();
	protected ref map<int, string> m_mWeaponLines = new map<int, string>();
	protected ref map<int, string> m_mWeaponTypeByWid = new map<int, string>();
	protected ref map<int, ref LL_ReplayShotCounter> m_mShots = new map<int, ref LL_ReplayShotCounter>();
	protected ref map<int, int> m_mLastWidByShooter = new map<int, int>();
	protected ref map<int, float> m_mLastShotTimeByShooter = new map<int, float>();
	protected ref map<int, float> m_mLastExplosionByShooter = new map<int, float>();
	protected ref array<ref LL_ReplayFollowed> m_aFollowed = {};
	protected bool m_bFollowing;
	protected bool m_bListening;
	protected int m_iPlayerShots;
	protected int m_iAiShots;

	void Init(LL_ReplayWriter writer, LL_ReplayEntities entities)
	{
		m_Writer = writer;
		m_Entities = entities;
	}

	void Start_S()
	{
		if (m_bListening)
			return;

		m_bListening = true;
		SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Insert(OnExplosionAmmoEffect);
	}

	//! The explosion invoker is static and outlives the recorder, so it is always removed here.
	void Stop_S()
	{
		if (m_bListening)
		{
			m_bListening = false;
			SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Remove(OnExplosionAmmoEffect);
		}

		if (m_bFollowing)
		{
			m_bFollowing = false;
			GetGame().GetCallqueue().Remove(FollowStep_S);
		}

		m_aFollowed.Clear();
	}

	void OnShot(int eid, IEntity shooter, BaseWeaponComponent weapon, IEntity projectile, bool grenade)
	{
		if (!m_bListening)
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		int wid = WeaponId(weapon);
		if (wid > 0)
		{
			m_mLastWidByShooter.Set(eid, wid);
			m_mLastShotTimeByShooter.Set(eid, now);
		}

		LL_ReplayShotCounter counter;
		if (!m_mShots.Find(eid, counter))
		{
			counter = new LL_ReplayShotCounter();
			m_mShots.Set(eid, counter);
		}

		if (counter.m_iCount == 0)
		{
			counter.m_iYaw = ShotYaw(shooter, projectile);
			counter.m_iWid = wid;
		}

		counter.m_iCount++;

		if (shooter && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(shooter) > 0)
			m_iPlayerShots++;
		else
			m_iAiShots++;

		if (!projectile)
			return;

		if (grenade)
		{
			Follow(projectile, "grenade", eid, wid, now);
			return;
		}

		string type;
		m_mWeaponTypeByWid.Find(wid, type);
		if (type == "rocketlauncher")
			Follow(projectile, "rocket", eid, wid, now);
		else if (type == "grenadelauncher")
			Follow(projectile, "grenade", eid, wid, now);
	}

	void OnLifeState(int eid, IEntity character, ECharacterLifeState previousLifeState, ECharacterLifeState newLifeState)
	{
		if (!m_bListening)
			return;

		LL_ReplayEntityRecord record = m_Entities.RecordOf(eid);

		if (newLifeState == ECharacterLifeState.INCAPACITATED)
		{
			IEntity attacker = KnockOutAttacker(character);
			string cause;
			int wid;
			int distance;
			Describe(character, attacker, cause, wid, distance);
			int attackerEid = m_Entities.EidOf(attacker);
			m_Writer.Add(string.Format("L %1 u %2 %3 %4 %5", eid, LL_ReplayWriter.IntOr(attackerEid, attackerEid > 0),
				LL_ReplayWriter.Tok(cause), LL_ReplayWriter.IntOr(wid, wid > 0), LL_ReplayWriter.IntOr(distance, distance >= 0)));
			if (record)
				record.m_bUnconscious = true;

			return;
		}

		if (newLifeState == ECharacterLifeState.ALIVE && previousLifeState == ECharacterLifeState.INCAPACITATED)
		{
			m_Writer.Add(string.Format("L %1 a", eid));
			if (record)
				record.m_bUnconscious = false;

			return;
		}

		if (newLifeState == ECharacterLifeState.DEAD && record)
			record.m_bUnconscious = false;
	}

	//! A deletion is not a death: the entity's removal is recorded when it disappears.
	void OnDestroyed(notnull SCR_InstigatorContextData instigatorContextData)
	{
		if (!m_bListening)
			return;

		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.DELETED | SCR_ECharacterDeathStatusRelations.DELETED_BY_EDITOR))
			return;

		IEntity victim = instigatorContextData.GetVictimEntity();
		int victimEid = m_Entities.EidOf(victim);
		if (victimEid < 0)
			return;

		IEntity killer = instigatorContextData.GetKillerEntity();
		string cause;
		int wid;
		int distance;
		Describe(victim, killer, cause, wid, distance);
		int killerEid = m_Entities.EidOf(killer);

		m_Writer.Add(string.Format("K %1 %2 %3 %4 %5 %6", victimEid, LL_ReplayWriter.IntOr(killerEid, killerEid > 0),
			Relation(instigatorContextData), LL_ReplayWriter.Tok(cause), LL_ReplayWriter.IntOr(wid, wid > 0), LL_ReplayWriter.IntOr(distance, distance >= 0)));

		LL_ReplayEntityRecord record = m_Entities.RecordOf(victimEid);
		if (record)
			record.m_bUnconscious = false;
	}

	void SecondStep_S()
	{
		foreach (int eid, LL_ReplayShotCounter counter : m_mShots)
		{
			if (counter.m_iCount == 0)
				continue;

			m_Writer.Add(string.Format("F %1 %2 %3 %4", eid, counter.m_iCount, counter.m_iYaw, LL_ReplayWriter.IntOr(counter.m_iWid, counter.m_iWid > 0)));
			counter.m_iCount = 0;
		}
	}

	void WriteSnapshotDictionaries_S()
	{
		foreach (int wid, string weaponLine : m_mWeaponLines)
		{
			m_Writer.AddSnapshotLine(weaponLine);
		}
	}

	string TakeShotCounts()
	{
		string counts = string.Format("playerShots=%1 aiShots=%2", m_iPlayerShots, m_iAiShots);
		m_iPlayerShots = 0;
		m_iAiShots = 0;
		return counts;
	}

	void CaptureContinuation(notnull LL_ReplayContinuation continuation)
	{
		continuation.nextWid = m_iNextWid;
		foreach (string key, int wid : m_mWidByKey)
		{
			int separator = key.IndexOf("|");
			if (separator < 0)
				continue;

			LL_ReplayWeaponEntry entry = new LL_ReplayWeaponEntry();
			entry.type = key.Substring(0, separator);
			entry.name = key.Substring(separator + 1, key.Length() - separator - 1);
			entry.wid = wid;
			continuation.weapons.Insert(entry);
		}
	}

	void RestoreContinuation(notnull LL_ReplayContinuation continuation)
	{
		m_iNextWid = continuation.nextWid;
		foreach (LL_ReplayWeaponEntry entry : continuation.weapons)
		{
			RegisterWeapon(entry.type, entry.name, entry.wid);
		}
	}

	protected void OnExplosionAmmoEffect(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		if (!m_bListening)
			return;

		int shooterEid = m_Entities.EidOf(instigator.GetInstigatorEntity());
		int wid = -1;
		if (shooterEid > 0)
		{
			m_mLastWidByShooter.Find(shooterEid, wid);
			m_mLastExplosionByShooter.Set(shooterEid, GetGame().GetWorld().GetWorldTime());
		}

		WriteDetonation(outMat[0], KindForWeapon(wid), shooterEid, wid);
	}

	protected void Follow(IEntity projectile, string kind, int shooterEid, int wid, float now)
	{
		if (m_aFollowed.Count() >= MAX_FOLLOWED)
			return;

		LL_ReplayFollowed followed = new LL_ReplayFollowed();
		followed.m_Entity = projectile;
		followed.m_vLast = projectile.GetOrigin();
		followed.m_sKind = kind;
		followed.m_iShooter = shooterEid;
		followed.m_iWid = wid;
		followed.m_fShotTime = now;
		m_aFollowed.Insert(followed);

		if (!m_bFollowing)
		{
			m_bFollowing = true;
			GetGame().GetCallqueue().CallLater(FollowStep_S, 0, true);
		}
	}

	protected void FollowStep_S()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = m_aFollowed.Count() - 1; i >= 0; i--)
		{
			LL_ReplayFollowed followed = m_aFollowed[i];
			if (followed.m_Entity)
			{
				vector position = followed.m_Entity.GetOrigin();
				ProjectileMoveComponent move = ProjectileMoveComponent.Cast(followed.m_Entity.FindComponent(ProjectileMoveComponent));
				if (move)
					followed.m_vVelocity = move.GetVelocity();

				followed.m_vLast = position;
				continue;
			}

			float exploded;
			bool reported = followed.m_iShooter > 0 && m_mLastExplosionByShooter.Find(followed.m_iShooter, exploded)
				&& exploded >= followed.m_fShotTime && now - exploded <= SAME_DETONATION_MS;
			if (!reported)
				WriteDetonation(RefineImpact(followed), followed.m_sKind, followed.m_iShooter, followed.m_iWid);

			m_aFollowed.Remove(i);
		}

		if (m_aFollowed.IsEmpty() && m_bFollowing)
		{
			m_bFollowing = false;
			GetGame().GetCallqueue().Remove(FollowStep_S);
		}
	}

	protected vector RefineImpact(LL_ReplayFollowed followed)
	{
		float speed = followed.m_vVelocity.Length();
		if (speed < 0.1)
			return followed.m_vLast;

		TraceParam param = new TraceParam();
		param.Start = followed.m_vLast;
		param.End = followed.m_vLast + followed.m_vVelocity.Normalized() * (speed * 0.05 + 2);
		param.Flags = TraceFlags.ENTS | TraceFlags.OCEAN | TraceFlags.WORLD;

		float fraction = GetGame().GetWorld().TraceMove(param, null);
		return param.Start + (param.End - param.Start) * fraction;
	}

	protected void WriteDetonation(vector position, string kind, int shooterEid, int wid)
	{
		int x = Math.Round(position[0]);
		int z = Math.Round(position[2]);
		int alt = Math.Round(position[1]);
		m_Writer.Add(string.Format("B %1 %2 %3 %4 %5 %6", x, z, alt, kind,
			LL_ReplayWriter.IntOr(shooterEid, shooterEid > 0), LL_ReplayWriter.IntOr(wid, wid > 0)));
	}

	protected IEntity KnockOutAttacker(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;

		SCR_DamageManagerComponent damage = chimera.GetDamageManager();
		if (!damage)
			return null;

		return damage.GetInstigator().GetInstigatorEntity();
	}

	protected void Describe(IEntity victim, IEntity attacker, out string cause, out int wid, out int distance)
	{
		cause = LastDamageType(victim);
		wid = -1;
		distance = -1;
		if (!attacker)
			return;

		int attackerEid = m_Entities.EidOf(attacker);
		float shotTime;
		if (attackerEid > 0 && m_mLastShotTimeByShooter.Find(attackerEid, shotTime)
			&& GetGame().GetWorld().GetWorldTime() - shotTime <= WEAPON_MEMORY_MS)
			m_mLastWidByShooter.Find(attackerEid, wid);
		else
			wid = CurrentWeaponId(attacker);

		if (victim)
		{
			float metres = vector.Distance(attacker.GetOrigin(), victim.GetOrigin());
			distance = Math.Round(metres);
		}
	}

	protected string LastDamageType(IEntity victim)
	{
		if (!victim)
			return "";

		ExtendedDamageManagerComponent damage = ExtendedDamageManagerComponent.Cast(victim.FindComponent(ExtendedDamageManagerComponent));
		if (!damage)
			return "";

		array<ref BaseDamageEffect> history = {};
		damage.GetDamageHistory(history);
		for (int i = history.Count() - 1; i >= 0; i--)
		{
			BaseDamageEffect effect = history[i];
			if (!effect)
				continue;

			EDamageType type = effect.GetDamageType();
			if (type == EDamageType.BLEEDING && effect.GetTotalDamage() < 1)
				continue;

			string name = typename.EnumToString(EDamageType, type);
			name.ToLower();
			return name;
		}

		return "";
	}

	protected int CurrentWeaponId(IEntity attacker)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(attacker);
		if (!chimera)
			return -1;

		CompartmentAccessComponent access = chimera.GetCompartmentAccessComponent();
		if (access)
		{
			BaseCompartmentSlot slot = access.GetCompartment();
			if (slot)
			{
				TurretControllerComponent turret = TurretControllerComponent.Cast(slot.GetController());
				if (!turret)
					turret = slot.GetAttachedTurret();

				if (turret && turret.GetWeaponManager())
					return WeaponId(turret.GetWeaponManager().GetCurrentWeapon());
			}
		}

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (controller && controller.GetWeaponManagerComponent())
			return WeaponId(controller.GetWeaponManagerComponent().GetCurrentWeapon());

		return -1;
	}

	protected int WeaponId(BaseWeaponComponent weapon)
	{
		if (!weapon)
			return -1;

		string name;
		UIInfo info = weapon.GetUIInfo();
		if (info)
			name = info.GetName();

		if (name == "")
			name = LL_ReplayEntities.PrefabFileName(weapon.GetOwner());

		string type = WeaponTypeName(weapon.GetWeaponType());
		int wid;
		if (m_mWidByKey.Find(type + "|" + name, wid))
			return wid;

		wid = m_iNextWid;
		m_iNextWid++;
		RegisterWeapon(type, name, wid);
		m_Writer.Add(m_mWeaponLines.Get(wid));
		return wid;
	}

	protected void RegisterWeapon(string type, string name, int wid)
	{
		m_mWidByKey.Set(type + "|" + name, wid);
		m_mWeaponTypeByWid.Set(wid, type);
		m_mWeaponLines.Set(wid, string.Format("W %1 %2 %3", wid, type, LL_ReplayWriter.Str(name)));
	}

	protected string KindForWeapon(int wid)
	{
		string type;
		m_mWeaponTypeByWid.Find(wid, type);
		if (type == "rocketlauncher")
			return "rocket";
		if (type == "grenadelauncher" || type == "fraggrenade")
			return "grenade";

		return "shell";
	}

	protected static string WeaponTypeName(EWeaponType type)
	{
		string name = typename.EnumToString(EWeaponType, type);
		name.ToLower();
		if (name.IndexOf("wt_") == 0)
			name = name.Substring(3, name.Length() - 3);

		if (name == "rifle" || name == "sniperrifle" || name == "machinegun" || name == "handgun" || name == "grenadelauncher"
			|| name == "rocketlauncher" || name == "fraggrenade" || name == "smokegrenade" || name == "autocannon")
			return name;

		return "other";
	}

	protected static int ShotYaw(IEntity shooter, IEntity projectile)
	{
		if (projectile)
		{
			ProjectileMoveComponent move = ProjectileMoveComponent.Cast(projectile.FindComponent(ProjectileMoveComponent));
			if (move)
			{
				vector velocity = move.GetVelocity();
				if (velocity.Length() > 0.1)
				{
					vector angles = velocity.VectorToAngles();
					return NormalizedYaw(angles[0]);
				}
			}
		}

		if (shooter)
		{
			vector facing = shooter.GetYawPitchRoll();
			return NormalizedYaw(facing[0]);
		}

		return 0;
	}

	protected static int NormalizedYaw(float degrees)
	{
		int yaw = Math.Round(degrees);
		return ((yaw % 360) + 360) % 360;
	}

	protected static string Relation(notnull SCR_InstigatorContextData instigatorContextData)
	{
		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.NOT_A_CHARACTER))
			return "vehicle";
		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_AI | SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER))
			return "enemy";
		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_AI | SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER))
			return "friendly";
		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.SUICIDE))
			return "suicide";
		if (instigatorContextData.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_UNLIMITED_EDITOR))
			return "editor";

		return "none";
	}
}
