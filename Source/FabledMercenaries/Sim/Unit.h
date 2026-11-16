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
enum class ActionState : uint8_t { Idle, Moving, AttackWindup, AttackRecover, Casting, Defending, Focusing, Stunned, Dead };

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
	float  maxHp = 100.f;                 // 최대 체력 (클래스 주입, 힐 회복 상한)
	bool   alive = true;
	float  moveSpeed = 300.f;             // units/sec
	float  attackRange = 100.f;           // 공격 사거리
	bool   ranged = false;                // 원거리(조준 필요) 여부

	/**
	 * 몸 반경(평면). 클래스에서 주입된다.
	 *
	 * 이 값이 있으면 유닛이 '자리를 차지한다' — 서로 겹치면 밀려나고, 발사체는 중심이 아니라
	 * 이 반경에 맞는다. 0이면 밀지도 밀리지도 않는다(연출용 더미 유닛 등).
	 */
	float  bodyRadius = 32.f;

	float  mp = 0.f;
	float  mpMax = 100.f;                 // 최대 MP (정신집중 상한, 스킬 시전 가능 여부)
	float cost = 10.f;

	// --- 공격 타이밍 (선/후딜) ---
	float  attackPreDelay = 0.4f;         // 선딜(윈드업/조준)
	float  attackPostDelay = 0.2f;        // 후딜(회복)
	float  attackDamage = 25.f;           // 기본 공격 데미지 (클래스 주입)
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

	/**
	 * 이 유닛이 장애물 격자를 따르는지.
	 *
	 * 탐험 중 지휘관 아바타는 false다 — 상위 레이어에 지형 높이를 직접 재는 턱 판정이
	 * 따로 있고(점프로 턱을 넘는 것까지 다룬다), 격자까지 끼면 뛰어오를 수 있는 턱을
	 * 격자가 먼저 막아 버린다. 전투에 들어가면(점프 불가) true로 돌린다.
	 */
	bool useNavGrid = true;

	// --- 추격 우회 경로 ---
	// Move 명령의 waypoints는 발급 시점에 한 번 계산해 두면 끝이지만,
	// 추격(Attack)은 대상이 계속 움직이므로 경로를 주기적으로 다시 깐다.
	// 비어 있으면 '직선으로 보이니 그냥 직진' 상태다.
	std::vector<Vec3> navPath;
	int               navIndex = 0;
	float             repathTimer = 0.f;  // 0 이하가 되면 다시 탐색

	// --- 사격 자리 ---
	// 사선이 막혔을 때 '거기 서면 쏠 수 있는' 가장 가까운 칸. 사거리가 긴 유닛만 쓴다.
	// 매 틱 찾으면 격자를 수백 칸 훑게 되므로 간격을 두고 갱신한다.
	// hasVantage가 false면 '못 찾았다' — 호출부는 예전처럼 대상 쪽으로 접근한다.
	Vec3              vantagePos;
	bool              hasVantage   = false;
	float             vantageTimer = 0.f;

	// --- 이동 진척 감시 ---
	// 몸이 생기면서 '목표 지점에 도달할 수 없는' 경우가 처음 생긴다 — 다른 유닛이 그 자리를
	// 차지하고 있으면 서로 밀며 영원히 비빈다. 한 틱 전의 남은 거리를 들고 있다가
	// 실제로 가까워지지 않으면 시간을 재고, 일정 시간 넘으면 이동을 포기한다.
	// (밀어내기는 수행 '뒤'에 일어나므로, 한 걸음 걷고 한 걸음 밀려나는 것을 이 비교가 잡아낸다)
	float             moveStuckTimer = 0.f;
	float             moveProgDist   = -1.f;  // -1 = 아직 안 재봄

	// --- 턴테이블 (명령 수령 게이지) ---
	// 명령을 '수행'하는 것과는 별개로, 이 유닛이 '다음 명령을 받을 수 있는가'만 센다.
	// 수행 중에도 계속 돈다 — 이동/공격 중에 새 명령으로 인터럽트하는 게 이 전투의 핵심이라
	// 게이지를 수행 상태에 묶으면 안 된다. (execGauge = 수행 진행도, 별개의 축)
	// turnMax == 0 이면 턴 제한 없음(탐험 중 아바타처럼 상시 명령 가능한 유닛).
	float  turnGauge = 0.f;
	float  turnMax   = 2.0f;              // 가득 차면 명령 1회분
	float  turnRate  = 1.0f;              // 초당 충전

	bool TurnReady() const { return turnGauge >= turnMax; }

	/** 0~1. 화면 게이지바용. turnMax가 0이면 항상 가득 찬 것으로 본다 */
	float TurnRatio() const { return (turnMax > 0.f) ? (turnGauge / turnMax) : 1.f; }
	bool defendStance = false;                 // 방어 태세 중(방어 스킬 시전 중)

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
		case CommandType::Item:   return ActionState::Casting;   // 아이템 사용도 시전 모션 재사용
		case CommandType::Defend: return ActionState::Defending;
		case CommandType::Focus:  return ActionState::Focusing;
		default:                  return ActionState::Idle;
		}
	}
};
