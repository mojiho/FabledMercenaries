#pragma once
#include <cstdint>

enum class SkillCategory : uint8_t { Passive, Active };
// 값이 그대로 Command::skillId이자 UI/패킷 상수다 — 뒤에만 덧붙이고 중간에 끼워 넣지 말 것.
enum class SkillType : uint8_t { None, Defense, Charge, MagicBolt, Heal, Meteor /*, Dash, Buff, Escape … 확장 */ };
enum class DefenseKind : uint8_t { None, Parry, Block, Dodge };
enum class TargetMode : uint8_t {Instant, Unit, Point};  // 즉시, 유닛, 지점
enum class TargetFilter : uint8_t {Any, Ally, Enemy};

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

	// [targetMode==Point] 지정한 바닥을 중심으로 한 피해 반경. 0이면 범위 피해 없음.
	// 화면 쪽(FMSimManager)도 이 값으로 마법진 크기를 그린다.
	float         radius = 0.f;

	// [type==Defense] 방어 파라미터
	float         attackSlow = 1.f;   // [Active Defense] 방어 태세 중 공격 선/후딜 배율(>1=느림)
	DefenseKind   defenseKind = DefenseKind::None;
	float         chance = 0.f;   // 방어 성공 확률 0~1

	// [category==Active] 나중에: float mpCost; float range; ... (현재 미사용)
	
	TargetMode targetMode = TargetMode::Unit;
	TargetFilter targetFilter = TargetFilter::Any;
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
	s.targetMode = TargetMode::Unit;        // 적 유닛 지정
	s.targetFilter = TargetFilter::Enemy;
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
	s.targetMode = TargetMode::Unit;        // 적 유닛 지정
	s.targetFilter = TargetFilter::Enemy;
	return s;
}

/**
 * 범위 마법 — 유닛이 아니라 바닥 한 지점을 지정한다.
 * 명령을 내리면 그 자리에 마법진이 뜨고, castTime(선딜)이 지나면 radius 안의 적이 한꺼번에 맞는다.
 * 시전이 끝나기 전에 적이 걸어 나가면 안 맞는다 — 판정은 터지는 순간의 위치로만 한다.
 */
inline Skill MakeMeteor(float damage, float radius, float castTime, float postDelay, float cooldown, int mpCost)
{
	Skill s;
	s.category = SkillCategory::Active;
	s.type = SkillType::Meteor;
	s.damage = damage;
	s.radius = radius;
	s.preDelay = castTime;
	s.postDelay = postDelay;
	s.cooldown = cooldown;
	s.mpCost = mpCost;
	s.targetMode = TargetMode::Point;       // 바닥 지정
	s.targetFilter = TargetFilter::Enemy;
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
	s.targetMode = TargetMode::Unit;        // 아군 유닛 지정
	s.targetFilter = TargetFilter::Ally;
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
	s.targetMode = TargetMode::Instant;     // 조준 없이 즉시 토글
	return s;
}