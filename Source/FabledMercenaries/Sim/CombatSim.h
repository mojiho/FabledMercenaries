#pragma once

#include <cstdint>			// uint32_t, uint64_t
#include <memory>			// std::unique_ptr
#include <unordered_map>	// std::unordered_map
#include <functional>		// std::function

#include "Vec3.h"
#include "Command.h"
#include "Unit.h"
#include "Commander.h"

enum class CommandResult { Accepted, Rejected, Queued, Cancelled };

class CombatSim
{
public:	// 등록 — brain 기본 null(=사람 조종). AI면 구체 Brain을 move로 넘김.
	Unit& AddUnit(uint64_t id, uint64_t ownerId, Faction faction, Class cls, const Vec3& pos, std::unique_ptr<Brain> brain = nullptr); // 클래스(이속/스킬) 주입. Brain은 move로 소유권 이전.
	Commander& AddCommander(uint64_t playerId); // Commander id는 외부에서 발급, Sim은 단순 등록만.
	Unit* GetUnit(uint64_t id);		// Unit id로 검색, 없으면 null

	//명령
	CommandResult IssueCommand(uint64_t unitId, Command cmd, bool reserve);	// Unit id로 명령 발급. Unit이 없으면 Rejected. Unit이 수행 중이면 Queued. 성공 시 Accepted.
	CommandResult CancelCommand(uint64_t unitId, uint32_t slotId);	// Unit id로 명령 취소. Unit이 없으면 Rejected. slotId가 없으면 Rejected. 성공 시 Cancelled.
	void CancelAllReserved(uint64_t unitId);	// Unit id로 예약 명령 전체 취소. Unit이 없으면 무시.

	void Tick(float dt);	// 시뮬레이션 진행. dt는 초 단위. 모든 Unit의 Brain.Decide 호출, 명령 수행 진행.

	// 상위(UE) 레이어 구독: 명령 완료 알림
	std::function<void(uint64_t unitId, uint32_t slotId)> OnCommandComplete; // Unit이 명령 완료 시 호출. slotId는 완료된 명령의 slotId.

	// 읽기 전용 (렌더가 pos 등 조회)
	const std::unordered_map<uint64_t, Unit>& Units() const { return _units; }

	Unit* FindNearestHostile(const Unit& self);   // 살아있는 다른 진영 중 최근접 (없으면 nullptr)
private:
	uint32_t NewSlot() { return ++_slotGen; }  // slotId 발급기. 0은 예약 안 된 명령용.

	bool     TryDefend(Unit& defender);   // 방어 패시브 롤(성공 시 쿨다운 시작)
	float    Rand01();                    // 결정적 난수 [0,1)
	uint32_t _rng = 2463534242u;          // 난수 시드(고정 → 재현 가능)

	std::unordered_map <uint64_t, Unit> _units;	// Unit id → Unit 값 보관(Unit은 이동전용)
	std::unordered_map <uint64_t, Commander> _commanders;	// per-player 게이지
	uint32_t _slotGen = 0;

	// 튜닝 상수(1차값 — P0에서 조정)
	static constexpr float ISSUE_COST = 1.0f;   // 명령 1회 발행 비용
	static constexpr float EXEC_RATE = 1.0f;   // 수행 게이지 충전/초
	static constexpr float EXEC_THRESH = 1.0f;   // 공격/스킬 발동 임계
	static constexpr float ATTACK_DAMAGE = 25.f;   // 1회 타격 데미지 (100hp = 4대)
};