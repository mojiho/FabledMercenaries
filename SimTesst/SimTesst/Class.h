#pragma once
#include <cstdint>
#include <vector>
#include "Skill.h"

enum class Class : uint8_t
{
	None,
	Warrior,   // 전사
	Tanker,    // 탱커
	Mage,      // 마법사
	Archer,    // 궁수
	Assassin,  // 암살자
	Healer
};

struct ClassStats
{
	float moveSpeed;
	float attackRange;		// 공격 사거리
	bool ranged;			// 원거리 공격 가능 여부
	float attackPreDelay;    // 선딜(윈드업/조준) 초
	float attackPostDelay;   // 후딜(회복) 초
	float attackDamage;      // 기본 공격 데미지
	float maxHp;          // 최대 체력
};

inline ClassStats GetClassStats(Class c)
{
	switch (c)
	{                       //  이속   사거리 원거리 선딜  후딜  뎀
	case Class::Warrior:  return { 300.f, 120.f, false, 0.4f, 0.2f, 25.f, 130.f };
	case Class::Tanker:   return { 250.f, 120.f, false, 0.5f, 0.3f, 18.f, 250.f };  // 단단
	case Class::Mage:     return { 280.f, 600.f, false, 0.8f, 0.5f, 10.f,  70.f };  // 물몸
	case Class::Archer:   return { 340.f, 700.f, true,  1.3f, 0.4f, 18.f,  90.f };
	case Class::Assassin: return { 360.f, 120.f, false, 0.3f, 0.2f, 22.f,  90.f };
	case Class::Healer:   return { 300.f, 500.f, false, 0.6f, 0.4f,  8.f,  80.f };  // 지원·약한평타·물몸
	default:              return { 300.f, 120.f, false, 0.4f, 0.2f, 25.f, 100.f };
	}
}

inline std::vector<Skill> GetClassSkills(Class c)
{
	switch (c)
	{
	case Class::Warrior:  return { MakeDefense(DefenseKind::Parry, 0.50f, 0.5f), MakeCharge(0.4f, 0.2f, 1.0f, 5) };
	case Class::Tanker:   return { MakeDefense(DefenseKind::Block, 0.70f, 2.0f), MakeDefenseStance(DefenseKind::Block, 0.85f, 1.8f) };
	case Class::Mage:     return { MakeMagicBolt(60.f, 1.0f, 0.5f, 3.0f, 30) };  // 강한 원거리 마법: 캐스팅1s, 쿨3s, MP30
	case Class::Archer:   return { MakeDefense(DefenseKind::Dodge, 0.40f, 1.0f) };
	case Class::Assassin: return { MakeDefense(DefenseKind::Dodge, 0.50f, 1.0f) };
	case Class::Healer:   return { MakeHeal(40.f, 0.8f, 0.4f, 4.0f, 25) };  // 회복40, 캐스팅0.8s, 쿨4s, MP25
	default:              return {};
	}
}