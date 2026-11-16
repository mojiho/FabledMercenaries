#include "NavGrid.h"

#include <algorithm>   // std::reverse, std::min
#include <cmath>       // floorf, ceilf
#include <cstring>     // memcpy
#include <queue>       // std::priority_queue

namespace
{
	constexpr float BIG = 3.0e38f;                 // '아직 안 가봄'을 뜻하는 g값
	constexpr float DIAG = 1.41421356f;            // 대각 이동 비용
	constexpr uint32_t NAVGRID_MAGIC = 0x4452474Eu; // "NGRD"
	constexpr uint32_t NAVGRID_VER = 1u;

	/** 8방향 격자에서의 최단거리(옥타일). A* 휴리스틱 — 실제 비용을 넘지 않아야 최적해가 보장된다 */
	float Octile(int dx, int dy)
	{
		dx = dx < 0 ? -dx : dx;
		dy = dy < 0 ? -dy : dy;
		const int lo = dx < dy ? dx : dy;
		const int hi = dx < dy ? dy : dx;
		return (float)(hi - lo) + DIAG * (float)lo;
	}

	struct PQNode
	{
		float   f;
		int32_t idx;
	};

	struct PQCmp
	{
		bool operator()(const PQNode& a, const PQNode& b) const
		{
			// f가 작은 쪽 우선. 동점은 인덱스가 낮은 쪽 우선 —
			// 이 타이브레이크가 없으면 같은 입력에서도 경로가 갈려 서버와 결과가 달라진다.
			if (a.f != b.f) return a.f > b.f;
			return a.idx > b.idx;
		}
	};

	template <typename T>
	void Append(std::vector<uint8_t>& buf, const T& v)
	{
		const size_t at = buf.size();
		buf.resize(at + sizeof(T));
		std::memcpy(buf.data() + at, &v, sizeof(T));
	}

	template <typename T>
	bool Take(const uint8_t* data, size_t size, size_t& at, T& out)
	{
		if (at + sizeof(T) > size) return false;
		std::memcpy(&out, data + at, sizeof(T));
		at += sizeof(T);
		return true;
	}
}

// ──────────────────────────────── 베이크 ────────────────────────────────

void NavGrid::Reset(const Vec3& minCorner, float cellSize, int width, int height)
{
	_origin = minCorner;
	_cell   = cellSize;
	_w      = (cellSize > 0.f && width  > 0) ? width  : 0;
	_h      = (cellSize > 0.f && height > 0) ? height : 0;

	_flags.assign((size_t)_w * (size_t)_h, (uint8_t)NavFlag::None);

	// 작업 버퍼는 크기가 바뀌었으니 버린다 (EnsureScratch가 다시 잡는다)
	_g.clear();
	_parent.clear();
	_stamp.clear();
	_closed.clear();
	_run = 0;
}

void NavGrid::ClearFlags()
{
	std::fill(_flags.begin(), _flags.end(), (uint8_t)NavFlag::None);
}

void NavGrid::SetSolid(int cx, int cy, bool bSolid)
{
	if (!InBounds(cx, cy)) return;
	uint8_t& f = _flags[Idx(cx, cy)];
	if (bSolid) f |= (uint8_t)NavFlag::Solid;
	else        f &= (uint8_t)~(uint8_t)NavFlag::Solid;
}

void NavGrid::SetSolidAt(const Vec3& p, bool bSolid)
{
	int cx, cy;
	if (WorldToCell(p, cx, cy)) SetSolid(cx, cy, bSolid);
}

void NavGrid::Inflate(float agentRadius)
{
	if (!Valid() || agentRadius <= 0.f) return;

	const int r = (int)std::ceil(agentRadius / _cell);
	if (r <= 0) return;

	// Solid를 읽으면서 Clearance를 쓰면 방금 깐 여유칸이 다시 씨앗이 되어 번진다.
	// 그래서 원본 Solid 분포를 따로 떠 놓고 거기서만 읽는다.
	const std::vector<uint8_t> src = _flags;
	const int r2 = r * r;

	for (int y = 0; y < _h; ++y)
	for (int x = 0; x < _w; ++x)
	{
		const int here = Idx(x, y);
		if (src[here] & (uint8_t)NavFlag::Solid) continue;   // 이미 벽

		bool bNear = false;
		for (int dy = -r; dy <= r && !bNear; ++dy)
		for (int dx = -r; dx <= r; ++dx)
		{
			if (dx * dx + dy * dy > r2) continue;            // 원형 반경
			const int nx = x + dx, ny = y + dy;
			if (!InBounds(nx, ny)) continue;                 // 격자 경계는 벽이 아니다
			if (src[Idx(nx, ny)] & (uint8_t)NavFlag::Solid) { bNear = true; break; }
		}

		if (bNear) _flags[here] |= (uint8_t)NavFlag::Clearance;
	}
}

// ──────────────────────────────── 조회 ────────────────────────────────

bool NavGrid::WorldToCell(const Vec3& p, int& cx, int& cy) const
{
	if (!Valid()) { cx = 0; cy = 0; return false; }

	const int ix = (int)std::floor((p.x - _origin.x) / _cell);
	const int iy = (int)std::floor((p.y - _origin.y) / _cell);

	const bool bInside = (ix >= 0 && iy >= 0 && ix < _w && iy < _h);

	// 밖이어도 가장 가까운 칸으로 잘라서 돌려준다 — 호출부가 매번 클램프하지 않게
	cx = ix < 0 ? 0 : (ix >= _w ? _w - 1 : ix);
	cy = iy < 0 ? 0 : (iy >= _h ? _h - 1 : iy);
	return bInside;
}

Vec3 NavGrid::CellToWorld(int cx, int cy) const
{
	return Vec3{ _origin.x + ((float)cx + 0.5f) * _cell,
	             _origin.y + ((float)cy + 0.5f) * _cell,
	             _origin.z };
}

bool NavGrid::IsSolid(int cx, int cy) const
{
	if (!InBounds(cx, cy)) return false;   // 격자 밖 = 열린 지형
	return (_flags[Idx(cx, cy)] & (uint8_t)NavFlag::Solid) != 0;
}

bool NavGrid::IsWalkable(int cx, int cy) const
{
	if (!InBounds(cx, cy)) return true;    // 격자 밖 = 열린 지형
	return (_flags[Idx(cx, cy)] & ((uint8_t)NavFlag::Solid | (uint8_t)NavFlag::Clearance)) == 0;
}

bool NavGrid::IsWalkableAt(const Vec3& p) const
{
	if (!Valid()) return true;
	int cx, cy;
	if (!WorldToCell(p, cx, cy)) return true;
	return IsWalkable(cx, cy);
}

bool NavGrid::TraceLine(const Vec3& a, const Vec3& b, bool bWalkableTest) const
{
	if (!Valid()) return true;

	auto Passable = [this, bWalkableTest](int cx, int cy) -> bool
	{
		return bWalkableTest ? IsWalkable(cx, cy) : !IsSolid(cx, cy);
	};

	// 선분이 '스치는' 칸까지 하나도 빠짐없이 훑는다 (Amanatides–Woo 격자 순회).
	//
	// 예전엔 브레젠험이었는데, 그건 장축 한 칸마다 칸 하나씩만 찍으므로
	// 선이 실제로 지나는 칸을 건너뛴다. 벽 모서리를 비스듬히 스치는 사선이
	// '트였다'로 나왔고, 그래서 궁수가 쏠 수 있다고 믿고 벽에 화살을 박았다.
	// (화살 쪽은 10cm씩 쪼개 검사해서 제대로 막혔다 — 둘의 답이 갈렸다)
	const float ax = (a.x - _origin.x) / _cell;
	const float ay = (a.y - _origin.y) / _cell;
	const float bx = (b.x - _origin.x) / _cell;
	const float by = (b.y - _origin.y) / _cell;

	int       x  = (int)std::floor(ax);
	int       y  = (int)std::floor(ay);
	const int x1 = (int)std::floor(bx);
	const int y1 = (int)std::floor(by);

	if (!Passable(x, y))       return false;
	if (x == x1 && y == y1)    return true;   // 같은 칸 안

	const float dx = bx - ax;
	const float dy = by - ay;
	const int   sx = (dx > 0.f) ? 1 : ((dx < 0.f) ? -1 : 0);
	const int   sy = (dy > 0.f) ? 1 : ((dy < 0.f) ? -1 : 0);

	constexpr float INF = 1e30f;

	// tMax = 다음 칸 경계까지의 진행도(0~1), tDelta = 한 칸 건너는 데 드는 진행도
	float tMaxX = (sx == 0) ? INF : ((float)(sx > 0 ? x + 1 : x) - ax) / dx;
	float tMaxY = (sy == 0) ? INF : ((float)(sy > 0 ? y + 1 : y) - ay) / dy;
	const float tDeltaX = (sx == 0) ? INF : (float)sx / dx;
	const float tDeltaY = (sy == 0) ? INF : (float)sy / dy;

	// 격자 밖 좌표가 들어와도 멈추도록 상한을 둔다 (밖은 열린 지형이라 끝없이 돌 수 있다)
	int guard = 4 * (_w + _h) + 16;

	while (guard-- > 0)
	{
		if (tMaxX < tMaxY) { x += sx; tMaxX += tDeltaX; }
		else               { y += sy; tMaxY += tDeltaY; }

		if (!Passable(x, y))    return false;
		if (x == x1 && y == y1) return true;

		// 선분 끝을 지났다 (두 축 모두 다음 경계가 1을 넘음)
		if (tMaxX > 1.f && tMaxY > 1.f) return true;
	}

	return true;
}

bool NavGrid::LineOfSight(const Vec3& a, const Vec3& b) const
{
	return TraceLine(a, b, /*bWalkableTest=*/false);
}

bool NavGrid::CanWalkStraight(const Vec3& a, const Vec3& b) const
{
	return TraceLine(a, b, /*bWalkableTest=*/true);
}

bool NavGrid::NearestWalkable(const Vec3& p, Vec3& out, int maxRing) const
{
	if (!Valid()) { out = p; return true; }

	int cx, cy;
	if (!WorldToCell(p, cx, cy))  { out = p; return true; }   // 격자 밖 = 열린 지형
	if (IsWalkable(cx, cy))       { out = p; return true; }

	for (int r = 1; r <= maxRing; ++r)
	{
		int   bestIdx = -1;
		float bestD2  = BIG;

		for (int dy = -r; dy <= r; ++dy)
		for (int dx = -r; dx <= r; ++dx)
		{
			// 링 둘레만 — 안쪽은 지난 반복에서 이미 봤다
			if (dx != -r && dx != r && dy != -r && dy != r) continue;

			const int nx = cx + dx, ny = cy + dy;
			if (!InBounds(nx, ny))   continue;
			if (!IsWalkable(nx, ny)) continue;

			const Vec3 c = CellToWorld(nx, ny);
			const float ddx = c.x - p.x, ddy = c.y - p.y;
			const float d2 = ddx * ddx + ddy * ddy;

			// 동점은 인덱스가 낮은 쪽 — 결정적 결과를 위해
			const int idx = Idx(nx, ny);
			if (d2 < bestD2 || (d2 == bestD2 && (bestIdx < 0 || idx < bestIdx)))
			{
				bestD2 = d2;
				bestIdx = idx;
			}
		}

		if (bestIdx >= 0)
		{
			out = CellToWorld(bestIdx % _w, bestIdx / _w);
			return true;
		}
	}

	return false;   // 주변이 통째로 막혔다
}

// ──────────────────────────────── A* ────────────────────────────────

void NavGrid::EnsureScratch() const
{
	const size_t n = (size_t)_w * (size_t)_h;
	if (_g.size() == n) return;

	_g.resize(n);
	_parent.resize(n);
	_closed.resize(n);
	_stamp.assign(n, 0u);   // 0 = '한 번도 안 건드림'. _run은 1부터 올라간다
	_run = 0;
}

bool NavGrid::FindPath(const Vec3& from, const Vec3& to, std::vector<Vec3>& outPath, int maxNodes) const
{
	outPath.clear();

	if (!Valid()) { outPath.push_back(to); return true; }   // 격자 없음 → 직선

	int sx, sy, gx, gy;
	const bool bSIn = WorldToCell(from, sx, sy);
	const bool bGIn = WorldToCell(to, gx, gy);

	// 시작이든 도착이든 격자 밖이면 길을 찾을 수 없다 → 직선으로 퇴화(얼어붙는 것보다 낫다)
	if (!bSIn || !bGIn) { outPath.push_back(to); return true; }

	// 대부분의 이동은 그냥 직선이다. 눈에 보이면 A*를 돌리지 않는다.
	if (IsWalkable(gx, gy) && TraceLine(from, to, true))
	{
		outPath.push_back(to);
		return true;
	}

	// 도착칸이 못 서는 칸이면(벽을 클릭했다) 가장 가까운 설 수 있는 칸으로 당긴다
	Vec3 goal = to;
	if (!IsWalkable(gx, gy))
	{
		Vec3 snapped;
		if (!NearestWalkable(to, snapped)) return false;
		goal = snapped;
		WorldToCell(goal, gx, gy);
	}

	const int startIdx = Idx(sx, sy);
	const int goalIdx  = Idx(gx, gy);
	if (startIdx == goalIdx) { outPath.push_back(goal); return true; }

	EnsureScratch();
	++_run;

	// 이번 호출에서 처음 보는 칸만 초기화한다 (큰 격자에서 w*h를 매번 미는 비용 회피)
	auto Touch = [this](int i)
	{
		if (_stamp[i] != _run)
		{
			_stamp[i]  = _run;
			_g[i]      = BIG;
			_parent[i] = -1;
			_closed[i] = 0;
		}
	};

	Touch(startIdx);
	_g[startIdx] = 0.f;

	std::priority_queue<PQNode, std::vector<PQNode>, PQCmp> open;
	open.push({ Octile(gx - sx, gy - sy), startIdx });

	static const int DX[8] = {  1, -1,  0,  0,  1,  1, -1, -1 };
	static const int DY[8] = {  0,  0,  1, -1,  1, -1,  1, -1 };

	int  expanded = 0;
	bool bFound   = false;

	while (!open.empty())
	{
		const int cur = open.top().idx;
		open.pop();

		Touch(cur);
		if (_closed[cur]) continue;   // 더 좋은 경로로 이미 처리됨
		_closed[cur] = 1;

		if (cur == goalIdx) { bFound = true; break; }
		if (++expanded > maxNodes) break;   // 한 틱이 통째로 먹히는 것 방지

		const int cx = cur % _w;
		const int cy = cur / _w;

		for (int n = 0; n < 8; ++n)
		{
			const int nx = cx + DX[n];
			const int ny = cy + DY[n];
			if (!InBounds(nx, ny))   continue;
			if (!IsWalkable(nx, ny)) continue;

			const bool bDiag = (DX[n] != 0 && DY[n] != 0);
			// 대각으로 벽 모서리를 뚫는 걸 막는다 — 양옆이 다 열려 있을 때만 대각 허용
			if (bDiag && (!IsWalkable(cx + DX[n], cy) || !IsWalkable(cx, cy + DY[n])))
				continue;

			const int ni = Idx(nx, ny);
			Touch(ni);
			if (_closed[ni]) continue;

			const float ng = _g[cur] + (bDiag ? DIAG : 1.f);
			if (ng < _g[ni])
			{
				_g[ni]      = ng;
				_parent[ni] = cur;
				open.push({ ng + Octile(gx - nx, gy - ny), ni });
			}
		}
	}

	if (!bFound) return false;

	// 칸 중심들을 이어 붙인 원시 경로
	std::vector<Vec3> raw;
	for (int i = goalIdx; i != -1; i = _parent[i])
		raw.push_back(CellToWorld(i % _w, i / _w));
	std::reverse(raw.begin(), raw.end());

	raw.front() = from;   // 첫 점은 칸 중심이 아니라 유닛의 실제 위치
	raw.back()  = goal;   // 끝 점도 실제 목표점

	// 당기기(string pulling) — 직선으로 보이는 구간은 중간 점을 버린다.
	// 안 하면 유닛이 칸 중심마다 꺾이며 계단으로 걷는다.
	size_t anchor = 0;
	for (size_t i = 2; i < raw.size(); ++i)
	{
		if (!TraceLine(raw[anchor], raw[i], true))
		{
			outPath.push_back(raw[i - 1]);   // 직선이 끊기기 직전 점이 꺾임점
			anchor = i - 1;
		}
	}
	outPath.push_back(raw.back());

	return true;
}

// ──────────────────────────────── 직렬화 ────────────────────────────────

void NavGrid::Serialize(std::vector<uint8_t>& out) const
{
	out.clear();
	Append(out, NAVGRID_MAGIC);
	Append(out, NAVGRID_VER);
	Append(out, _cell);
	Append(out, (int32_t)_w);
	Append(out, (int32_t)_h);
	Append(out, _origin.x);
	Append(out, _origin.y);
	Append(out, _origin.z);

	if (!_flags.empty())
	{
		const size_t at = out.size();
		out.resize(at + _flags.size());
		std::memcpy(out.data() + at, _flags.data(), _flags.size());
	}
}

bool NavGrid::Deserialize(const uint8_t* data, size_t size)
{
	if (!data) return false;

	size_t at = 0;
	uint32_t magic = 0, ver = 0;
	float cell = 0.f;
	int32_t w = 0, h = 0;
	Vec3 origin;

	if (!Take(data, size, at, magic) || magic != NAVGRID_MAGIC) return false;
	if (!Take(data, size, at, ver)   || ver   != NAVGRID_VER)   return false;
	if (!Take(data, size, at, cell)) return false;
	if (!Take(data, size, at, w))    return false;
	if (!Take(data, size, at, h))    return false;
	if (!Take(data, size, at, origin.x)) return false;
	if (!Take(data, size, at, origin.y)) return false;
	if (!Take(data, size, at, origin.z)) return false;

	if (cell <= 0.f || w <= 0 || h <= 0) return false;

	const size_t cells = (size_t)w * (size_t)h;
	if (at + cells != size) return false;   // 칸 수와 남은 바이트가 정확히 맞아야 한다

	Reset(origin, cell, (int)w, (int)h);
	std::memcpy(_flags.data(), data + at, cells);
	return true;
}
