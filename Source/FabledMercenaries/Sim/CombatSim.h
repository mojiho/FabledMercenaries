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
#include "Item.h"
#include "NavGrid.h"

enum class CommandResult { Accepted, Rejected, Queued, Cancelled, Disobeyed };

class CombatSim
{
public:	// 등록 — brain 기본 null(=사람 조종). AI면 구체 Brain을 move로 넘김.
	Unit& AddUnit(uint64_t id, uint64_t ownerId, Faction faction, Class cls, const Vec3& pos, std::unique_ptr<Brain> brain = nullptr); // 클래스(이속/스킬) 주입. Brain은 move로 소유권 이전.
	Commander& AddCommander(uint64_t playerId, CommanderType type = CommanderType::Command); // Commander id는 외부에서 발급, Sim은 단순 등록만.
	Unit* GetUnit(uint64_t id);		// Unit id로 검색, 없으면 null

	// 유닛 제거 — 전투 종료 시 용병/적을 필드에서 내린다(탐험맵엔 지휘관 아바타만 남음).
	// 지휘관 usedCost를 되돌리고, 이 유닛과 얽힌 발사체를 무효화한다.
	// 주의: Tick() 안에서 호출하지 말 것(_units 순회 중 제거 = 이터레이터 무효화).
	bool RemoveUnit(uint64_t id);

	// 장비 적용 — AddUnit 직후 1회. 공격력/최대체력에 장비 보너스를 더하고 체력도 같이 올린다.
	// (인덱스 = EquipSlot - 1, 0 = 빈 칸)
	void ApplyEquipment(uint64_t id, const uint32_t (&equip)[EQUIP_SLOT_COUNT]);

	//명령
	CommandResult IssueCommand(uint64_t unitId, Command cmd, bool reserve);	// Unit id로 명령 발급. Unit이 없으면 Rejected. Unit이 수행 중이면 Queued. 성공 시 Accepted.
	CommandResult CancelCommand(uint64_t unitId, uint32_t slotId);	// Unit id로 명령 취소. Unit이 없으면 Rejected. slotId가 없으면 Rejected. 성공 시 Cancelled.
	void CancelAllReserved(uint64_t unitId);	// Unit id로 예약 명령 전체 취소. Unit이 없으면 무시.

	void Tick(float dt);	// 시뮬레이션 진행. dt는 초 단위. 모든 Unit의 Brain.Decide 호출, 명령 수행 진행.

	// 상위(UE) 레이어 구독 — Sim은 애니를 모르고 이벤트만 통지
	std::function<void(uint64_t unitId, uint32_t slotId)>           OnCommandComplete; // 명령 완료
	std::function<void(uint64_t unitId)>                            OnAttackFired;     // 휘두름/발사 순간
	// 피격 리액션. damage는 방어/크리 배율이 모두 적용된 '실제로 깎인' 값 — 화면 팝업이 이 숫자를 띄운다
	std::function<void(uint64_t unitId, float damage, bool fromBehind, bool crit)> OnDamaged;
	// 막아냄 (피해 0). 어떤 방식으로 막았는지까지 넘긴다 — 패링/블록/회피를 따로 보여주기 위해
	std::function<void(uint64_t unitId, DefenseKind kind)>           OnDefended;
	std::function<void(uint64_t unitId)>                            OnDeath;           // 사망
	std::function<void(uint64_t unitId, SkillType type)>           OnSkillCast;        // 스킬 시전

	// ---------- 지형 ----------

	/**
	 * 장애물 격자 주입. null이면 '장애물 없음'으로 동작한다(격자를 넣기 전과 완전히 같다).
	 * 소유권은 호출자에게 남는다 — 레벨이 살아 있는 동안 격자도 살아 있어야 한다.
	 * 레벨을 넘어갈 때는 반드시 다시 주입하거나 null로 비울 것.
	 */
	void SetNavGrid(const NavGrid* nav) { _nav = nav; }
	const NavGrid* Nav() const { return _nav; }

	/** 사선이 비었는가. 격자가 없으면 항상 true — 공격/스킬이 벽을 통과하지 않게 */
	bool HasLineOfSight(const Vec3& from, const Vec3& to) const;

	/**
	 * self가 target에게 사선을 확보할 수 있는 '가장 가까운 자리'를 찾는다.
	 *
	 * 없으면 false — 호출부는 예전처럼 대상 쪽으로 접근하면 된다.
	 * 이게 없으면 사거리 700짜리 궁수가 벽에 가렸을 때 대상 위치로 직진해서,
	 * 벽을 빙 돌아 적 코앞까지 걸어간 다음 쏜다. 사거리를 통째로 버리는 셈이다.
	 *
	 * 조건: 걸을 수 있는 칸 / 거기서 대상이 사거리 안 / 대상까지 사선이 트임 /
	 *       지금 자리에서 똑바로 걸어갈 수 있음(우회가 필요한 자리는 고르지 않는다 —
	 *       벽 반대편 칸을 골라 놓고 한참 돌아가는 동작을 막는다).
	 */
	bool FindVantagePoint(const Unit& self, const Unit& target, Vec3& out) const;

	// 읽기 전용 (렌더가 pos 등 조회)
	const std::unordered_map<uint64_t, Unit>& Units() const { return _units; }

	Unit* FindNearestHostile(const Unit& self);   // 살아있는 다른 진영 중 최근접 (없으면 nullptr)

	const std::vector<Projectile>& Projectiles() const { return _projectiles; }  // 발사체(화살/투사체) 목록

private:
	uint32_t NewSlot() { return ++_slotGen; }  // slotId 발급기. 0은 예약 안 된 명령용.

	/** 방어 롤(성공 시 쿨다운 시작). @return 막아낸 방식, None이면 못 막음 */
	DefenseKind TryDefend(Unit& defender);
	float    Rand01();                    // 결정적 난수 [0,1)
	uint32_t _rng = 2463534242u;          // 난수 시드(고정 → 재현 가능)

	std::unordered_map<uint64_t, Unit>      _units;       // Unit id → Unit 값 보관(Unit은 이동전용)
	std::unordered_map<uint64_t, Commander> _commanders;  // per-player 게이지
	uint32_t _slotGen = 0;

	bool _aiIssuing = false;   // 브레인 의사결정 중 = AI 명령 → 게이지/불복종 면제(반격은 자원 무소모)

	// 튜닝 상수(1차값 — P0에서 조정)
	static constexpr float ISSUE_COST    = 1.0f;   // 명령 1회 발행 비용
	static constexpr float EXEC_RATE     = 1.0f;   // execGauge 진행/초 (선/후딜 타이머 속도)
	static constexpr float CRIT_MULT     = 2.0f;   // 후면 크리 배율 (암살자는 후속에 더 크게)
	static constexpr float DEFEND_BLOCK_CHANCE = 0.85f; // 방어 태세 시 막을 확률(쿨다운 무시)
	static constexpr float FOCUS_RATE          = 25.f;  // 정신집중 MP 충전/초 (mpMax 100 → 4초)
	static constexpr float CHARGE_STUN = 0.8f;
	std::vector<Projectile> _projectiles;		// 발사체(화살/투사체) 목록
	uint32_t _projGen = 0;	// 발사체 id 발급기
	static constexpr float DISOBEY_SLOPE = 1.0f;   // 코스트 초과비율당 불복종 증가
	static constexpr float DISOBEY_MAX   = 0.75f;  // 불복종 확률 상한

	// ---------- 지형 (내부) ----------
	const NavGrid* _nav = nullptr;                 // 소유하지 않는다. null = 장애물 없음

	static constexpr float REPATH_INTERVAL = 0.4f; // 추격 경로 재탐색 간격(초). 매 틱 A*는 너무 비싸다

	/** Move 명령의 요청 지점들을 격자 경로로 치환. 격자가 없으면 그대로 둔다 */
	void BuildPath(const Unit& u, Command& cmd) const;

	/** 장애물을 고려한 한 걸음의 '도착 위치'. 막히면 벽을 따라 미끄러지고, 완전히 막히면 제자리 */
	Vec3 ResolveStep(const Unit& u, const Vec3& desired) const;

	/** from→to 직선에서 걸어서 갈 수 있는 마지막 지점 (돌진이 벽을 뚫지 않게) */
	Vec3 LastWalkableAlong(const Vec3& from, const Vec3& to) const;

	/** 움직이는 목표를 향해 한 걸음. 직선이 막혔으면 우회 경로를 깔아 따라간다 */
	void ChaseStep(Unit& u, const Vec3& goal, float dt);

	/** 새 명령을 설치할 때 지난 명령의 이동 흔적(우회 경로·끼임 타이머)을 지운다 */
	static void ResetNavState(Unit& u);

	// ---------- 몸 겹침 ----------

	/**
	 * 겹친 유닛을 서로 밀어낸다. 명령 수행이 '끝난 뒤' 한 번 돈다.
	 *
	 * 결정성을 위해 두 가지를 지킨다:
	 *  - id 순으로 순회한다 (_units는 unordered_map이라 순회 순서가 보장되지 않는다)
	 *  - 밀림량을 전부 모은 뒤 한 번에 적용한다 (먼저 처리된 쪽이 유리해지지 않게)
	 * 둘 중 하나라도 빠지면 같은 입력에서 서버와 클라의 좌표가 갈린다.
	 */
	void SeparateUnits(float dt);

	static constexpr float PUSH_RATE      = 0.6f;   // 겹친 양의 몇 할을 이번 틱에 떼어낼지 (1=즉시, 떨림)
	static constexpr float STUCK_GIVEUP   = 1.2f;   // 이 시간 동안 나아가지 못하면 이동 포기(초)
	static constexpr float STUCK_PROGRESS = 0.3f;   // 한 걸음의 이 비율도 못 줄였으면 '못 나아갔다'
	static constexpr float PROJ_HIT_MIN   = 30.f;   // 발사체 피격 반경의 하한 (몸이 얇아도 이만큼은 맞는다)

	// 사격 자리 찾기 — 근접은 어차피 붙어야 하므로 사거리가 이보다 긴 유닛만 쓴다
	static constexpr float VANTAGE_MIN_RANGE = 250.f;
	static constexpr float VANTAGE_INTERVAL  = 0.4f;  // 재탐색 간격(초)
	static constexpr int   VANTAGE_MAX_RING  = 10;    // 자기 칸에서 이 칸 수까지만 (10칸 = 500cm)

	// 밀어내기용 스크래치 — 매 틱 할당하지 않게 멤버로 들고 있는다
	std::vector<Unit*> _pushList;
	std::vector<Vec3>  _pushDelta;

	bool ResolveHit(Unit& target, const Vec3& fromPos, float damage);		// 피격 처리. 방어/크리 판정, HP 감소, 사망 처리. true=살아있음, false=사망
	void ApplyAreaDamage(const Unit& caster, const Vec3& center, float radius, float damage);	// 범위 피해. center 반경 안의 '적'만, 거리는 평면(XY)으로 잰다
	void SpawnProjectile(Unit& owner, Unit& tartget, float damage, bool arc);						// 발사체 생성. owner=발사자, target=목표. Projectile 목록에 추가
	void UpdateProjectiles(float dt);								// 발사체 이동, 충돌 처리, 사망 처리


};
