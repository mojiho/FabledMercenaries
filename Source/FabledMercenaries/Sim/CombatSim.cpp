#include "CombatSim.h"
#include <algorithm>	// std::min
#include <utility>		// std::move
#include "Class.h"
#include "Item.h"

// ---------- 지형 보조 ----------

bool CombatSim::HasLineOfSight(const Vec3& from, const Vec3& to) const
{
	if (!_nav || !_nav->Valid()) return true;   // 격자 없음 = 전부 트임
	return _nav->LineOfSight(from, to);
}

bool CombatSim::FindVantagePoint(const Unit& self, const Unit& target, Vec3& out) const
{
	if (!_nav || !_nav->Valid()) return false;
	if (!self.useNavGrid)        return false;

	int cx = 0, cy = 0;
	_nav->WorldToCell(self.pos, cx, cy);   // 격자 밖이면 클램프된 칸이 들어온다

	const int   w = _nav->Width();
	const int   h = _nav->Height();
	const float r2 = self.attackRange * self.attackRange;

	// 가까운 링부터 본다 — 처음 찾은 링이 '가장 가까운 자리'다.
	// 같은 링 안에서는 대상에 가까운 쪽, 그래도 같으면 칸 인덱스가 낮은 쪽을 고른다.
	// 이 타이브레이크가 없으면 같은 입력에서 서버와 다른 자리를 골라 위치가 갈린다.
	for (int ring = 1; ring <= VANTAGE_MAX_RING; ++ring)
	{
		bool     bFound  = false;
		Vec3     best{};
		float    bestD2  = 0.f;
		int32_t  bestIdx = 0;

		for (int dy = -ring; dy <= ring; ++dy)
		for (int dx = -ring; dx <= ring; ++dx)
		{
			// 링 테두리만 — 안쪽은 이전 링에서 이미 봤다
			const int ax = dx < 0 ? -dx : dx;
			const int ay = dy < 0 ? -dy : dy;
			if ((ax > ay ? ax : ay) != ring) continue;

			const int nx = cx + dx;
			const int ny = cy + dy;
			if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;   // 격자 밖은 '열린 지형'이라
			if (!_nav->IsWalkable(nx, ny))              continue;   // CellToWorld가 의미를 잃는다

			const Vec3  c   = _nav->CellToWorld(nx, ny);
			const float ddx = target.pos.x - c.x;
			const float ddy = target.pos.y - c.y;
			const float d2  = ddx * ddx + ddy * ddy;

			if (d2 > r2)                                continue;   // 거기 서도 사거리 밖
			if (!_nav->LineOfSight(c, target.pos))      continue;   // 거기서도 가려짐
			if (!_nav->CanWalkStraight(self.pos, c))    continue;   // 똑바로 못 가는 자리는 제외

			const int32_t idx = ny * w + nx;
			if (!bFound || d2 < bestD2 || (d2 == bestD2 && idx < bestIdx))
			{
				bFound  = true;
				best    = c;
				bestD2  = d2;
				bestIdx = idx;
			}
		}

		if (bFound)
		{
			best.z = self.pos.z;   // z는 상위 레이어가 지형 높이로 다시 깐다
			out = best;
			return true;
		}
	}
	return false;
}

void CombatSim::ResetNavState(Unit& u)
{
	u.navPath.clear();
	u.navIndex = 0;
	u.repathTimer = 0.f;
	u.hasVantage = false;
	u.vantageTimer = 0.f;
	u.moveStuckTimer = 0.f;
	u.moveProgDist   = -1.f;   // 새 목표 — 지난 명령의 남은 거리와 비교하면 안 된다
}

void CombatSim::BuildPath(const Unit& u, Command& cmd) const
{
	if (!_nav || !_nav->Valid()) return;   // 격자 없음 — 요청 경로를 그대로 쓴다
	if (!u.useNavGrid)           return;
	if (cmd.waypoints.empty())   return;

	std::vector<Vec3> routed;
	routed.reserve(cmd.waypoints.size() * 2);

	Vec3 at = u.pos;
	std::vector<Vec3> leg;

	for (const Vec3& wp : cmd.waypoints)
	{
		if (!_nav->FindPath(at, wp, leg))
			break;   // 여기부터 길이 없다 — 앞 구간까지만 살린다

		for (const Vec3& p : leg) routed.push_back(p);
		if (!leg.empty()) at = leg.back();
	}

	// 첫 구간부터 실패했으면(갈 수 있는 길이 아예 없는 목표) 요청 좌표를 그대로 둔다.
	// 유닛은 직선으로 밀고 들어가다 ResolveStep에 걸려 벽을 따라 미끄러지며 멈춘다 —
	// 명령을 통째로 버리면 플레이어 입장에선 클릭이 조용히 씹힌 것처럼 느껴진다.
	// (FindPath는 막힌 도착점을 이미 설 수 있는 칸으로 당겨 보므로, 여기까지 오면
	//  '설 수는 있지만 닿을 수 없는' 목표다 — 섬 건너편 같은 경우)
	if (!routed.empty()) cmd.waypoints = std::move(routed);
}

Vec3 CombatSim::ResolveStep(const Unit& u, const Vec3& desired) const
{
	// z는 Sim의 판정에 쓰이지 않는다(상위 레이어가 지형 높이로 다시 깐다) — 그대로 들고 간다
	const Vec3 want{ desired.x, desired.y, u.pos.z };

	if (!_nav || !_nav->Valid())     return want;
	if (!u.useNavGrid)               return want;
	if (_nav->IsWalkableAt(want))    return want;

	// 벽에 닿았다 — 축을 하나씩 떼어 미끄러진다.
	// 경로가 최신이면 거의 안 타지만, 추격처럼 직선으로 밀고 들어갈 때 벽에 끼는 걸 막아 준다.
	const Vec3 slideX{ desired.x, u.pos.y, u.pos.z };
	if (_nav->IsWalkableAt(slideX))  return slideX;

	const Vec3 slideY{ u.pos.x, desired.y, u.pos.z };
	if (_nav->IsWalkableAt(slideY))  return slideY;

	return u.pos;   // 완전히 막힘 → 제자리
}

Vec3 CombatSim::LastWalkableAlong(const Vec3& from, const Vec3& to) const
{
	if (!_nav || !_nav->Valid()) return to;

	Vec3 d = to - from;
	d.z = 0.f;
	const float len = d.Length();
	if (len <= 0.f) return from;

	const Vec3  dir  = d.Normalized();
	const float inc  = _nav->CellSize() * 0.5f;   // 반 칸씩 — 얇은 벽을 건너뛰지 않을 정도로 촘촘히

	Vec3 best = from;
	for (float t = inc; t <= len; t += inc)
	{
		Vec3 p = from + dir * t;
		p.z = from.z;
		if (!_nav->IsWalkableAt(p)) break;
		best = p;
	}
	return best;
}

void CombatSim::ChaseStep(Unit& u, const Vec3& goal, float dt)
{
	const float step = u.moveSpeed * dt;
	if (step <= 0.f) return;

	u.repathTimer -= dt;

	// 걸어서 똑바로 갈 수 있으면 A*를 돌리지 않는다 — 추격의 대부분이 여기서 끝난다
	const bool bStraight = !_nav || !_nav->Valid() || !u.useNavGrid
		|| _nav->CanWalkStraight(u.pos, goal);
	if (bStraight)
	{
		ResetNavState(u);

		Vec3 to = goal - u.pos;
		to.z = 0.f;
		const float d = to.Length();
		if (d <= 0.f) return;

		const Vec3 dir = to.Normalized();
		u.facing = dir;
		u.pos = ResolveStep(u, u.pos + dir * std::min(step, d));
		return;
	}

	// 우회가 필요하다. 대상이 움직이므로 간격을 두고 경로를 다시 깐다.
	if (u.navPath.empty() || u.navIndex >= (int)u.navPath.size() || u.repathTimer <= 0.f)
	{
		u.repathTimer = REPATH_INTERVAL;   // 실패해도 간격은 둔다 — 매 틱 재시도하면 A*가 틱을 먹는다
		if (_nav->FindPath(u.pos, goal, u.navPath))
			u.navIndex = 0;
		else
		{
			u.navPath.clear();
			u.navIndex = 0;
			return;                        // 갈 길이 없다 — 이번 틱은 제자리
		}
	}

	if (u.navIndex >= (int)u.navPath.size()) return;

	Vec3 to = u.navPath[u.navIndex] - u.pos;
	to.z = 0.f;
	const float d = to.Length();

	if (d <= step)
	{
		u.pos = ResolveStep(u, u.navPath[u.navIndex]);
		u.navIndex++;
	}
	else
	{
		const Vec3 dir = to.Normalized();
		u.facing = dir;
		u.pos = ResolveStep(u, u.pos + dir * step);
	}
}

// ---------- 몸 겹침 ----------

void CombatSim::SeparateUnits(float dt)
{
	if (dt <= 0.f) return;

	// 시체는 자리를 차지하지 않는다 — 시신 위로 걸어간다.
	// 포인터를 들고 도는 게 안전한 이유: _units는 노드 기반(unordered_map)이라
	// 이 패스 안에서 삽입/삭제가 없으면 주소가 움직이지 않는다.
	_pushList.clear();
	for (auto& [id, u] : _units)
		if (u.alive && u.bodyRadius > 0.f)
			_pushList.push_back(&u);

	if (_pushList.size() < 2) return;

	std::sort(_pushList.begin(), _pushList.end(),
		[](const Unit* a, const Unit* b) { return a->id < b->id; });

	const size_t n = _pushList.size();
	_pushDelta.assign(n, Vec3{ 0.f, 0.f, 0.f });

	// 전부 대 전부. 전장 유닛이 수십 명 규모라 이걸로 충분하다 —
	// 수백 명이 되면 격자 버킷으로 바꿀 자리가 여기다.
	for (size_t i = 0; i + 1 < n; ++i)
	{
		Unit& a = *_pushList[i];
		for (size_t j = i + 1; j < n; ++j)
		{
			Unit& b = *_pushList[j];

			const float want = a.bodyRadius + b.bodyRadius;
			const float dx = b.pos.x - a.pos.x;
			const float dy = b.pos.y - a.pos.y;
			const float d2 = dx * dx + dy * dy;
			if (d2 >= want * want) continue;   // 안 겹침

			Vec3  dir;
			float d = sqrtf(d2);
			if (d > 1e-4f)
			{
				dir = Vec3{ dx / d, dy / d, 0.f };
			}
			else
			{
				// 좌표가 완전히 같다 — 밀어낼 방향을 뽑을 수 없다.
				// id로 각도를 정해 고정적으로 흩는다. 난수를 쓰면 _rng가 돌아가
				// 같은 입력에서 전투 판정(방어/불복종)까지 달라진다.
				const uint32_t h = (uint32_t)(a.id * 2654435761u) ^ (uint32_t)(b.id * 40503u);
				const float    ang = (float)(h % 3600u) * 0.0017453293f;   // 0.1도 단위
				dir = Vec3{ cosf(ang), sinf(ang), 0.f };
				d = 0.f;
			}

			// 질량 = 반경². 큰 쪽이 덜 밀린다(탱커가 버티는 느낌)
			const float ma  = a.bodyRadius * a.bodyRadius;
			const float mb  = b.bodyRadius * b.bodyRadius;
			const float sum = ma + mb;
			const float shareA = (sum > 0.f) ? (mb / sum) : 0.5f;   // a가 받는 몫

			const float push = (want - d) * PUSH_RATE;
			_pushDelta[i] = _pushDelta[i] - dir * (push * shareA);
			_pushDelta[j] = _pushDelta[j] + dir * (push * (1.f - shareA));
		}
	}

	for (size_t i = 0; i < n; ++i)
	{
		Unit& u = *_pushList[i];

		Vec3 delta = _pushDelta[i];
		delta.z = 0.f;
		const float len = delta.Length();
		if (len <= 1e-4f) continue;

		// 걷는 속도보다 빨리 밀려나면 얼음판에서 미끄러지는 것처럼 보인다.
		// 여러 명에게 동시에 끼인 유닛이 튀어나가는 것도 이 상한이 막는다.
		const float cap = u.moveSpeed * dt;
		if (cap > 0.f && len > cap) delta = delta * (cap / len);

		// 벽 안쪽으로 밀려들어가면 갇힌다 — 이동과 같은 길로 해소한다
		u.pos = ResolveStep(u, u.pos + delta);
	}
}

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
	u.bodyRadius      = st.bodyRadius;
	u.skills          = GetClassSkills(cls);
	u.cost            = st.cost;
	u.turnGauge       = u.turnMax;   // 턴테이블을 채워 시작 — 전투 시작 직후 첫 명령이 바로 먹힌다
	if (auto cit = _commanders.find(ownerId); cit != _commanders.end())
		cit->second.usedCost += u.cost;   // 지휘관 소속이면 코스트 합산 (★ emplace 전에!)

	u.pos = pos;
	u.brain = std::move(brain);                 // 소유권 이전

	auto [it, ok] = _units.emplace(id, std::move(u));  // Unit은 이동전용 → move
	return it->second;
}

// <summary>
// 장비 적용: 보너스만큼 공격력·최대체력을 올리고, 체력도 같은 양만큼 채운다(풀피 유지).
// 이후 호출부가 지난 전투 체력으로 hp를 덮어쓰는 건 자유.
// </summary>
void CombatSim::ApplyEquipment(uint64_t id, const uint32_t (&equip)[EQUIP_SLOT_COUNT])
{
	Unit* u = GetUnit(id);
	if (!u) return;
	const EquipBonus b = SumEquipBonus(equip);
	u->attackDamage += b.atk;
	u->maxHp        += b.hp;
	u->hp           += b.hp;
}

// <summary>
// 등록: Commander. playerId는 외부에서 발급, Sim은 단순 등록만.
// </summary>
bool CombatSim::RemoveUnit(uint64_t id)
{
	auto it = _units.find(id);
	if (it == _units.end()) return false;

	// 지휘관 코스트 환원 (0 밑으로 내려가지 않게)
	if (auto cit = _commanders.find(it->second.ownerId); cit != _commanders.end())
	{
		cit->second.usedCost -= it->second.cost;
		if (cit->second.usedCost < 0.f) cit->second.usedCost = 0.f;
	}

	_units.erase(it);

	// 이 유닛이 쏜/이 유닛을 노리던 발사체는 무효화 — 사라진 id를 계속 추적하면 안 된다
	for (Projectile& p : _projectiles)
		if (p.ownerId == id || p.targetId == id)
			p.alive = false;

	// 이 유닛을 대상으로 하던 남의 명령은 GetUnit()이 null을 돌려줘 자동으로 완료 처리된다.
	return true;
}

Commander& CombatSim::AddCommander(uint64_t playerId, CommanderType type)
{
	Commander c = MakeCommander(playerId, type);
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
DefenseKind CombatSim::TryDefend(Unit& defender)
{
	// 방어 태세: 액티브 Defense 스킬의 차단 확률로 막음(쿨 무시, 정면 한정은 ResolveHit에서)
	if (defender.defendStance)
	{
		for (auto& s : defender.skills)
			if (s.category == SkillCategory::Active && s.type == SkillType::Defense)
				return (Rand01() < s.chance) ? s.defenseKind : DefenseKind::None;
		return DefenseKind::None;
	}

	for (auto& s : defender.skills)
	{
		if (s.category != SkillCategory::Passive) continue;
		if (s.type     != SkillType::Defense)     continue;
		if (s.cdRemaining > 0.f)                  continue;   // 쿨다운 중 → 방어 불가
		if (Rand01() < s.chance)
		{
			s.cdRemaining = s.cooldown;                       // 방어 성공 → 쿨다운 시작
			return s.defenseKind;
		}
	}
	return DefenseKind::None;
}

// <summary>
// 명령 발급: Unit id로 명령 발급. Unit이 없으면 Rejected. Unit이 수행 중이면 Queued. 성공 시 Accepted.
// </summary>
CommandResult CombatSim::IssueCommand(uint64_t unitId, Command cmd, bool reserve)
{
	Unit* u = GetUnit(unitId);
	if (!u) return CommandResult::Rejected;

	// 플레이어 명령만 지휘관 게이지·불복종 적용. AI(브레인) 반격은 자원 무소모 → 항상 발동.
	// (덕분에 반격이 플레이어 게이지를 갉아먹지 못해, 플레이어 명령이 언제나 우선)
	if (!_aiIssuing)
	{
		// 이 유닛의 턴테이블이 돌아야 명령을 받는다. 수행 중인지는 보지 않는다 — 인터럽트는 허용.
		if (!u->TurnReady()) return CommandResult::Rejected;

		auto cit = _commanders.find(u->ownerId);
		if (cit == _commanders.end() || cit->second.cmdGauge < ISSUE_COST)
			return CommandResult::Rejected;
		cit->second.cmdGauge -= ISSUE_COST;
		u->turnGauge = 0.f;   // 턴 소모 — 아래 불복종으로 날아가도 소모된 것으로 친다

		// Cost 초과 -> 용병 불복종 (게이지는 이미 소모됨)
		Commander& cm = cit->second;
		float over = cm.usedCost - cm.costBudget;
		if (over > 0.f)
		{
			float disobey = std::min(DISOBEY_MAX, (over / cm.costBudget) * DISOBEY_SLOPE);
			if (Rand01() < disobey) return CommandResult::Disobeyed;   // 용병이 명령 무시
		}
	}

	cmd.slotId = NewSlot();

	// 장애물을 피하는 실제 경로로 바꿔 끼운다. 예약 명령도 여기서 미리 깔아 둔다 —
	// 예약은 '나중에' 수행되므로 그때 지형이 달라질 수 있지만, 지금 이 게임의 지형은 정적이다.
	// (파괴 가능한 벽이 생기면 승격 시점에 다시 깔아야 한다)
	if (cmd.type == CommandType::Move)
		BuildPath(*u, cmd);

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
	ResetNavState(*u);                           // 지난 명령의 우회 경로를 물고 가지 않게
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

	// 1.7) 턴테이블 충전 — 명령 수행 중인지와 무관하게 돈다.
	// 수행 중에도 새 명령으로 끊을 수 있어야 하므로 게이지를 수행 상태에 묶지 않는다.
	// 죽은 유닛은 돌리지 않는다(시체에 '명령 가능' 테두리가 뜨면 안 된다).
	for (auto& [id, u] : _units)
		u.turnGauge = u.alive ? std::min(u.turnMax, u.turnGauge + u.turnRate * dt) : 0.f;

	// 2) 의사결정 — brain 있는 유닛만(AI). 사람 조종은 brain==null → 건너뜀.
	_aiIssuing = true;                     // 이 구간의 IssueCommand는 AI 명령 → 게이지 무소모
	for (auto& [id, u] : _units)
		if (u.brain && u.alive)            // 사망자는 판단하지 않는다 (죽은 채 스폰된 용병 포함)
			u.brain->Decide(*this, u, dt);
	_aiIssuing = false;

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

			// 경로를 다 걸었나. 빈 경로로 들어온 명령도 여기서 걸러진다 —
			// 예전엔 이 검사 뒤에 break가 없어서 빈 vector를 인덱싱했다.
			if (u.curWaypoint >= (int)u.current.waypoints.size())
			{
				done = true;                         // 경로 끝
				if (u.current.hasArriveFacing)       // 도착 방향 지정됐으면 그쪽 봄
					u.facing = u.current.arriveFacing;
				break;
			}

			const Vec3 target = u.current.waypoints[u.curWaypoint];
			Vec3  to = target - u.pos;
			to.z = 0.f;                              // 거리는 평면으로 잰다 (지형 높이차에 속도가 흔들리지 않게)
			const float dist = to.Length();
			const float step = u.moveSpeed * dt;

			// 한 틱 전보다 실제로 가까워졌는지 본다. '걸은 양'이 아니라 '줄어든 거리'를 보는
			// 이유: 밀어내기가 명령 수행 뒤에 돌기 때문에, 한 걸음 걷고 그만큼 밀려나면
			// 걸음 자체는 정상이어도 제자리다. 몸이 생기면서 처음 생긴 상황이다.
			if (u.moveProgDist >= 0.f && dist > u.moveProgDist - step * STUCK_PROGRESS)
				u.moveStuckTimer += dt;
			else
				u.moveStuckTimer = 0.f;
			u.moveProgDist = dist;

			if (u.moveStuckTimer >= STUCK_GIVEUP)
			{
				// 더는 못 간다 — 그 자리를 남이 차지했거나, 벽 반대편이 목표다.
				// 명령을 붙들고 영원히 비비는 쪽이 더 나쁘다.
				// 도착 방향은 주지 않는다 — 도착한 게 아니다.
				done = true;
				break;
			}

			// 마지막 지점만 몸 반경만큼 느슨하게 본다. 한 좌표에 설 수 있는 건 한 명이라,
			// 정확한 점을 요구하면 둘째부터는 영원히 도착하지 못한다.
			const bool  bFinal = (u.curWaypoint + 1 >= (int)u.current.waypoints.size());
			const float reach  = bFinal ? std::max(step, u.bodyRadius) : step;

			if (dist <= reach)                       // 웨이포인트 도달
			{
				// 느슨한 판정으로 끊었으면 좌표를 당기지 않는다 — 몸 반경만큼 순간이동해 버린다
				if (dist <= step) u.pos = ResolveStep(u, target);

				u.curWaypoint++;
				u.moveProgDist = -1.f;               // 목표가 바뀌었다 — 지난 거리와 비교 금지
				if (u.curWaypoint >= (int)u.current.waypoints.size())
				{
					done = true;                     // 경로 끝
					if (u.current.hasArriveFacing)   // 도착 방향 지정됐으면 그쪽 봄
						u.facing = u.current.arriveFacing;
				}
			}
			else
			{
				const Vec3 dir = to.Normalized();
				u.pos    = ResolveStep(u, u.pos + dir * step);
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

			// 벽에 가리면 사거리 안이어도 못 때린다 — 엄폐가 작동하는 지점이다.
			// 근접도 같이 막는다: 얇은 벽을 사이에 두고 사거리가 겹치면 벽을 때려 관통하게 된다.
			const bool bBlocked = u.useNavGrid && !HasLineOfSight(u.pos, target->pos);
			if (!bBlocked) u.hasVantage = false;     // 트였다 — 들고 있던 사격 자리는 버린다

			if (dist > u.attackRange || bBlocked)    // 사거리 밖이거나 가려짐 → 접근(필요하면 우회)
			{
				Vec3 goal = target->pos;

				// 사거리는 닿는데 사선만 막힌 경우, 대상에게 걸어가는 건 사거리를 버리는 짓이다.
				// 궁수/마법사는 '쏠 수 있는 가장 가까운 자리'로 비켜선다. 근접은 어차피 붙어야 하므로 제외.
				if (bBlocked && u.attackRange >= VANTAGE_MIN_RANGE)
				{
					u.vantageTimer -= dt;
					if (!u.hasVantage || u.vantageTimer <= 0.f)
					{
						u.vantageTimer = VANTAGE_INTERVAL;   // 못 찾아도 간격은 둔다 — 매 틱 격자를 훑으면 안 된다
						u.hasVantage   = FindVantagePoint(u, *target, u.vantagePos);
					}
					if (u.hasVantage) goal = u.vantagePos;   // 못 찾았으면 예전처럼 대상 쪽으로
				}

				ChaseStep(u, goal, dt);
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
				else
					ResolveHit(*target, u.pos, u.attackDamage);   // 근접: 즉시 판정 (연속 공격; 대상 사망은 위 대상소멸 체크에서 done)
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

			// 바닥을 지정하는 스킬은 시전하는 내내 그 지점을 본다
			if (u.current.hasTargetPos)
			{
				Vec3 toPoint = u.current.targetPos - u.pos;
				toPoint.z = 0.f;
				if (toPoint.Length() > 0.f) u.facing = toPoint.Normalized();
			}

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
							Vec3 land = u.pos + dir * (d - u.attackRange);  // 근접 사거리 앞까지

							// 벽을 뚫고 돌진하지 않는다 — 막히면 갈 수 있는 데까지만 밀고 들어간다
							if (_nav && _nav->Valid() && u.useNavGrid && !_nav->CanWalkStraight(u.pos, land))
								land = LastWalkableAlong(u.pos, land);

							land.z = u.pos.z;
							u.pos = land;
							u.facing = dir;
						}

						// 돌진이 벽에 끊겼으면 충돌이 없었던 것 — 스턴도 없다
						Vec3 gap = target->pos - u.pos;
						gap.z = 0.f;
						if (gap.Length() <= u.attackRange * 1.25f)
							target->stunRemaining = CHARGE_STUN;   // ★ 돌진 충돌 → 짧은 스턴(즉시 도망 방지)
					}
				}
				else if (sk->type == SkillType::MagicBolt)
				{
					if (Unit* target = GetUnit(u.current.targetId))
						SpawnProjectile(u, *target, sk->damage, false);   // 마법탄(직선), 데미지=스킬값
				}
				else if (sk->type == SkillType::Meteor)
				{
					// 지정한 바닥을 중심으로 터진다. 지정이 없으면(대상 없이 들어온 명령) 아무 일도 안 일어난다
					if (u.current.hasTargetPos)
						ApplyAreaDamage(u, u.current.targetPos, sk->radius, sk->damage);
				}
				else if (sk->type == SkillType::Defense)
				{
					u.defendStance = !u.defendStance;   // 방어 태세 토글(플레이어 시전 경로)
				}
				else if (sk->type == SkillType::Heal)
				{
					Unit* ally = u.current.targetId ? GetUnit(u.current.targetId) : &u;
					if (ally && ally->alive)
						ally->hp = std::min(ally->maxHp, ally->hp + sk->damage);
				}
			}

			// 후딜까지 끝 → 완료
			if (u.execGauge >= sk->preDelay + sk->postDelay)
				done = true;
			break;
		}
		case CommandType::Item:
		{
			const ItemType   it  = (ItemType)u.current.itemId;
			const ItemDef    def = GetItemDef(u.current.itemId);
			if (it == ItemType::None) { done = true; break; }   // 모르는 아이템

			// 선딜 완료 순간 1회 발동 (스킬과 동일한 사이클)
			if (!u.attackFired && u.execGauge >= def.preDelay)
			{
				u.attackFired = true;

				if (it == ItemType::HealPotion)
				{
					// targetId 0 = 자신에게 사용
					Unit* t = u.current.targetId ? GetUnit(u.current.targetId) : &u;
					if (t && t->alive)
						t->hp = std::min(t->maxHp, t->hp + def.amount);
				}
			}

			// 후딜까지 끝 → 완료
			if (u.execGauge >= def.preDelay + def.postDelay)
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
				ResetNavState(u);
			}
			else
			{
				u.executing = false;                 // Idle
			}
		}
	}

	// 4) 몸 겹침 해소 — 수행이 끝난 뒤 한 번.
	// 수행 중간에 끼우면 명령을 먼저 처리한 유닛이 자리를 먼저 차지해 순회 순서가 결과를 바꾼다.
	SeparateUnits(dt);

	// 5) 발사체 — 밀어내기가 끝난 좌표로 충돌을 본다(한 프레임 전 위치에 맞는 일이 없게)
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
	// 후면은 못 막음(방어 무시). 정면일 때만 방어 롤.
	const DefenseKind defended = fromBehind ? DefenseKind::None : TryDefend(target);
	if (defended != DefenseKind::None)
	{
		if (OnDefended) OnDefended(target.id, defended);
		return false;
	}

	float dmg = damage * (fromBehind ? CRIT_MULT : 1.f);   // 후면=크리 배율
	target.hp -= dmg;
	if (OnDamaged) OnDamaged(target.id, dmg, fromBehind, fromBehind);

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

void CombatSim::ApplyAreaDamage(const Unit& caster, const Vec3& center, float radius, float damage)
{
	if (radius <= 0.f) return;

	const float r2 = radius * radius;
	for (auto& [id, t] : _units)
	{
		if (!t.alive)                    continue;
		if (t.faction == caster.faction) continue;   // 아군 오사는 없다

		// 높이는 보지 않는다 — Sim은 평면이고 z는 포물선 연출용이다
		const float dx = t.pos.x - center.x;
		const float dy = t.pos.y - center.y;
		if (dx * dx + dy * dy > r2)      continue;

		// 떨어진 자리에서 맞은 것으로 처리 — 방어/후면 판정은 일반 피격과 같은 길을 탄다
		ResolveHit(t, center, damage);
	}
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
	for (auto& p : _projectiles)
	{
		if (!p.alive) continue;

		// 경로상 '소유자와 다른 진영' 유닛 중 가장 가까운 것에 충돌 (아군은 관통 → 몸빵 가능).
		// 판정 거리는 유닛마다 다르다 — 덩치 큰 쪽이 더 잘 맞는다. 몸이 얇아도 PROJ_HIT_MIN은
		// 보장해, 반경을 줄인 유닛이 화살을 공짜로 피하지 않게 한다.
		Unit* hit  = nullptr;
		float best = 0.f;                                // '얼마나 깊이 들어왔나'로 비교한다
		for (auto& [id, u] : _units)
		{
			if (!u.alive) continue;
			if (u.faction == p.ownerFaction) continue;   // 같은 편 관통

			const float reach = std::max(PROJ_HIT_MIN, u.bodyRadius);
			Vec3 d = u.pos - p.pos; d.z = 0.f;
			const float dd = d.Length();
			if (dd > reach) continue;

			// 반경이 섞여 있으니 '남은 거리'로 비교하면 큰 몸이 항상 이긴다.
			// 파고든 깊이로 비교해, 작은 유닛을 스치고 큰 유닛을 맞추는 일이 없게 한다.
			// 동점은 id가 낮은 쪽. _units 순회 순서(해시)에 결과를 맡기면 서버와 갈린다
			const float depth = reach - dd;
			if (!hit || depth > best || (depth == best && u.id < hit->id))
			{
				best = depth;
				hit  = &u;
			}
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

		const Vec3 prev = p.pos;
		p.pos = p.pos + to.Normalized() * (p.speed * dt);

		// 벽에 박히면 소멸 — 엄폐물 뒤로 숨으면 화살이 안 들어온다.
		// 격자에 높이 개념이 없으니 포물선 화살도 똑같이 막힌다(낮은 엄폐물을 넘기는 건 아직 못 한다).
		if (_nav && _nav->Valid() && !_nav->LineOfSight(prev, p.pos))
		{
			p.alive = false;
			continue;
		}

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
