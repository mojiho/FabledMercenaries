# Fabled Mercenaries — P0 싱글 프로토타입 설계 & 작업 핸드오프

> **목적**: 서버보다 **UE5 싱글플레이 프로토타입을 먼저** 만들어 코어 전투 루프(2계층 게이지·명령·인터럽트·예약)의 재미를 검증한다. 그 다음 검증된 시뮬을 서버로 이식한다.
> **이 문서만 보고 다른 PC에서 콜드 스타트**할 수 있도록 작성됨. 상위 설계는 `fabled_mercenaries_combat_network_design.md`(v1), 서버 설계는 `server_technical_design_v2.md` 참조.
> ⚠️ 이 docs 폴더는 서버 git 리포(`MMO/Server` = IOCP.git) **바깥**이다. 다른 PC에서 보려면 이 파일을 따로 동기화하거나 UE 프로토타입 리포에 함께 커밋할 것.

---

## 0. 의사결정 기록 (2026-06-23)

### 네트워킹 방향: **Path A — 커스텀 IOCP 서버 (확정)**
- UE 데디케이티드 서버(+Iris)를 검토했으나 **채택 안 함.** 이유: C++ 실시간 서버 구축 자체가 목표(학습·기술자산) + 대규모 지속월드·저비용 지향.
- 함의: 실시간 시뮬을 IOCP 서버가 직접 돌림 → 클라/서버 두 곳에서 같은 시뮬이 돌아야 함 → **아래 "엔진 비의존 Sim 코어 격리"가 선택이 아니라 필수.**
- 백엔드(로그인/DB/매치메이킹)는 IOCP가 아닌 별도 경량 서비스로. 데디서버를 쓰면 IOCP가 무의미해지므로 하이브리드는 배제, 단일 IOCP로 간다.

### 왜 이 순서인가

- 1인 개발 → **재미 검증 전엔 네트워크(서버 권위 틱/AOI/스냅샷)를 짓지 않는다.**
- 순서: **P0 싱글 전투루프 → P1 탐험/인카운트 → P2 시뮬코어 서버 이식 → P3 서버권위·AOI·멀티.**
- ⚠️ **최대 원칙**: 코어 시뮬을 **엔진 비의존 순수 C++**로 격리한다. UE의 `AActor`/`UWorld`/`UObject`/블루프린트에 시뮬 로직을 묶지 말 것. 그래야 **똑같은 코드를 나중에 서버 틱 루프에 그대로 얹는다**(= C++ 통일을 택한 이유).

---

## 1. 아키텍처 — 2층 분리

```
┌─────────────────────────────────────────┐
│  UE5 레이어 (엔진 의존)                   │
│  - 입력(클릭) → Command 생성 → Sim 호출   │
│  - 매 프레임 Sim.Tick(DeltaTime)          │  ← 싱글: 클라가 시뮬 직접 구동
│  - Unit.pos 읽어 Actor 위치/애님 렌더      │     (멀티 전환 시 이 Tick이 서버로 이동)
│  - 게이지 UI                              │
└───────────────┬─────────────────────────┘
                │ (호출만)
┌───────────────▼─────────────────────────┐
│  Sim 코어 (엔진 비의존 순수 C++)          │  ← 이 폴더를 나중에 서버가 그대로 컴파일
│  Vec3 / Command / Unit / Commander       │
│  CombatSim: Tick(dt), IssueCommand, ...   │
└───────────────────────────────────────────┘
```

**폴더 구조 제안** (UE 프로젝트 내):
- `Source/<Proj>/Sim/` ← 순수 C++. `#include "CoreMinimal.h"` 금지, UE 타입 금지. 표준 라이브러리만.
- `Source/<Proj>/Game/` ← UE 액터/폰/플레이어컨트롤러/위젯. Sim을 호출.
- 나중에 서버는 `Sim/` 파일들을 자기 프로젝트에 추가해 동일 로직 공유.

> UE 빌드에서 Sim 폴더가 `CoreMinimal.h` 없이 컴파일되게 하려면, 해당 .cpp들이 PCH를 강제 포함하지 않도록 주의(또는 Sim을 별도 비-UE static lib 모듈로 분리). 프로토타입 초기엔 같은 모듈에 두되 UE 타입만 안 쓰면 충분.

---

## 2. Sim 코어 — 클래스 스케치 (엔진 비의존)

> 아래는 **직접 타이핑하며 학습**하기 위한 골격. 채워가며 만들 것.

```cpp
// Sim/Vec3.h
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator-(const Vec3& o) const { return { x-o.x, y-o.y, z-o.z }; }
    Vec3 operator+(const Vec3& o) const { return { x+o.x, y+o.y, z+o.z }; }
    Vec3 operator*(float s)       const { return { x*s, y*s, z*s }; }
    float Length() const;          // sqrt(x*x+y*y+z*z)
    Vec3  Normalized() const;      // length 0 가드
};
```

```cpp
// Sim/Command.h
#include <vector>
#include <cstdint>

enum class CommandType : uint16_t { None, Move, Attack, Skill };

struct Command {
    uint32_t          slotId   = 0;     // 취소/완료 식별 키 (Sim이 발급)
    CommandType       type     = CommandType::None;
    std::vector<Vec3> waypoints;        // Move: 경로 리스트(A형)
    uint64_t          targetId = 0;     // Attack/Skill 대상
    uint32_t          skillId  = 0;
    uint32_t          issueSeq = 0;     // 입력 발행 순번
};
```

> **설계 진화(2026-06-25 최종 — faction/brain 컴포지션)**: 원칙 = *런타임에 안 바뀌는 것만 상속, 바뀌는 것은 데이터/컴포넌트*. 적→아군 전향이 가능 → Enemy를 클래스로 분리하면 깨짐(객체 클래스 런타임 변경 불가). 그래서 **Enemy/Character 서브클래스 폐기 → `Unit` 단일 타입.**
> - `Unit`: `Faction faction`(데이터, 변경 가능) + `unique_ptr<Brain> brain`(컴포넌트, null=사람/있으면 AI). 진영=데이터, 조종주체=컴포넌트.
> - `Brain`(추상): AI 의사결정 전략. 전향 = faction 변경(+brain 교체/해제), id·상태 유지.
> - `Commander`(Sim, per-player): cmdGauge. `Player`(메타, Sim 밖): 로스터·재화·아이템.
> - 서버 대응: `Unit`↔`Creature`, faction/brain은 거기서도 데이터/컴포넌트, `Player`(session)만 서브클래스.

```cpp
// Sim/Brain.h — AI 의사결정 전략(갈아끼우는 두뇌)
class CombatSim;
class Unit;

class Brain {
public:
    virtual ~Brain() = default;                                    // 베이스 포인터로 다룸 → virtual
    virtual void Decide(CombatSim& sim, Unit& self, float dt) = 0; // 스스로 IssueCommand
};
```

```cpp
// Sim/Unit.h — 단일 구체 타입(상속 없음). 진영=데이터, 조종주체=컴포넌트.
#include <cstdint>
#include <deque>
#include <memory>
#include "Vec3.h"
#include "Command.h"
#include "Brain.h"        // unique_ptr<Brain> 멤버 → 완전한 정의 필요(소멸 시점)

enum class Faction : uint8_t { Player, Hostile, Neutral };   // ★ 런타임 변경 가능(전향)

class Unit {
public:
    uint64_t id        = 0;
    uint64_t ownerId   = 0;                    // 지휘하는 플레이어 id (cmdGauge 연결)
    Faction  faction   = Faction::Neutral;
    Vec3     pos;
    float    hp        = 100.f;
    float    moveSpeed = 300.f;                // units/sec

    std::unique_ptr<Brain> brain;              // null=외부(사람)가 명령 / 있으면 그 두뇌가 IssueCommand

    // 수행 상태
    float               execGauge   = 0.f;
    Command             current;
    bool                executing   = false;
    std::deque<Command> reserveQueue;
    int                 curWaypoint = 0;
};
// (virtual 소멸자 불필요 — Unit은 더 이상 다형 베이스가 아님. 다형성은 Brain으로 이동.)

// 지휘관 명령 게이지(플레이어 단위 자원). 유닛 아님 — Sim의 per-player 전투 상태.
struct Commander {
    uint64_t id      = 0;     // playerId (메타 Player와 동일 키)
    float    cmdGauge = 0.f;
    float    cmdRate  = 1.0f; // 초당 충전
    float    cmdMax   = 3.0f; // 상한
};
```

> 메타 `Player`(재화·아이템·로스터)는 **Sim 폴더 밖**(예: `Meta/Player.h`)에 둔다 — 전투 Sim 순수성 유지. P0 전투엔 당장 불필요, 골격만.
> ⚠️ 옛 `Character.h`/`Enemy.h`/`Enemy.cpp`는 **삭제**(Unit으로 흡수). 아래 CombatSim 코드블록의 `AddCharacter/AddEnemy`/`Think` 참조도 다음 단계에서 `AddUnit`/`brain->Decide`로 갱신 예정.

```cpp
// Sim/CombatSim.h
#include <unordered_map>
#include <memory>
#include <functional>

enum class CommandResult { Accepted, Queued, Rejected, Cancelled };

class CombatSim {
public:
    Character& AddCharacter(uint64_t id, uint64_t ownerId, Vec3 pos);  // 플레이어 진영 유닛
    Enemy&     AddEnemy(uint64_t id, Vec3 pos);                         // AI 유닛
    Commander& AddCommander(uint64_t playerId);                        // per-player 게이지
    Unit*      GetUnit(uint64_t id);                                   // 없으면 nullptr

    // 명령 발행: reserve=false면 즉시 대체(인터럽트), true면 예약 큐 적재
    CommandResult IssueCommand(uint64_t unitId, Command cmd, bool reserve);
    CommandResult CancelCommand(uint64_t unitId, uint32_t slotId); // 예약만 취소 가능
    void          CancelAllReserved(uint64_t unitId);

    void Tick(float dt);   // 핵심 루프

    // UE 레이어가 구독: 명령 완료/결과 이벤트
    std::function<void(uint64_t unitId, uint32_t slotId)> OnCommandComplete;

    const std::unordered_map<uint64_t, std::unique_ptr<Unit>>& Units() const { return _units; }

private:
    uint32_t NewSlot() { return ++_slotGen; }

    // 다형 컨테이너: Character/Enemy를 베이스 포인터로 한 곳에 보관.
    std::unordered_map<uint64_t, std::unique_ptr<Unit>> _units;
    // per-player 지휘관 게이지 (유닛과 별개)
    std::unordered_map<uint64_t, Commander> _commanders;
    uint32_t _slotGen = 0;

    static constexpr float ISSUE_COST   = 1.0f;  // 명령 1회 발행 비용
    static constexpr float EXEC_RATE    = 1.0f;  // 수행 게이지 충전/초
    static constexpr float EXEC_THRESH  = 1.0f;  // 공격/스킬 발동 임계
};
```

### Tick 의사코드 (CombatSim.cpp)
```
Tick(dt):                                  // 컨테이너는 unordered_map<id, unique_ptr<Unit>>
  // 1) 지휘관 게이지 충전 — Commander(플레이어 단위)
  for (auto& [pid, c] : commanders):
    c.cmdGauge = min(c.cmdMax, c.cmdGauge + c.cmdRate*dt)

  // 2) AI 사고 — Enemy만 실제 반응(베이스 Think는 no-op). 여기서 Enemy가 IssueCommand 호출 가능.
  for (auto& [id, up] : units): up->Think(*this, dt)

  // 3) 명령 수행
  for (auto& [id, up] : units):
    Unit& u = *up;
    if (!u.executing) continue;
    u.execGauge += EXEC_RATE * dt;

    bool done = false;
    switch (u.current.type):
      case Move:
        Vec3 target = u.current.waypoints[u.curWaypoint];
        Vec3 to = target - u.pos;
        float step = u.moveSpeed * dt;
        if (to.Length() <= step) {           // 웨이포인트 도달
            u.pos = target; u.curWaypoint++;
            if (u.curWaypoint >= u.current.waypoints.size()) done = true;  // 경로 끝
        } else {
            u.pos = u.pos + to.Normalized() * step;
        }
        break;
      case Attack: case Skill:
        if (u.execGauge >= EXEC_THRESH) { /* 데미지 적용 등 */ done = true; }
        break;

    if (done):
        if (OnCommandComplete) OnCommandComplete(u.id, u.current.slotId);
        if (!u.reserveQueue.empty()) {       // 예약 승격
            u.current = u.reserveQueue.front(); u.reserveQueue.pop_front();
            u.execGauge = 0; u.curWaypoint = 0; u.executing = true;
        } else {
            u.executing = false;             // Idle
        }
```

> 주의: 매 틱 dynamic_cast가 부담되면 commander id를 별도 vector로 들거나, Unit에 `virtual void UpdateGauges(float dt){}`를 두고 Character가 override하는 방식으로 바꿀 수 있음. 프로토타입 단계는 dynamic_cast로 충분.

### IssueCommand 의사코드
```
IssueCommand(unitId, cmd, reserve):
  Unit* u = GetUnit(unitId); if (!u) return Rejected;
  // 이 유닛을 지휘하는 플레이어의 Commander 게이지 검사
  auto it = commanders.find(u->ownerId);
  if (it == commanders.end() || it->second.cmdGauge < ISSUE_COST) return Rejected;
  it->second.cmdGauge -= ISSUE_COST;
  cmd.slotId = NewSlot();
  if (reserve):
    u->reserveQueue.push_back(cmd); return Queued;
  else:
    u->current = cmd; u->executing = true; u->execGauge = 0; u->curWaypoint = 0;
    // 정책(확정): 예약 큐는 유지(리셋 안 함)
    return Accepted;

CancelCommand(unitId, slotId):
  // reserveQueue에서 slotId 찾으면 제거 → Cancelled
  // current로 이미 승격됐으면 → Rejected (수행 중 취소 불가)
```

---

## 3. UE5 레이어 — 와이어링 가이드

- **Pawn/Character**: 지휘관 아바타 1개(P0). `Tick`에서 `Sim.Tick(DeltaTime)` 호출 후, `Sim.Units()`의 pos를 읽어 `SetActorLocation`(부드럽게 보간 권장). Vec3↔FVector 변환은 UE 레이어에서만.
- **PlayerController**: 지면 클릭 → 히트 위치를 웨이포인트로 하는 `Command{type=Move, waypoints=[hit]}` 생성 → `Sim.IssueCommand(unitId, cmd, reserve=false)`. Shift+클릭이면 reserve=true(예약).
- **UI(UMG)**: cmdGauge / execGauge 진행바, 예약 큐 슬롯 표시(취소 버튼 → CancelCommand).
- **변환 규약**: 게임은 cm 단위(UE 기본). Sim의 moveSpeed=300 = 3 m/s. Vec3는 UE 좌표 그대로(x,y,z) 매핑.

---

## 4. P0 작업 순서 (마일스톤, 직접 타이핑)

1. **Sim 골격**: Vec3, Command, Unit, Commander, CombatSim — `Tick`과 `IssueCommand(Move)`까지. (UE 없이 콘솔 테스트로 이동 로그 찍어 검증 가능)
2. **UE 이동**: 지휘관 폰 + 클릭→Move 명령 + Tick 구동 + 액터가 pos 따라감. (서버 권위 스타일이지만 로컬)
3. **인터럽트/예약**: 이동 중 새 클릭 = 즉시 대체, Shift+클릭 = 예약 큐 적재, 취소. UI 게이지.
4. **더미 적 + 공격**: Attack 명령, execGauge 임계 발동, HP/사망.
5. → **P1**: 탐험 이동 + 심볼 인카운트(거리 진입 감지) → 전투 전이 → **용병 등장(스폰)**. (용병은 Unit 추가 + 지휘관 unitIds)

### 검증 기준(재미 체크)
- 명령 인터럽트의 즉각 반응성(RTS 우클릭 느낌)이 좋은가?
- 2계층 게이지(얼마나 자주 지시 × 얼마나 빨리 수행)가 튜닝 레버로 체감되는가?
- 예약 큐가 직관적인가, 아니면 거추장스러운가? → 여기서 설계 미결정(예약 vs 대체)을 실제로 결정.

---

## 5. 서버 작업 현황 (P2에서 회수, 지금 보류)

이미 해둔 것 — **버리지 말 것**:
- `MMO/Server`(IOCP.git)에 명령 패킷 proto 추가 완료: `C_ISSUE_COMMAND / C_CANCEL_COMMAND / C_CANCEL_ALL_RESERVED / S_COMMAND_RESULT / S_COMMAND_COMPLETE / S_SNAPSHOT` + `CommandInfo`/`UnitSnapshot` 구조 + `CommandType`/`CommandResult` enum. (PKT id 1012~1017)
- 서버 클래스 구조 정리: 수행 상태(execGauge/current/reserveQueue/curWaypoint)를 `Creature`로, cmdGauge를 `Player(=지휘관)`로 분리.
- `.gitignore`에 `.vs/`·빌드산출물 추가하여 푸시 정상화.

P2 이식 시: **Sim 코어의 Tick/IssueCommand 로직을 서버 `Room::UpdateTick` + 핸들러에 그대로 이식**. proto의 `CommandInfo` ↔ Sim의 `Command` 변환 어댑터만 작성. Sim의 `Vec3`가 서버 설계 §8의 "경량 Vector"에 해당.

---

## 6. 핵심 미결정(설계 v1 §9 중 P0에서 결정할 것)
- [ ] 기본(대체) 명령 시 예약 큐 유지 vs 리셋 → **현재 잠정: 유지**. P0 3단계에서 체감 후 확정.
- [ ] 예약 큐 UX가 가치 있는가 (없애고 즉시 명령만으로 충분할 수도).
- [ ] 게이지 수치(cmdRate/cmdMax/execRate/threshold/moveSpeed) 1차 튜닝값.
- [ ] 인카운트 감지 반경/추격 규칙(P1).
