#pragma once

#include <deque>
#include <vector>
#include <cstdint>
#include <memory>
#include "Vec3.h"
#include "Command.h"
#include "Brain.h"
#include "Skill.h"
#include "Class.h"

enum class Faction : uint8_t { Player, Hostile, Neutral };
enum class ActionState : uint8_t { Idle, Moving, AttackWindup, AttackRecover, Casting, Stunned, Dead };

class Unit
{
public:
	// --- 식별 / 소속 ---
	uint64_t id = 0;
	uint64_t ownerId = 0;                 // 지휘하는 플레이어 id (cmdGauge 연결)
	Faction  faction = Faction::Neutral;
	Class    unitClass = Class::None;

	// --- 기본 스탯 (대부분 클래스에서 주입) ---
	Vec3   pos;
	Vec3   facing = { 1.f, 0.f, 0.f };    // 바라보는 방향(백어택 판정용)
	float  hp = 100.f;
	bool   alive = true;
	float  moveSpeed = 300.f;             // units/sec
	float  attackRange = 100.f;           // 공격 사거리
	bool   ranged = false;                // 원거리(조준 필요) 여부
	int    mp = 0;
	int    mpMax = 100;

	// --- 공격 타이밍 (선/후딜) ---
	float  attackPreDelay = 0.4f;         // 선딜(윈드업/조준)
	float  attackPostDelay = 0.2f;        // 후딜(회복)
	bool   attackFired = false;           // 이번 사이클에 발사했는지(선딜 후 1회)

	// --- 구성 ---
	std::unique_ptr<Brain> brain;         // null=외부(사람)가 명령 / 있으면 그 두뇌가 IssueCommand
	std::vector<Skill> skills;            // 패시브/액티브 스킬. 클래스/전직에서 주입

	// --- 수행 상태 (런타임) ---
	float  execGauge = 0.f;               // 현재 명령 사이클 경과(초) — 선/후딜 타이머
	Command current;
	bool   executing = false;
	std::deque<Command> reserveQueue;
	int    curWaypoint = 0;
	float  stunRemaining = 0.f;           // 스턴 남은 시간(초)

	// 표현(애니)용 파생 상태 — Sim은 애니를 모르고 상태만 노출
	ActionState GetActionState() const
	{
		if (!alive)              return ActionState::Dead;
		if (stunRemaining > 0.f) return ActionState::Stunned;
		if (!executing)          return ActionState::Idle;
		switch (current.type)
		{
		case CommandType::Move:   return ActionState::Moving;
		case CommandType::Attack: return attackFired ? ActionState::AttackRecover : ActionState::AttackWindup;
		case CommandType::Skill:  return ActionState::Casting;
		default:                  return ActionState::Idle;
		}
	}
};
