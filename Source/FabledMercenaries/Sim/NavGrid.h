#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "Vec3.h"

// <summary>
// 엔진 비의존 내비게이션 격자 (장애물 데이터 + 경로탐색 + 사선 판정).
//
// CoreMinimal.h / UE 타입 절대 금지 — 서버에서도 그대로 컴파일돼야 한다.
// UE NavMesh를 쓰지 않는 이유: 길찾기를 클라가 하면 서버에서 같은 결과가 안 나온다.
// 격자와 A*를 Sim이 직접 들고 있어야 같은 입력 → 같은 경로가 보장된다.
//
// 좌표계: XY 평면만 본다. z는 무시한다(Sim은 평면이고 z는 연출용).
// 격자 밖은 '열린 지형'으로 취급한다 — 베이크 범위가 전장보다 작게 잡혀도
// 유닛이 얼어붙는 대신 장애물만 없는 상태로 부드럽게 망가진다.
// </summary>

enum class NavFlag : uint8_t
{
	None      = 0,
	Solid     = 1 << 0,   // 실제 지오메트리(벽/건물/절벽). 이동과 사선을 모두 막는다
	Clearance = 1 << 1,   // Solid를 에이전트 반경만큼 부풀린 여유칸. 이동만 막고 사선은 통과시킨다
};

class NavGrid
{
public:
	// ---------- 베이크 ----------

	/**
	 * 격자를 새로 깔고 전부 열린 칸으로 초기화한다.
	 * @param minCorner  칸 (0,0)의 '좌하단' 월드 좌표 (z는 보관만 하고 판정에 쓰지 않는다)
	 * @param cellSize   한 칸의 월드 크기. 작을수록 정밀하고 느리다
	 */
	void Reset(const Vec3& minCorner, float cellSize, int width, int height);

	bool  Valid()    const { return _w > 0 && _h > 0 && _cell > 0.f; }
	int   Width()    const { return _w; }
	int   Height()   const { return _h; }
	float CellSize() const { return _cell; }
	Vec3  MinCorner()const { return _origin; }

	void SetSolid(int cx, int cy, bool bSolid);
	void SetSolidAt(const Vec3& p, bool bSolid);

	/**
	 * Solid 칸을 에이전트 반경만큼 부풀려 Clearance를 깐다.
	 * 베이크의 '마지막'에 한 번만 호출할 것 — Clearance는 Solid를 보고 만들어지므로
	 * 두 번 부르면 여유가 두 배로 두꺼워진다.
	 *
	 * 부풀린 칸을 Solid로 칠하지 않고 따로 표시하는 이유: 화살이 벽 옆을 스쳐가는 걸
	 * 막으면 안 된다. 걸어 들어갈 수 없는 칸과 쏘고 지날 수 없는 칸은 다르다.
	 */
	void Inflate(float agentRadius);

	/** Solid/Clearance 전부 지운다 (격자 크기는 유지) */
	void ClearFlags();

	// ---------- 조회 ----------

	/** 격자 밖이면 false를 반환하고 cx/cy는 가장 가까운 칸으로 잘라 준다 */
	bool WorldToCell(const Vec3& p, int& cx, int& cy) const;

	/** 칸의 '중심' 월드 좌표. z는 격자 원점의 z를 그대로 쓴다 */
	Vec3 CellToWorld(int cx, int cy) const;

	/** 실제 지오메트리인가. 격자 밖은 false(열린 지형) */
	bool IsSolid(int cx, int cy) const;

	/** 서서 지나갈 수 있는가(Solid도 Clearance도 아님). 격자 밖은 true */
	bool IsWalkable(int cx, int cy) const;
	bool IsWalkableAt(const Vec3& p) const;

	/**
	 * a→b 직선이 벽을 지나지 않는가 (Clearance는 통과시킨다).
	 * 원거리 공격·투사체·범위마법 시야 판정에 쓴다. 격자가 없으면 항상 true.
	 */
	bool LineOfSight(const Vec3& a, const Vec3& b) const;

	/**
	 * a→b를 '걸어서' 똑바로 갈 수 있는가 (여유칸도 막힌 것으로 본다).
	 * 사선 판정(LineOfSight)과 기준이 달라서 따로 둔다 — 쏠 수는 있지만 걸을 수는 없는 틈이 있다.
	 */
	bool CanWalkStraight(const Vec3& a, const Vec3& b) const;

	/**
	 * p가 못 서는 칸이면 가장 가까운 설 수 있는 칸의 중심을 찾아 준다.
	 * 플레이어가 벽을 클릭했을 때 명령을 버리지 않고 벽 앞까지 보내기 위한 것.
	 * @return 찾았으면 true (p가 이미 설 수 있는 칸이면 p를 그대로 돌려준다)
	 */
	bool NearestWalkable(const Vec3& p, Vec3& out, int maxRing = 16) const;

	/**
	 * A* 경로탐색. 성공 시 outPath에 '시작점을 제외한' 월드 좌표 경로가 담긴다.
	 * 결과는 사선 판정으로 당겨서(string pulling) 계단 모양이 아닌 직선 구간으로 나온다.
	 *
	 * 격자가 없거나 시작/도착이 격자 밖이면 to 하나만 담아 true를 반환한다(직선 이동으로 퇴화).
	 * 도착칸이 막혀 있으면 가장 가까운 설 수 있는 칸으로 당겨서 거기까지 안내한다.
	 *
	 * @param maxNodes 펼칠 노드 상한. 넘으면 실패(false) — 한 틱이 통째로 먹히는 걸 막는다
	 */
	bool FindPath(const Vec3& from, const Vec3& to, std::vector<Vec3>& outPath,
	              int maxNodes = 20000) const;

	// ---------- 직렬화 ----------
	// 서버와 클라가 '같은' 격자를 쓰기 위한 것. 리틀엔디안 고정이고 버전만 본다
	// (지금은 양쪽 다 x64라 이걸로 충분하다. 플랫폼이 섞이면 바이트 순서를 손봐야 한다)

	void Serialize(std::vector<uint8_t>& out) const;
	bool Deserialize(const uint8_t* data, size_t size);

private:
	int  Idx(int cx, int cy) const { return cy * _w + cx; }
	bool InBounds(int cx, int cy) const { return cx >= 0 && cy >= 0 && cx < _w && cy < _h; }

	/**
	 * a→b 선분이 지나는 모든 칸을 훑는다 (supercover DDA).
	 * @param bWalkableTest true면 '걸을 수 있나'(Solid+Clearance), false면 '뚫려 있나'(Solid만)
	 *
	 * 이동 경로 당기기와 사선 판정이 서로 다른 기준을 써야 해서 플래그로 갈랐다.
	 * 여유칸(Clearance)을 통과하는 경로를 만들면 유닛이 벽에 끼고,
	 * 반대로 사선 판정에 여유칸을 넣으면 벽 옆을 스치는 화살이 막힌다.
	 */
	bool TraceLine(const Vec3& a, const Vec3& b, bool bWalkableTest) const;

	Vec3  _origin;          // 칸 (0,0)의 좌하단
	float _cell = 0.f;
	int   _w = 0, _h = 0;
	std::vector<uint8_t> _flags;

	// A* 작업 버퍼. 호출마다 w*h를 0으로 밀면 큰 격자에서 탐색보다 초기화가 더 비싸진다.
	// 그래서 세대 번호(_run)를 같이 들고 다니며 '이번 호출에서 건드린 칸'만 초기화한다.
	// const 함수에서 쓰므로 mutable — Sim은 단일 스레드라 경쟁 조건은 없다.
	mutable std::vector<float>    _g;
	mutable std::vector<int32_t>  _parent;
	mutable std::vector<uint32_t> _stamp;
	mutable std::vector<uint8_t>  _closed;
	mutable uint32_t              _run = 0;

	void EnsureScratch() const;
};
