#pragma once
#include <cstdint>

enum class SkillCategory : uint8_t { Passive, Active };
enum class SkillType : uint8_t { None, Defense, Charge, MagicBolt, Heal /*, Attack, Dash, Buff, Escape … 확장 */ };
enum class DefenseKind : uint8_t { None, Parry, Block, Dodge };

// 스킬 1개 (패시브/액티브 공통). 평면 구조 = 서버 직렬화 용이.
struct Skill
{
	SkillCategory category = SkillCategory::Passive;
	SkillType     type = SkillType::None;

	float damage = 0.f;		// 액티브 공격 스킬 데미지
	int mpCost = 0;			// 액티브 스킬 MP 소모
	// 공통: 쿨다운
	float         cooldown = 0.f;
	float         cdRemaining = 0.f;   // 런타임
	float         preDelay = 0.f;   // 선딜(시전/윈드업) — 스킬마다 다름
	float         postDelay = 0.f;   // 후딜(회복) — 스킬마다 다름

	// [type==Defense] 방어 파라미터
	float         attackSlow = 1.f;   // [Active Defense] 방어 태세 중 공격 선/후딜 배율(>1=느림)
	DefenseKind   defenseKind = DefenseKind::None;
	float         chance = 0.f;   // 방어 성공 확률 0~1

	// [category==Active] 나중에: float mpCost; float range; ... (현재 미사용)
};

// 방어 패시브 스킬 만들기 (헬퍼)
inline Skill MakeDefense(DefenseKind kind, float chance, float cooldown)
{
	Skill s;
	s.category = SkillCategory::Passive;
	s.type = SkillType::Defense;
	s.defenseKind = kind;
	s.chance = chance;
	s.cooldown = cooldown;
	return s;
}

inline Skill MakeCharge(float preDelay, float postDelay, float cooldown, int mpCost = 0)
{
	Skill s;
	s.category = SkillCategory::Active;
	s.type = SkillType::Charge;
	s.preDelay = preDelay;
	s.postDelay = postDelay;
	s.cooldown = cooldown;
	s.mpCost = mpCost;
	return s;
}

inline Skill MakeMagicBolt(float damage, float preDelay, float postDelay, float cooldown, int mpCost)
{
	Skill s;
	s.category = SkillCategory::Active;
	s.type = SkillType::MagicBolt;
	s.damage = damage;
	s.preDelay = preDelay;
	s.postDelay = postDelay;
	s.cooldown = cooldown;
	s.mpCost = mpCost;
	return s;
}

inline Skill MakeHeal(float healAmount, float preDelay, float postDelay, float cooldown, int mpCost)
{
	Skill s;
	s.category = SkillCategory::Active;
	s.type = SkillType::Heal;
	s.damage = healAmount;   // Heal은 damage 필드를 '회복량'으로 재사용
	s.preDelay = preDelay;
	s.postDelay = postDelay;
	s.cooldown = cooldown;
	s.mpCost = mpCost;
	return s;
}

inline Skill MakeDefenseStance(DefenseKind kind, float blockChance, float attackSlow)
{
	Skill s;
	s.category = SkillCategory::Active;   // 액티브 = 토글 방어 태세
	s.type = SkillType::Defense;
	s.defenseKind = kind;
	s.chance = blockChance;
	s.attackSlow = attackSlow;
	return s;
}