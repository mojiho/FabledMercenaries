# Fabled Mercenaries — 진행 현황 & 다음 할 일 (STATUS)

> 단일 "지금 어디까지 했고 다음에 뭐 할지" 참조. 최종 갱신: **2026-06-26**.
> 관련 문서: 전투/클래스 설계 `combat_class_design.md`, P0 프로토타입 설계 `prototype_phase_p0_design.md`, 서버 설계 `server_technical_design_v2.md`.

---

## 0. 한 줄 요약
P0 싱글 프로토타입의 **엔진 비의존 전투 Sim 코어**를 콘솔(`SimTesst`)에서 구축·검증 완료. **다음: UE 엔진 연동(와이어링)** — 집 PC(UE 가능)에서 진행.

---

## 1. ✅ 오늘까지 완료 (Sim 코어, 콘솔 검증됨)

전부 순수 C++ (`UWorld`/`AActor` 비의존), `MMO/SimTesst/SimTesst/`에서 작업·검증.

- **기반**: `Vec3`(연산자+Length/Normalized), `Command`(slotId/type/waypoints/targetId/skillId), 결정적 난수 `Rand01`(xorshift).
- **유닛 모델**: `Unit`(단일 타입) — pos/hp/alive/moveSpeed/faction/facing/mp + 수행상태(execGauge/current/executing/reserveQueue/curWaypoint) + `vector<Skill> skills` + ranged/attackRange/attackPreDelay/attackPostDelay/attackFired. **다형성은 `Brain`(컴포넌트)로**, faction/조종주체는 데이터(런타임 변경 가능 = 전향).
- **CombatSim**: `unordered_map<id, Unit>` + `unordered_map<id, Commander>`. `Tick(dt)`, `AddUnit(...,Class,...)`, `AddCommander`, `GetUnit`, `FindNearestHostile`, `IssueCommand/Cancel/CancelAllReserved`, `OnCommandComplete` 콜백.
- **명령 시스템**: 이동(웨이포인트), 즉시 대체(인터럽트), 명시적 예약 큐(다중·취소), 자동 승격. 2계층 게이지(지휘관 cmdGauge × 유닛 수행). ✔ 검증.
- **클래스/스탯**: `Class`(전사/탱커/마법사/궁수/암살자) + `GetClassStats`(이속·사거리·ranged·선딜·후딜) + `GetClassSkills`. ✔
- **방어 패시브**: `Skill`(통합: category Passive/Active + type) — 패링/방패/회피(확률+쿨다운). 피격 시 롤로 무효. ✔ (전사>탱커 상성 재현)
- **거리/사거리**: 근접/원거리 사거리, 사거리 밖이면 발동 보류. `ChaseAttackBrain`(멀면 이동/가까우면 공격), `KiteBrain`(거리 유지하며 쏘기). ✔
- **공격 타이밍**: 선딜→발사(1회)→후딜 사이클. 원거리 "조준"=긴 선딜, 선딜 중 피격 시 윈드업 리셋(조준 풀림). ✔
- **AI Brain**: `Brain` 추상 + `ChaseAttackBrain`/`KiteBrain`. 양측 자동 교전. ✔
- **액티브 스킬 + 돌진(Charge)**: `Skill.category::Active`, `MakeCharge`, MP 게이팅, 선딜→효과(대상 근접까지 순간이동)→후딜. ✔ 검증.

---

## 2. 📁 코드 위치 & ⚠️ 동기화 경고

- **`MMO/SimTesst/SimTesst/`** — 콘솔 테스트 프로젝트. **현재 Sim 코어의 정본**(여기서 작업·검증). 별도 솔루션.
- **`MMO/Client/`** — UE5 프로젝트(`FabledMercenaries`, git: `mojiho/FabledMercenaries.git`). `Source/FabledMercenaries/Sim/`에 **오래된 Sim 사본**(SimTesst와 갈라짐).
- ⚠️ **두 사본이 어긋나 있음.** 집에서 UE 작업하려면 **SimTesst의 최신 Sim 파일을 UE 프로젝트로 복사·동기화**해야 함. (`docs/`도 git 밖이라 같이 옮겨야 집에서 보임.)

**파일 목록(SimTesst → UE로 복사 대상)**: `Vec3.h, Command.h, Skill.h, Brain.h, Class.h, Unit.h, Commander.h, CombatSim.h, CombatSim.cpp, AIBrain.h, AIBrain.cpp`. (`main.cpp`는 콘솔 테스트용이라 제외)

---

## 3. 🎯 다음 할 일 — 집(UE 가능 PC)에서

### STEP 0 (필수 선행): Sim → UE 동기화
1. SimTesst의 Sim `.h/.cpp` 11개를 `Client/Source/FabledMercenaries/Sim/`로 **복사(덮어쓰기)**.
2. `docs/`도 `Client/docs/`로 복사(집에서 설계 보려면).
3. UE 프로젝트 파일 재생성 → 컴파일 확인(엔진 비의존이라 통과해야 함).
   - 주의: 예전에 UE 사본에서 "`Class` 미정의" 났던 건 사본이 stale했기 때문 — 최신본으로 덮으면 해결.

### STEP 1: UE 와이어링 (P0 마일스톤 2 — 핵심 목표)
> 목표: "Sim이 굴리는 유닛이 UE 화면에서 움직이는 것"부터.
```
[구동] 매니저 AActor가 CombatSim 1개 소유 → Tick에서 sim.Tick(DeltaTime)
[표현] 유닛마다 AActor → 매 틱 Sim의 Unit.pos(Vec3) → FVector로 SetActorLocation (보간 권장)
[입력] PlayerController 좌클릭 → 지면 라인트레이스 → 월드좌표 → Command(Move) → sim.IssueCommand
        Shift+클릭=예약(reserve), 우클릭/단축키=Attack 등
[연결] OnCommandComplete 구독 → 애님/이펙트 (나중)
```
- **변환**: Sim `Vec3{x,y,z}` ↔ UE `FVector(x,y,z)` 1:1 (cm 단위, moveSpeed 300 = 3m/s).
- 이미 만든 카메라(`Core/FM_CameraPawn` + 컨트롤러 WASD/줌/우클릭 패닝)에 입력 얹기.
- 최소 검증: 지휘관 아바타 1기가 클릭한 곳으로 서버권위식 이동.

### STEP 2~: 전투 시각화
- 유닛 스폰/디스폰, HP바, 게이지 UI, 공격/방어/돌진 애님 트리거(OnCommandComplete 등).

---

## 4. 🔧 남은 Sim 전투 기능 (UE 연동과 병행/이후, SimTesst 또는 UE에서)

- **B-2: AI가 돌진 사용** — `ChaseAttackBrain`이 "사거리 밖 + 돌진 쿨 OK + MP 충분"이면 Charge → 카이팅 카운터 완성. (오늘 돌진은 수동 발동까지만 검증)
- **클래스별 기본 데미지** — 지금 전역 `ATTACK_DAMAGE=25`. `ClassStats`에 `attackDamage` 추가 → 마법사 평타 약하게.
- **후면 크리(universal)** — `dot((attacker.pos-target.pos).Normalized(), target.facing) < 0`이면 무조건 크리. 암살자 극단. (방어 무시 여부 결정 필요)
- **충돌/접촉 규칙**(combat_class_design.md §7) — 전사/탱커 밀치고 전진, 방어모드 탱커 충돌 시 반격뎀+0.5초 스턴→명령 해제, 암살자 백스탭→모든 명령 취소·정지.
- **상태이상 등급 체계** — 0.5초 스턴=명령 해제 / 상급=명령 취소.
- **MP/정신집중(Focus)** — MP 회복 수단. 지금은 테스트로 mp=mpMax 세팅.
- **마법사 스킬**(원거리 화력), 기타 액티브 스킬.

---

## 5. ❓ 미해결 설계 결정 (combat_class_design.md §8)
교전 결과 모델 세부, 클래스별 수치(방어 확률·크리 배율·선후딜 등), 백어택 각도·방어무시, MP 수치, Defend 효과, 전직 트리/강림 상세, 지휘관 유형 수치, 불복종 확률 곡선, 충돌 판정 방식 — **P0 재미검증하며 튜닝**.
