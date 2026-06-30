#pragma once

#include <cstdint>			// uint32_t, uint64_t
#include <memory>			// std::unique_ptr
#include <unordered_map>	// std::unordered_map
#include <functional>		// std::function
#include <vector>			// std::vector

#include "Vec3.h"
#include "Command.h"
#include "Unit.h"
#include "Commander.h"
#include "Projectile.h"

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

	// 상위(UE) 레이어 구독 — Sim은 애니를 모르고 이벤트만 통지
	std::function<void(uint64_t unitId, uint32_t slotId)>           OnCommandComplete; // 명령 완료
	std::function<void(uint64_t unitId)>                            OnAttackFired;     // 휘두름/발사 순간
	std::function<void(uint64_t unitId, bool fromBehind, bool crit)> OnDamaged;        // 피격 리액션
	std::function<void(uint64_t unitId)>                            OnDeath;           // 사망
	std::function<void(uint64_t unitId, SkillType type)>           OnSkillCast;        // 스킬 시전

	// 읽기 전용 (렌더가 pos 등 조회)
	const std::unordered_map<uint64_t, Unit>& Units() const { return _units; }

	Unit* FindNearestHostile(const Unit& self);   // 살아있는 다른 진영 중 최근접 (없으면 nullptr)

	const std::vector<Projectile>& Projectiles() const { return _projectiles; }  // 발사체(화살/투사체) 목록

private:
	uint32_t NewSlot() { return ++_slotGen; }  // slotId 발급기. 0은 예약 안 된 명령용.

	bool     TryDefend(Unit& defender);   // 방어 패시브 롤(성공 시 쿨다운 시작)
	float    Rand01();                    // 결정적 난수 [0,1)
	uint32_t _rng = 2463534242u;          // 난수 시드(고정 → 재현 가능)

	std::unordered_map<uint64_t, Unit>      _units;       // Unit id → Unit 값 보관(Unit은 이동전용)
	std::unordered_map<uint64_t, Commander> _commanders;  // per-player 게이지
	uint32_t _slotGen = 0;

	// 튜닝 상수(1차값 — P0에서 조정)
	static constexpr float ISSUE_COST    = 1.0f;   // 명령 1회 발행 비용
	static constexpr float EXEC_RATE     = 1.0f;   // execGauge 진행/초 (선/후딜 타이머 속도)
	static constexpr float CRIT_MULT     = 2.0f;   // 후면 크리 배율 (암살자는 후속에 더 크게)
	static constexpr float DEFEND_BLOCK_CHANCE = 0.85f; // 방어 태세 시 막을 확률(쿨다운 무시)
	static constexpr float FOCUS_RATE          = 25.f;  // 정신집중 MP 충전/초 (mpMax 100 → 4초)
	static constexpr float CHARGE_STUN = 0.8f;
	std::vector<Projectile> _projectiles;		// 발사체(화살/투사체) 목록
	uint32_t _projGen = 0;	// 발사체 id 발급기

	bool ResolveHit(Unit& target, const Vec3& fromPos, float damage);		// 피격 처리. 방어/크리 판정, HP 감소, 사망 처리. true=살아있음, false=사망
	void SpawnProjectile(Unit& owner, Unit& tartget, float damage, bool arc);						// 발사체 생성. owner=발사자, target=목표. Projectile 목록에 추가
	void UpdateProjectiles(float dt);								// 발사체 이동, 충돌 처리, 사망 처리
};
