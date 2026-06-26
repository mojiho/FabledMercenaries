#pragma once
#include <cstdint>

// 지휘관 명령 게이지 — 플레이어 단위 '전체 명령 발행 속도' 자
// 유닛이 아니라 per-player 전투 상태.
struct Commander
{
	uint64_t id = 0;     // playerId (메타 Player와 동일 키)
	float    cmdGauge = 0.f;
	float    cmdRate = 1.0f;  // 초당 충전
	float    cmdMax = 3.0f;  // 상한
};