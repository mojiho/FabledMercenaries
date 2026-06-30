#include "AIBrain.h"
#include "CombatSim.h"

void ChaseAttackBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	Unit* target = sim.FindNearestHostile(self);
	if (!target) return;

	float dist = (target->pos - self.pos).Length();

	// 사거리 안 → 공격
	if (dist <= self.attackRange)
	{
		bool attackingTarget = self.executing
			&& self.current.type == CommandType::Attack
			&& self.current.targetId == target->id;
		if (!attackingTarget)
		{
			Command atk; atk.type = CommandType::Attack; atk.targetId = target->id;
			sim.IssueCommand(self.id, atk, false);
		}
		return;
	}

	// 사거리 밖 — 돌진 시전 중이면 그대로 둠
	if (self.executing && self.current.type == CommandType::Skill) return;

	// 준비된 돌진 스킬(Active/Charge, 쿨0, MP충분) 있나?
	bool hasCharge = false;
	for (const auto& sk : self.skills)
		if (sk.category == SkillCategory::Active && sk.type == SkillType::Charge
			&& sk.cdRemaining <= 0.f && self.mp >= sk.mpCost)
		{
			hasCharge = true;
			break;
		}

	if (hasCharge)
	{
		// 돌진으로 거리 좁히기
		Command ch; ch.type = CommandType::Skill;
		ch.skillId  = (uint32_t)SkillType::Charge;
		ch.targetId = target->id;
		sim.IssueCommand(self.id, ch, false);
	}
	else if (!(self.executing && self.current.type == CommandType::Move))
	{
		// 돌진 못 쓰면 평범하게 접근
		Command mv; mv.type = CommandType::Move;
		mv.waypoints.push_back(target->pos);
		sim.IssueCommand(self.id, mv, false);
	}
}

void KiteBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	Unit* target = sim.FindNearestHostile(self);
	if (!target) return;

	float dist = (target->pos - self.pos).Length();
	const float kiteMin = 150.f;   // 이보다 가까우면 도망(밴드 하한)

	if (dist < kiteMin)
	{
		// 너무 가까움 → 적 반대 방향으로 도망 (이동 중이 아닐 때만 새로 발행)
		if (!(self.executing && self.current.type == CommandType::Move))
		{
			Vec3 away = (self.pos - target->pos).Normalized();
			Vec3 fleeTo = self.pos + away * 400.f;
			Command mv; mv.type = CommandType::Move; mv.waypoints.push_back(fleeTo);
			sim.IssueCommand(self.id, mv, false);
		}
	}
	else if (dist <= self.attackRange)
	{
		// 적정 거리(밴드 안) → 멈춰서 공격
		bool attackingTarget = self.executing
			&& self.current.type == CommandType::Attack
			&& self.current.targetId == target->id;
		if (!attackingTarget)
		{
			Command atk; atk.type = CommandType::Attack; atk.targetId = target->id;
			sim.IssueCommand(self.id, atk, false);
		}
	}
	else
	{
		// 너무 멀음 → 접근
		if (!(self.executing && self.current.type == CommandType::Move))
		{
			Command mv; mv.type = CommandType::Move; mv.waypoints.push_back(target->pos);
			sim.IssueCommand(self.id, mv, false);
		}
	}
}

void MageBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	Unit* target = sim.FindNearestHostile(self);
	if (!target) return;

	// 내 MagicBolt 스킬 찾기
	const Skill* bolt = nullptr;
	for (const auto& sk : self.skills)
		if (sk.category == SkillCategory::Active && sk.type == SkillType::MagicBolt) { bolt = &sk; break; }
	if (!bolt) return;

	// 시전/충전 중이면 그대로 유지
	if (self.executing && (self.current.type == CommandType::Skill || self.current.type == CommandType::Focus))
		return;

	if (bolt->cdRemaining <= 0.f && self.mp >= bolt->mpCost)
	{
		// 쿨 차고 MP 충분 → 마법탄
		Command c; c.type = CommandType::Skill;
		c.skillId = (uint32_t)SkillType::MagicBolt;
		c.targetId = target->id;
		sim.IssueCommand(self.id, c, false);
	}
	else if (self.mp < bolt->mpCost)
	{
		// MP 부족 → 정신집중(충전)
		Command f; f.type = CommandType::Focus;
		sim.IssueCommand(self.id, f, false);
	}
	// 쿨 도는 중(MP는 충분) → 대기
}


void HealerBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	// 내 Heal 스킬 찾기
	const Skill* heal = nullptr;
	for (const auto& sk : self.skills)
		if (sk.category == SkillCategory::Active && sk.type == SkillType::Heal) { heal = &sk; break; }
	if (!heal) return;

	// 시전/충전 중이면 유지
	if (self.executing && (self.current.type == CommandType::Skill || self.current.type == CommandType::Focus))
		return;

	// 가장 다친 아군(같은 진영, 살아있음, hp<maxHp) — 자신 포함, HP 비율 최저
	const Unit* patient = nullptr;
	float worst = 1.f;
	for (const auto& [id, u] : sim.Units())
	{
		if (!u.alive) continue;
		if (u.faction != self.faction) continue;
		if (u.hp >= u.maxHp) continue;
		float ratio = u.hp / u.maxHp;
		if (ratio < worst) { worst = ratio; patient = &u; }
	}

	if (patient && heal->cdRemaining <= 0.f && self.mp >= heal->mpCost)
	{
		Command c; c.type = CommandType::Skill;
		c.skillId = (uint32_t)SkillType::Heal;
		c.targetId = patient->id;
		sim.IssueCommand(self.id, c, false);     // 회복 시전
	}
	else if (self.mp < heal->mpCost)
	{
		Command f; f.type = CommandType::Focus;
		sim.IssueCommand(self.id, f, false);     // MP 부족 → 정신집중
	}
	// 다친 아군 없음 / 쿨 중 → 대기(후방 위치 유지)
}


void TankBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	Unit* enemy = sim.FindNearestHostile(self);
	if (!enemy) return;

	// 보호 대상: 같은 편 후방 직업(마법사/궁수/힐러) 중 적에게 가장 가까운(가장 위험한) 아군
	const Unit* protectee = nullptr;
	float nearestToEnemy = 0.f;
	for (const auto& [id, u] : sim.Units())
	{
		if (u.id == self.id)            continue;
		if (!u.alive)                   continue;
		if (u.faction != self.faction)  continue;
		if (u.unitClass != Class::Mage && u.unitClass != Class::Archer && u.unitClass != Class::Healer)
			continue;
		float d = (u.pos - enemy->pos).Length();
		if (!protectee || d < nearestToEnemy) { protectee = &u; nearestToEnemy = d; }
	}

	float distToEnemy = (enemy->pos - self.pos).Length();

	// (A) 후방 아군 없음 → 평범하게 돌격 공격
	if (!protectee)
	{
		if (distToEnemy <= self.attackRange)
		{
			bool atking = self.executing && self.current.type == CommandType::Attack
				&& self.current.targetId == enemy->id;
			if (!atking) { Command a; a.type = CommandType::Attack; a.targetId = enemy->id; sim.IssueCommand(self.id, a, false); }
		}
		else if (!(self.executing && self.current.type == CommandType::Move))
		{
			Command m; m.type = CommandType::Move; m.waypoints.push_back(enemy->pos);
			sim.IssueCommand(self.id, m, false);
		}
		return;
	}

	// (B) 후방 아군 있음 → 그 앞(적 방향)을 막아선다
	const float GUARD_DIST = 150.f;   // 보호 대상에서 적 쪽으로 이만큼 앞에 선다
	Vec3 toEnemy = (enemy->pos - protectee->pos).Normalized();
	Vec3 guardPos = protectee->pos + toEnemy * GUARD_DIST;
	float distToGuard = (guardPos - self.pos).Length();

	if (distToEnemy <= self.attackRange)
	{
		// 적이 코앞 → 때려서 피킹(다이브 차단)
		bool atking = self.executing && self.current.type == CommandType::Attack
			&& self.current.targetId == enemy->id;
		if (!atking) { Command a; a.type = CommandType::Attack; a.targetId = enemy->id; sim.IssueCommand(self.id, a, false); }
	}
	else if (distToGuard > 40.f)
	{
		// 자리 이탈 → 보호 위치로 이동(투사체 몸빵 라인 형성)
		if (!(self.executing && self.current.type == CommandType::Move))
		{
			Command m; m.type = CommandType::Move; m.waypoints.push_back(guardPos);
			sim.IssueCommand(self.id, m, false);
		}
	}
	else
	{
		// 자리 잡음 → 방어 태세 유지(투사체/돌진/평타 흡수)
		if (!(self.executing && self.current.type == CommandType::Defend))
		{
			Command d; d.type = CommandType::Defend;
			sim.IssueCommand(self.id, d, false);
		}
	}
}