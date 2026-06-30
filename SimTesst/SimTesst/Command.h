#pragma once

#include <vector>
#include <cstdint>
#include "Vec3.h"

//<summary>
// 명령 종류. enum class = 타입 안전 + 이름 충돌 방지.
// : uint16_t = 바이트 크기 고정(나중에 패킷으로 보낼 때 중요).
//</summary>
enum class CommandType : uint16_t
{
    None,
    Move,
    Attack,
    Skill,
    Defend,
    Focus,
    Stop
};

struct Command
{
    uint32_t slotId = 0;    // 취소/완료 식별 키 (Sim이 발급)
    CommandType type = CommandType::None;
    std::vector<Vec3> waypoints;    // Move: 이동 경로(여러 지점)
    uint64_t targetId = 0; // 공격/스킬 대상의 ID (Unit.id와 동일 타입)
    uint32_t skillId = 0;  // 스킬 ID
    uint32_t issueSeq = 0; // 발급 순서 번호 (순서 구분용)
};