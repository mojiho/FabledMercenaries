#pragma once

#include <cmath>    // sqrtf

// <summary>
// 엔진 비의존 3D 벡터 구조체
// CoreMinimal.h / UE 타입 절대 금지 — 서버에서도 그대로 컴파일돼야 함.
// </summary>

struct Vec3
{
	float x = 0.f, y = 0.f, z = 0.f;

	Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
	Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
	Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }

	// 길이 (크기). sqrt(x*x + y*y + z*z)
	float Length() const { return sqrtf(x * x + y * y + z * z); }

	// 단위벡터(방향만, 길이 1). 길이 0이면 0벡터 반환(0 나누기 가드).
	Vec3 Normalized() const
	{
		float len = Length();
		if (len == 0.f) return { 0.f, 0.f, 0.f };
		float inv = 1.f / len;
		return { x * inv, y * inv, z * inv };
	}
};
