#include "AIBrain.h"
#include "CombatSim.h"

void ChaseAttackBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	Unit* target = sim.FindNearestHostile(self);
	if (!target) return;

	float dist = (target->pos - self.pos).Length();

	if (dist <= self.attackRange)
	{
		// 사거리 안: 그 대상을 공격 중이 아니면 공격 발행
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
		// 사거리 밖: 이동 중이 아니면 대상 쪽으로 이동 발행
		if (!(self.executing && self.current.type == CommandType::Move))
		{
			Command mv; mv.type = CommandType::Move; mv.waypoints.push_back(target->pos);
			sim.IssueCommand(self.id, mv, false);
		}
	}
}

void KiteBrain::Decide(CombatSim& sim, Unit& self, float dt)
{
	Unit* target = sim.FindNearestHostile(self);
	if (!target) return;

	float dist = (target->pos - self.pos).Length();
	const float kiteMin = 120.f;   // 이보다 가까우면 도망(밴드 하한)

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