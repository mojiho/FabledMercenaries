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