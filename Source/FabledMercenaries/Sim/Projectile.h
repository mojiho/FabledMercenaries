#pragma once
#include <cstdint>
#include "Vec3.h"

enum class Faction : uint8_t;   // Unit.h에 정의 (같은 편 관통 판정용)

struct Projectile
{
	uint32_t id = 0;
	uint64_t ownerId = 0;     // 쏜 유닛
	uint64_t targetId = 0;     // 목표(호밍)
	float    damage = 0.f;
	float    speed = 600.f; // units/sec
	bool     arc = false; // 포물선(화살) 여부
	float    arcHeight = 0.f;   // 포물선 최고 높이

	Vec3     pos;               // 현재 위치
	Vec3     start;             // 발사 위치
	float    startDist = 1.f;   // 발사~목표 직선거리(진행도 계산용)
	bool     alive = true;
	Faction ownerFaction;
};
