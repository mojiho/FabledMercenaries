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
	Assassin   // 암살자
};

struct ClassStats
{
	float moveSpeed;
	float attackRange;		// 공격 사거리
	bool ranged;			// 원거리 공격 가능 여부
	float attackPreDelay;    // 선딜(윈드업/조준) 초
	float attackPostDelay;   // 후딜(회복) 초
};

inline ClassStats GetClassStats(Class c)
{
	switch (c)
	{                       //  이속   사거리 원거리 선딜  후딜
	case Class::Warrior:  return { 300.f, 120.f, false, 0.4f, 0.2f };
	case Class::Tanker:   return { 250.f, 120.f, false, 0.5f, 0.3f };
	case Class::Mage:     return { 280.f, 600.f, false, 0.8f, 0.5f };  // 지팡이 기본=약한 근접, 화력은 스킬
	case Class::Archer:   return { 340.f, 700.f, true,  1.3f, 0.4f };  // 조준
	case Class::Assassin: return { 360.f, 120.f, false, 0.3f, 0.2f };  // 빠른 타격
	default:              return { 300.f, 120.f, false, 0.4f, 0.2f };
	}
}

inline std::vector<Skill> GetClassSkills(Class c)
{
	switch (c)
	{
	case Class::Warrior:  return { MakeDefense(DefenseKind::Parry, 0.50f, 0.5f), MakeCharge(0.4f, 0.2f, 1.0f, 5) };
	case Class::Tanker:   return { MakeDefense(DefenseKind::Block, 0.70f, 2.0f) };
	case Class::Mage:     return {};                                       // 근접 방어 없음
	case Class::Archer:   return { MakeDefense(DefenseKind::Dodge, 0.40f, 1.0f) };
	case Class::Assassin: return { MakeDefense(DefenseKind::Dodge, 0.50f, 1.0f) };
	default:              return {};
	}
}