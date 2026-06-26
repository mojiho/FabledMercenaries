# Fabled Mercenaries — 서버 기술 설계 & 구현 로드맵 (v2)

> v1 설계안(`Desktop/fabled_mercenaries_combat_network_design.md`)을 **현재 IOCP 서버 코드 위에 어떻게 올릴지**로 구체화한 구현용 문서.
> 대상 코드: `MMO/Server/` (Server.sln). 클라이언트: `MMO/S1/` (UE5).

---

## 0. 확정된 결정 (2026-06-23)

| 항목 | 결정 | 비고 |
|------|------|------|
| 서버 틱레이트 | **20Hz 기본, 상수로 튜닝 가능** | `TICK_RATE` 한 곳에서 조정. 추후 30Hz 검토 |
| 위치 동기화 전송 | **TCP 단독 시작** | 기존 IOCP 신뢰성 채널로 스냅샷도 전송. UDP는 후순위 분리 |
| 예약 큐 정책 | **기본(대체) 명령 시 예약 큐 유지** | 별도 "예약 전체 취소" 명령 제공 |
| 이동 웨이포인트 | **A형: 이동 1개 = 경로 리스트 보유** | 취소 단위 = 명령 단위 |

미결정(설계 진행에는 영향 없음, 추후): 난입 협동/적대, 적 리스폰 정책, AOI 섹터 크기, UDP 도입 시점.

---

## 1. 현재 베이스라인 요약

- `ServerCore`: IOCP 네트워킹 완성 — 재사용.
- `GameServer`: 단일 `GRoom`, Enter/Leave/Move/Spawn/Despawn. `C_MOVE`는 **클라 권위**(좌표 복사+재전송). `UpdateTick`은 10Hz `cout` 하트비트뿐, 시뮬 없음.
- 객체 모델: `Object`(objectInfo+posInfo) → `Creature`(빈 스텁) → `Player`(weak GameSession). 1플레이어=1오브젝트.
- 실제 proto 원본: `Server/Common/protoc-21.12-win64/bin/*.proto`. (GameServer 폴더 .proto는 stale → 동기화 대상)

---

## 2. 핵심 전환: 클라 권위 → 서버 권위 틱

현재 흐름:
```
C_MOVE 수신 → player.pos = pkt.pos → 전체 브로드캐스트   (서버는 검증 안 함)
```

목표 흐름:
```
C_ISSUE_COMMAND(이동, 웨이포인트들) 수신 → 명령 검증 → unit.current 에 적재
매 틱(50ms):
  cmdGauge/execGauge 진행 → current(이동)이면 웨이포인트 따라 pos 갱신
  → 완료 판정 → 큐 pop → AOI 스냅샷 브로드캐스트
```
클라는 결과를 **보간**만 한다(권위는 서버).

---

## 3. 상태 구조 (서버 권위)

```cpp
// 명령 1건
struct Command {
    uint32            slotId;       // 취소/완료 식별 키 (서버 발급)
    uint16            cmdType;      // Move / Attack / Skill ...
    std::vector<FVector> waypoints; // 이동: 경로 리스트(A형). 그 외: 단일/빈
    uint64            targetId = 0; // 공격/스킬 대상
    uint32            skillId  = 0;
    uint32            issueSeq;     // 클라 발행 순번 (중복/순서 방지)
};

// 용병(유닛) 단위 — 기존 Creature 확장 또는 신규 Unit
struct UnitState {
    uint64               unitId;
    uint64               ownerId;     // 지휘관(플레이어) id
    FVector              pos;
    float                execGauge;   // 수행 게이지
    Command              current;
    bool                 executing;
    std::deque<Command>  queue;       // '다음 행동 예약'(다중, 취소가능)
    int                  curWaypoint; // 이동 진행 인덱스
};

// 지휘관(플레이어) 단위
struct CommanderState {
    uint64 ownerId;
    float  cmdGauge;                  // 명령 발행 게이지 (rate로 충전, 발행 시 소비)
    std::vector<uint64> ownedUnits;   // 보유 용병 unitId 들
};
```

> **클래스 구조(확정)**: `Object → Creature → {Player(=지휘관), Mercenary, Monster}`.
> - 수행 상태(execGauge/current/reserveQueue/curWaypoint)는 필드 액터 공통 → **Creature**.
> - 명령 발행(cmdGauge) + 용병 로스터 → **Player**.
>
> **필드 라이프사이클(확정)**: 탐험맵엔 **지휘관 아바타(Player)만** 필드에 등장·이동. **전투 진입 시 보유 용병(Mercenary)들이 Room에 스폰**, 종료 시 디스폰. 용병 로스터는 Player가 항상 소유(데이터)하되 Room에 들어오는 건 전투 중에만.
> - M1: 지휘관 아바타의 서버 권위 이동만 구현(용병 미등장). Mercenary 클래스/스폰은 전투(M2)에서.

---

## 4. 틱 루프 (Room::UpdateTick 교체)

```
const float dt = 1.0f / TICK_RATE;   // 20Hz → 0.05
매 틱, 활성 유닛에 대해:
  1. commander.cmdGauge = min(max, cmdGauge + cmdRate*dt)
     unit.executing 이면 unit.execGauge += execRate*dt
  2. current.cmdType == Move 이면:
        목표 웨이포인트로 pos 이동(speed*dt)
        도달하면 curWaypoint++ → 마지막이면 완료
     (Attack/Skill 은 execGauge가 임계 도달 시 1회 판정 → 완료)
  3. 완료 시: S_COMMAND_COMPLETE 발행
        queue 비어있지 않으면 pop_front → current 승격, execGauge=0
        비어있으면 executing=false (Idle)
  4. (후속) AI Perception 인카운트 감지 → 전투 전이
  5. AOI 필터 스냅샷을 주변 클라에 브로드캐스트 (1단계: 전체 브로드캐스트)
```

`DoTimer(1000/TICK_RATE, &Room::UpdateTick)` 로 기존 JobTimer 재사용.

---

## 5. 명령 발행/취소 분기

```cpp
void OnIssue(UnitState& u, CommanderState& c, const Command& cmd, bool reserve) {
    // 게이팅: cmdGauge 부족하면 Rejected
    if (c.cmdGauge < ISSUE_COST) { reply(Rejected); return; }
    c.cmdGauge -= ISSUE_COST;

    if (reserve) {
        cmd.slotId = NewSlot();
        u.queue.push_back(cmd);          // 명시적 예약만 큐 적재
        reply(Queued, cmd.slotId);
    } else {
        cmd.slotId = NewSlot();
        u.current = cmd; u.executing = true; u.execGauge = 0;
        // 결정: 예약 큐 유지 (리셋하지 않음)
        reply(Accepted, cmd.slotId);
    }
}

void OnCancel(UnitState& u, uint32 slotId) {
    // 큐에 있으면 제거 → Cancelled
    // 이미 current 로 승격됐으면 → Rejected ("수행 중 취소 불가")
    // 취소는 서버 권위로만 확정, 클라는 응답 전 "취소 중" 표시
}
```

별도 `C_CANCEL_ALL_RESERVED`(예약 전체 취소) 제공.

---

## 6. 패킷 프로토콜 추가 (proto)

`Struct.proto`:
```proto
message CommandInfo {
    uint32 slot_id    = 1;
    uint32 cmd_type   = 2;   // enum CommandType
    repeated PosInfo waypoints = 3;
    uint64 target_id  = 4;
    uint32 skill_id   = 5;
    uint32 issue_seq  = 6;
}
```
`Enum.proto`: `CommandType { NONE, MOVE, ATTACK, SKILL }`, `CommandResult { ACCEPTED, QUEUED, REJECTED, CANCELLED }`.

`Protocol.proto`:
```proto
message C_ISSUE_COMMAND   { uint64 unit_id = 1; CommandInfo cmd = 2; bool reserve = 3; }
message C_CANCEL_COMMAND  { uint64 unit_id = 1; uint32 slot_id = 2; }
message C_CANCEL_ALL_RESERVED { uint64 unit_id = 1; }
message S_COMMAND_RESULT  { uint64 unit_id = 1; uint32 slot_id = 2; CommandResult result = 3; }
message S_COMMAND_COMPLETE{ uint64 unit_id = 1; uint32 slot_id = 2; uint32 cmd_type = 3; }
message UnitSnapshot      { uint64 unit_id = 1; PosInfo pos = 2; float exec_gauge = 3; uint32 cur_cmd = 4; }
message S_SNAPSHOT        { repeated UnitSnapshot units = 1; }   // AOI 필터 적용
```

> proto 변경 후 protoc로 .pb.h/.cc 재생성 + GameServer/.proto 와 bin/.proto **동기화 필수**(현재 불일치).

---

## 7. 구현 로드맵 (마일스톤)

### M1 — 수직 슬라이스: 서버 권위 이동 (현재 목표)
1. proto 동기화 + 위 패킷 추가, protoc 재생성, 빌드 통과.
2. `UnitState`/`CommanderState` 도입 (플레이어당 용병 1기).
3. `Room::UpdateTick` → 고정 dt 틱 루프 (게이지 진행 + 이동 웨이포인트 추종 + 완료/큐).
4. `C_ISSUE_COMMAND(MOVE)` / `C_CANCEL_COMMAND` 핸들러, `S_COMMAND_RESULT/COMPLETE` + `S_SNAPSHOT` 전체 브로드캐스트.
5. DummyClient로 발행→이동→완료 검증.

### M2 — 전투 기본
- 용병 N기, `ATTACK` 명령, execGauge 임계 판정, HP/사망, `S_COMMAND_COMPLETE` 결과.

### M3 — 몬스터 & 인카운트
- `Monster` AI(시야/어그로), 심볼 인카운트 → 전투 전이.

### M4 — AOI / 멀티
- 섹터 그리드, 주변 엔티티만 스냅샷, 전투 난입 합류.

### M5 — 스킬/강림 전직, UDP 분리 검토.

---

## 8. 리스크 / 주의

- **proto 불일치**: bin/.proto(실사용) vs GameServer/.proto(stale). 먼저 일원화하고 빌드 스크립트로 자동 생성.
- **JobQueue 동시성**: 모든 룸 상태 변경은 `Room`의 Job으로 직렬화(`DoAsync`). 틱 루프와 핸들러가 같은 큐에서 돌게 유지.
- **FVector 의존**: 서버는 UE5 비의존이어야 함 → 서버용 경량 Vector(x,y,z) 자체 정의 권장(설계안의 `FVector`는 클라 기준).
- **틱 누적 오차**: dt 고정 + `LEndTickCount` 기반. 프레임 드랍 시 catch-up 정책 후속 결정.
