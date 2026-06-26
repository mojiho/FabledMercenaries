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


class Unit
{
public:
	uint64_t id = 0;
	uint64_t ownerId = 0;          // 지휘하는 플레이어 id (cmdGauge 연결)
	Faction  faction = Faction::Neutral;
	Vec3     pos;
	float    hp = 100.f;
	bool    alive = true;
	float    moveSpeed = 300.f;      // units/sec
	std::unique_ptr<Brain> brain;    // null=외부(사람)가 명령 / 있으면 그 두뇌가 IssueCommand
	std::vector<Skill> skills;        // 패시브/액티브 스킬. 클래스/전직에서 주입
	Class  unitClass = Class::None;
	int    mp = 0;
	int    mpMax = 100;
	float  defenseCdRemaining = 0.f;             // 방어기 재사용 대기(0이면 사용 가능)
	Vec3   facing = { 1.f, 0.f, 0.f };  // 바라보는 방향(백어택 판정용)
	float  attackRange = 100.f;		// 공격 사거리 (클래스에서 주입)
	float attackPreDelay = 0.4f;
	float attackPostDelay = 0.2f;
	bool  attackFired = false;   // 이번 사이클에 발사했는지(선딜 후 1회)

	// --- 수행 상태 ---
	bool ranged = false;   // 원거리(조준 필요) 여부
	float execGauge = 0.f;
	Command current;
	bool executing = false;
	std::deque<Command> reserveQueue;
	int curWaypoint = 0;
};