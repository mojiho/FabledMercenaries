#include "CombatSim.h"
#include <algorithm>	// std::min
#include <utility>		// std::move
#include "Class.h"

// <summary>
// 등록: Unit. id는 외부에서 발급, Sim은 단순 등록만. Brain은 move로 소유권 이전.
// </summary>
Unit& CombatSim::AddUnit(uint64_t id, uint64_t ownerId, Faction faction, Class cls, const Vec3& pos, std::unique_ptr<Brain> brain)
{
	Unit u;
	u.id = id;
	u.ownerId = ownerId;
	u.faction = faction;
	u.unitClass = cls;

	ClassStats st = GetClassStats(cls);          // ★ 주입은 emplace(이동) '전에' 해야 저장 유닛에 반영됨
	u.moveSpeed       = st.moveSpeed;
	u.attackRange     = st.attackRange;
	u.ranged          = st.ranged;
	u.attackPreDelay  = st.attackPreDelay;
	u.attackPostDelay = st.attackPostDelay;
	u.attackDamage    = st.attackDamage;
	u.maxHp           = st.maxHp;
	u.hp              = st.maxHp;   // 시작 체력 = 최대
	u.skills          = GetClassSkills(cls);

	u.pos = pos;
	u.brain = std::move(brain);                 // 소유권 이전

	auto [it, ok] = _units.emplace(id, std::move(u));  // Unit은 이동전용 → move
	return it->second;
}

// <summary>
// 등록: Commander. playerId는 외부에서 발급, Sim은 단순 등록만.
// </summary>
Commander& CombatSim::AddCommander(uint64_t playerId)
{
	Commander c;
	c.id = playerId;
	auto [it, ok] = _commanders.emplace(playerId, c);
	return it->second;
}

// <summary>
// 검색: Unit. id로 검색, 없으면 null
// </summary>
Unit* CombatSim::GetUnit(uint64_t id)
{
	auto it = _units.find(id);
	return it == _units.end() ? nullptr : &it->second;
}

// 결정적 난수 [0,1) — xorshift32 (std::rand 대신 → 나중에 서버/클라 동일 결과)
float CombatSim::Rand01()
{
	_rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
	return (_rng & 0xFFFFFFu) / 16777216.f;
}

// 피격 시 방어 패시브 롤. 쿨 찬 방어 스킬이 성공하면 쿨다운 시작 후 true.
bool CombatSim::TryDefend(Unit& defender)
{
	// 방어 태세: 액티브 Defense 스킬의 차단 확률로 막음(쿨 무시, 정면 한정은 ResolveHit에서)
	if (defender.defendStance)
	{
		for (auto& s : defender.skills)
			if (s.category == SkillCategory::Active && s.type == SkillType::Defense)
				return Rand01() < s.chance;
		return false;
	}

	for (auto& s : defender.skills)
	{
		if (s.category != SkillCategory::Passive) continue;
		if (s.type     != SkillType::Defense)     continue;
		if (s.cdRemaining > 0.f)                  continue;   // 쿨다운 중 → 방어 불가
		if (Rand01() < s.chance)
		{
			s.cdRemaining = s.cooldown;                       // 방어 성공 → 쿨다운 시작
			return true;
		}
	}
	return false;
}

// <summary>
// 명령 발급: Unit id로 명령 발급. Unit이 없으면 Rejected. Unit이 수행 중이면 Queued. 성공 시 Accepted.
// </summary>
CommandResult CombatSim::IssueCommand(uint64_t unitId, Command cmd, bool reserve)
{
	Unit* u = GetUnit(unitId);
	if (!u) return CommandResult::Rejected;

	// 이 유닛을 지휘하는 플레이어의 게이지 검사
	auto cit = _commanders.find(u->ownerId);
	if (cit == _commanders.end() || cit->second.cmdGauge < ISSUE_COST)
		return CommandResult::Rejected;
	cit->second.cmdGauge -= ISSUE_COST;

	cmd.slotId = NewSlot();

	if (reserve)
	{
		u->reserveQueue.push_back(std::move(cmd));   // 명시적 예약만 큐 적재
		return CommandResult::Queued;
	}

	u->current = std::move(cmd);                 // 즉시 대체(인터럽트)
	u->executing = true;
	u->execGauge = 0.f;
	u->curWaypoint = 0;
	u->attackFired = false;
	// 정책(확정): 기존 예약 큐는 유지(리셋 안 함)
	return CommandResult::Accepted;
}

CommandResult CombatSim::CancelCommand(uint64_t unitId, uint32_t slotId)
{
	Unit* u = GetUnit(unitId);
	if (!u) return CommandResult::Rejected;

	for (auto it = u->reserveQueue.begin(); it != u->reserveQueue.end(); ++it)
	{
		if (it->slotId == slotId)
		{
			u->reserveQueue.erase(it);
			return CommandResult::Cancelled;          // 예약 슬롯 제거
		}
	}
	return CommandResult::Rejected;                   // 이미 수행 중(current) → 취소 불가
}

void CombatSim::CancelAllReserved(uint64_t unitId)
{
	if (Unit* u = GetUnit(unitId))
		u->reserveQueue.clear();
}

// ---------- 틱 루프 ----------
void CombatSim::Tick(float dt)
{
	// 1) 지휘관 게이지 충전 (플레이어 단위)
	for (auto& [pid, c] : _commanders)
		c.cmdGauge = std::min(c.cmdMax, c.cmdGauge + c.cmdRate * dt);

	// 1.5) 스킬 쿨다운 감소 (모든 유닛의 모든 스킬, 행동 여부 무관)
	for (auto& [id, u] : _units)
		for (auto& s : u.skills)
			if (s.cdRemaining > 0.f)
				s.cdRemaining = std::max(0.f, s.cdRemaining - dt);

	// 1.6) 스턴 감소
	for (auto& [id, u] : _units)
		if (u.stunRemaining > 0.f)
			u.stunRemaining = std::max(0.f, u.stunRemaining - dt);

	// 2) 의사결정 — brain 있는 유닛만(AI). 사람 조종은 brain==null → 건너뜀.
	for (auto& [id, u] : _units)
		if (u.brain)
			u.brain->Decide(*this, u, dt);

	// 3) 명령 수행
	for (auto& [id, u] : _units)
	{
		if (!u.alive) { u.executing = false; continue; }   // 사망 → 행동 정지
		if (u.stunRemaining > 0.f) continue;               // 스턴 중 행동 불가
		if (!u.executing) continue;
		u.execGauge += EXEC_RATE * dt;

		bool done = false;
		switch (u.current.type)
		{
		case CommandType::Move:
		{
			if (u.defendStance) break;   // 방어 태세 중엔 이동 불가
			if (u.curWaypoint >= (int)u.current.waypoints.size()) { done = true; break; } // 가드(빈 경로/끝)
			Vec3  target = u.current.waypoints[u.curWaypoint];
			Vec3  to = target - u.pos;
			float dist = to.Length();
			float step = u.moveSpeed * dt;
			if (dist <= step)                        // 웨이포인트 도달
			{
				u.pos = target;
				u.curWaypoint++;
				if (u.curWaypoint >= (int)u.current.waypoints.size())
					done = true;                     // 경로 끝
			}
			else
			{
				Vec3 dir = to.Normalized();
				u.pos    = u.pos + dir * step;
				u.facing = dir;                      // 이동 방향 바라봄(백어택 판정용)
			}
			break;
		}
		case CommandType::Attack:
		{
			Unit* target = GetUnit(u.current.targetId);
			if (!target || !target->alive) { done = true; break; }   // 대상 소멸
			
			float slow = 1.f;
			if (u.defendStance)
				for (auto& s : u.skills)
					if (s.category == SkillCategory::Active && s.type == SkillType::Defense) { slow = s.attackSlow; break; }
			float preD = u.attackPreDelay * slow;
			float postD = u.attackPostDelay * slow;

			Vec3  toTarget = target->pos - u.pos;
			float dist = toTarget.Length();
			if (dist > 0.f) u.facing = toTarget.Normalized();   // 공격 중엔 대상을 바라봄(후면 판정 정상화)
			if (dist > u.attackRange)                // 사거리 밖 → 진행 불가, 윈드업 리셋
			{
				u.execGauge = 0.f;
				u.attackFired = false;
				break;
			}

			// 선딜 완료 순간 1회 발사
			if (!u.attackFired && u.execGauge >= u.attackPreDelay)
			{
				u.attackFired = true;
				if (OnAttackFired) OnAttackFired(u.id);   // 휘두름/발사

				if (u.ranged)
					SpawnProjectile(u, *target, u.attackDamage, true);                       // 원거리: 투사체 발사(도달 시 판정)
				else if (ResolveHit(*target, u.pos, u.attackDamage))   // 근접: 즉시 판정
					done = true;                                       // 처치 시 명령 완료
			}

			// 선딜+후딜 끝 → 사이클 반복(연속 공격)
			if (u.execGauge >= u.attackPreDelay + u.attackPostDelay)
			{
				u.execGauge = 0.f;
				u.attackFired = false;
			}
			break;
		}
		case CommandType::Skill:
		{
			SkillType want = (SkillType)u.current.skillId;   // 명령이 지정한 스킬 종류
			Skill* sk = nullptr;
			for (auto& s : u.skills)
				if (s.category == SkillCategory::Active && s.type == want) { sk = &s; break; }
			if (!sk) { done = true; break; }                 // 그런 액티브 스킬 없음

			// 선딜 완료 순간 1회 발동
			if (!u.attackFired && u.execGauge >= sk->preDelay)
			{
				u.attackFired = true;

				if (sk->cdRemaining > 0.f || u.mp < sk->mpCost) { done = true; break; }  // 쿨/마나 부족 → 취소
				sk->cdRemaining = sk->cooldown;
				u.mp -= sk->mpCost;
				if (OnSkillCast) OnSkillCast(u.id, sk->type);   // 시전 이벤트

				if (sk->type == SkillType::Charge)
				{
					if (Unit* target = GetUnit(u.current.targetId))
					{
						Vec3  toT = target->pos - u.pos;
						float d = toT.Length();
						if (d > u.attackRange)
						{
							Vec3 dir = toT.Normalized();
							u.pos = u.pos + dir * (d - u.attackRange);  // 근접 사거리 앞까지 순간이동
							u.facing = dir;
						}
						target->stunRemaining = CHARGE_STUN;   // ★ 돌진 충돌 → 짧은 스턴(즉시 도망 방지)
					}
				}
				else if (sk->type == SkillType::MagicBolt)
				{
					if (Unit* target = GetUnit(u.current.targetId))
						SpawnProjectile(u, *target, sk->damage, false);   // 마법탄(직선), 데미지=스킬값
				}
				else if (sk->type == SkillType::Defense)
				{
					u.defendStance = !u.defendStance;   // 방어 태세 토글(플레이어 시전 경로)
				}
				else if (sk->type == SkillType::Heal)
				{
					if (Unit* ally = GetUnit(u.current.targetId))
						if (ally->alive)
							ally->hp = std::min(ally->maxHp, ally->hp + sk->damage);  // 회복(최대 HP 상한)
				}
			}

			// 후딜까지 끝 → 완료
			if (u.execGauge >= sk->preDelay + sk->postDelay)
				done = true;
			break;
		}
		case CommandType::Defend:
			u.defendStance = !u.defendStance;   // (호환) 방어 태세 토글
			done = true;
			break;

		case CommandType::Focus:
			u.mp += FOCUS_RATE * dt;                 // MP 충전 (제자리, 무방비)
			if (u.mp >= u.mpMax) { u.mp = u.mpMax; done = true; }   // 가득 차면 완료
			break;

		default:
			done = true;                             // None 등 → 완료 처리
			break;
		}

		if (done)
		{
			if (OnCommandComplete)
				OnCommandComplete(u.id, u.current.slotId);

			if (!u.reserveQueue.empty())             // 예약 승격
			{
				u.current = std::move(u.reserveQueue.front());
				u.reserveQueue.pop_front();
				u.execGauge = 0.f;
				u.curWaypoint = 0;
				u.executing = true;
				u.attackFired = false;
			}
			else
			{
				u.executing = false;                 // Idle
			}
		}
	}

	UpdateProjectiles(dt);
}

Unit* CombatSim::FindNearestHostile(const Unit& self)
{
	Unit* best = nullptr;
	float bestDist = 0.f;
	for (auto& [id, u] : _units)
	{
		if (u.id == self.id)           continue;
		if (!u.alive)                  continue;
		if (u.faction == self.faction) continue;   // 같은 진영 제외
		float d = (u.pos - self.pos).Length();
		if (!best || d < bestDist) { best = &u; bestDist = d; }
	}
	return best;
}

bool CombatSim::ResolveHit(Unit& target, const Vec3& fromPos, float damage)
{
	if (!target.alive) return false;
	bool fromBehind = (fromPos - target.pos).Normalized().Dot(target.facing) < 0.f;
	bool defended = (!fromBehind) && TryDefend(target); // 후면은 못 막음(방어 무시). 정면일 때만 방어 롤.
	if (defended) return false;

	float dmg = damage * (fromBehind ? CRIT_MULT : 1.f);   // 후면=크리 배율
	target.hp -= dmg;
	if (OnDamaged) OnDamaged(target.id, fromBehind, fromBehind);

	// 원거리 조준 중 피격 → 조준 풀림
	if (target.ranged && target.executing && target.current.type == CommandType::Attack && !target.attackFired)
	{
		target.execGauge = 0.f;   // 원거리 조준 중 피격 → 조준 풀림
	}

	if (!target.executing)
	{
		Vec3 toAttacker = fromPos - target.pos; toAttacker.z = 0.f;
		if (toAttacker.Length() > 0.f)
			target.facing = toAttacker.Normalized();
	}

	if (target.hp <= 0.f)
	{
		target.hp = 0.f;
		target.alive = false;
		if (OnDeath) OnDeath(target.id);
		return true;   // 사망
	}

	return false;
}

void CombatSim::SpawnProjectile(Unit& owner, Unit& target, float damage, bool arc)
{
	Projectile p;
	p.id = ++_projGen;
	p.ownerId = owner.id;
	p.targetId = target.id;
	p.damage = damage;
	p.pos = owner.pos;
	p.start = owner.pos;
	p.startDist = (target.pos - owner.pos).Length();
	p.arc = arc;
	p.arcHeight = arc ? p.startDist * 0.25f : 0.f;
	p.ownerFaction = owner.faction;
	_projectiles.push_back(p);
}

void CombatSim::UpdateProjectiles(float dt)
{
	const float HIT_RADIUS = 30.f;
	for (auto& p : _projectiles)
	{
		if (!p.alive) continue;

		// 경로상 '소유자와 다른 진영' 유닛 중 가장 가까운 것에 충돌 (아군은 관통 → 몸빵 가능)
		Unit* hit = nullptr;
		float best = HIT_RADIUS;
		for (auto& [id, u] : _units)
		{
			if (!u.alive) continue;
			if (u.faction == p.ownerFaction) continue;   // 같은 편 관통
			Vec3 d = u.pos - p.pos; d.z = 0.f;
			float dd = d.Length();
			if (dd <= best) { best = dd; hit = &u; }
		}
		if (hit)
		{
			ResolveHit(*hit, p.pos, p.damage);   // 가로챈 유닛(또는 목표)이 맞음 — 방어/크리는 ResolveHit에서
			p.alive = false;
			continue;
		}

		// 아직 안 맞았으면 목표 쪽으로 비행
		Unit* target = GetUnit(p.targetId);
		if (!target || !target->alive) { p.alive = false; continue; }
		Vec3 to = target->pos - p.pos; to.z = 0.f;
		p.pos = p.pos + to.Normalized() * (p.speed * dt);

		// 포물선 높이(궤적용 z)
		if (p.arc && p.startDist > 0.f)
		{
			float prog = (p.pos - p.start).Length() / p.startDist;
			if (prog > 1.f) prog = 1.f;
			p.pos.z = p.arcHeight * 4.f * prog * (1.f - prog);
		}
	}
	_projectiles.erase(std::remove_if(_projectiles.begin(), _projectiles.end(),
		[](const Projectile& p) { return !p.alive; }), _projectiles.end());
}
