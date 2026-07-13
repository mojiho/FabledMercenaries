#pragma once
#include <cstdint>


// 지휘관 명령 게이지 — 플레이어 단위 '전체 명령 발행 속도' 자
// 유닛이 아니라 per-player 전투 상태.
enum class CommanderType : uint8_t { Combat, Command };		// 전투형, 지휘형 지휘관 타입 분류


struct Commander
{
	uint64_t id = 0;     // playerId (메타 Player와 동일 키)
	CommanderType type = CommanderType::Command;
	float    cmdGauge = 0.f;
	float    cmdRate = 1.0f;  // 초당 충전
	float    cmdMax = 3.0f;  // 상한

	float costBudget = 100.f;  // 지휘 가능한 코스트 총량
	float usedCost = 0.f;      // 현재 사용중인 코스트
};

// 유형별 기본값 세팅 (free 함수)
inline Commander MakeCommander(uint64_t id, CommanderType type)
{
	Commander c;
	c.id = id;
	c.type = type;
	if (type == CommanderType::Command) { c.cmdMax = 5.0f; c.cmdRate = 1.5f; c.costBudget = 150.f; }  // 지휘형
	else                                { c.cmdMax = 3.0f; c.cmdRate = 1.0f; c.costBudget = 80.f;  }  // 전투형
	c.cmdGauge = c.cmdMax;   // ★ 게이지 가득 채워 시작 (입장 직후 첫 명령 바로 먹히게)
	return c;
}