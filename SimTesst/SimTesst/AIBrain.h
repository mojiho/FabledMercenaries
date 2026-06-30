#pragma once
#include "Brain.h"

class ChaseAttackBrain : public Brain	//	근접 유닛용: 적에게 접근하여 공격
{
public:
	void Decide(CombatSim& sim, Unit& self, float dt) override;
};

class KiteBrain : public Brain		//	원거리 유닛용: 적에게 접근하지 않고 사거리 밖에서 공격
{
public:
	void Decide(CombatSim& sim, Unit& self, float dt) override;
};

class MageBrain : public Brain   // 마법사: MagicBolt 시전, MP 부족하면 정신집중
{
public:
	void Decide(CombatSim& sim, Unit& self, float dt) override;
};

class HealerBrain : public Brain   // 힐러: 가장 다친 아군 회복, MP 부족하면 정신집중
{
public:
	void Decide(CombatSim& sim, Unit& self, float dt) override;
};

class TankBrain : public Brain   // 탱커: 후방 아군 있으면 그 앞을 지킴(몸빵/방어), 없으면 공격
{
public:
	void Decide(CombatSim& sim, Unit& self, float dt) override;
};