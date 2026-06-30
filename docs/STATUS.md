# Fabled Mercenaries — 진행 현황 & 다음 할 일 (STATUS)

> 단일 "지금 어디까지 했고 다음에 뭐 할지" 참조. 최종 갱신: **2026-06-30**.
> 관련 문서: 전투/클래스 설계 `combat_class_design.md`, P0 프로토타입 설계 `prototype_phase_p0_design.md`, 서버 설계 `server_technical_design_v2.md`, **아트 파이프라인 `art_pipeline.md`**.
>
> 제약 메모: 1인 **프로그래머** — 코드/개발 시간 충분, **병목은 아트(특히 3D 모델링)**. 전략은 `art_pipeline.md` 참조(베이스 에셋+Mixamo+툰 셰이더+모듈러로 우회).

---

## 0. 한 줄 요약
P0 싱글 프로토타입의 **엔진 비의존 전투 Sim 코어**를 콘솔(`SimTesst`)에서 구축·검증 완료. **팀 시너지(탱+딜+힐 4 vs 적 3)로 승리 재현 = "조합이 전부" 설계 검증됨.** **다음: UE 엔진 연동(와이어링)** — 집 PC(UE 가능)에서 진행. (최신 Sim 12파일은 `Client/Source/FabledMercenaries/Sim/`에 동기화 완료)

---

## 1. ✅ 오늘까지 완료 (Sim 코어, 콘솔 검증됨)

전부 순수 C++ (`UWorld`/`AActor` 비의존), `MMO/SimTesst/SimTesst/`에서 작업·검증.

- **기반**: `Vec3`(연산자+Length/Normalized), `Command`(slotId/type/waypoints/targetId/skillId), 결정적 난수 `Rand01`(xorshift).
- **유닛 모델**: `Unit`(단일 타입) — pos/hp/alive/moveSpeed/faction/facing/mp + 수행상태(execGauge/current/executing/reserveQueue/curWaypoint) + `vector<Skill> skills` + ranged/attackRange/attackPreDelay/attackPostDelay/attackFired. **다형성은 `Brain`(컴포넌트)로**, faction/조종주체는 데이터(런타임 변경 가능 = 전향).
- **CombatSim**: `unordered_map<id, Unit>` + `unordered_map<id, Commander>`. `Tick(dt)`, `AddUnit(...,Class,...)`, `AddCommander`, `GetUnit`, `FindNearestHostile`, `IssueCommand/Cancel/CancelAllReserved`, `OnCommandComplete` 콜백.
- **명령 시스템**: 이동(웨이포인트), 즉시 대체(인터럽트), 명시적 예약 큐(다중·취소), 자동 승격. 2계층 게이지(지휘관 cmdGauge × 유닛 수행). ✔ 검증.
- **클래스/스탯**: `Class`(전사/탱커/마법사/궁수/암살자/**힐러**) + `GetClassStats`(이속·사거리·ranged·선딜·후딜·**attackDamage·maxHp**) + `GetClassSkills`. ✔ **클래스별 HP**(탱커250/마법사·힐러70~80 물몸) — HP 하나로 3v3 0:3 패배 → 승리로 역전 검증.
- **방어 패시브 + 방어 태세**: `Skill`(통합: category Passive/Active + type). **Passive Defense** = 피격 시 확률 롤(패링/방패/회피, 쿨다운). **Active Defense(방어 태세, `MakeDefenseStance`)** = 토글(`Unit.defendStance`) — 차단율↑·공격 둔화(`attackSlow`)·이동 불가. 수치는 스킬 데이터에서 읽음. ✔
- **힐러/Heal**: `SkillType::Heal`(`MakeHeal`) — 아군 HP 회복(최대치 상한). `HealerBrain`이 가장 다친 아군 회복, MP 부족 시 Focus. ✔
- **투사체 시스템**: `Projectile`(호밍/포물선 arc). **몸빵(interception)** = 경로상 가장 가까운 적 진영 유닛이 맞고 아군은 관통 → 탱커가 후방 향한 화살 차단. 포물선 z(시각). ✔
- **후면 크리(universal)**: 피격 방향이 `target.facing` 뒤면 **방어 무시 + 크리 2배**. idle 유닛은 피격 후 공격자 쪽으로 돌아봄(연속 백스탭 방지). ✔
- **거리/사거리**: 근접/원거리 사거리, 사거리 밖이면 발동 보류. 원거리 "조준"=긴 선딜, 선딜 중 피격 시 윈드업 리셋(조준 풀림). ✔
- **공격 타이밍**: 선딜→발사(1회)→후딜 사이클. 방어 태세 시 선/후딜 ×`attackSlow`. ✔
- **AI Brain**: `Brain` 추상 + `ChaseAttackBrain`(사거리밖+돌진쿨OK+MP충분 → Charge, 아니면 이동) / `KiteBrain`(거리 유지 사격) / `MageBrain`(MagicBolt, MP낮으면 Focus) / **`HealerBrain`** / **`TankBrain`**(후방 마/궁/힐 있으면 그 앞을 방어 태세로 지킴, 없으면 돌격). 양측 자동 교전. ✔
- **액티브 스킬**: `MakeCharge`(돌진+짧은 스턴 `CHARGE_STUN`, 카이팅 카운터), `MakeMagicBolt`(원거리 마법탄), MP 게이팅, 선딜→효과→후딜. ✔ 검증.
- **MP/정신집중(Focus)**: Focus로 MP 충전(제자리·무방비), 스킬이 MP 소모. ✔

---

## 2. 📁 코드 위치 & ⚠️ 동기화 경고

- **`MMO/SimTesst/SimTesst/`** — 콘솔 테스트 프로젝트. **현재 Sim 코어의 정본**(여기서 작업·검증). 별도 솔루션.
- **`MMO/Client/`** — UE5 프로젝트(`FabledMercenaries`, git: `mojiho/FabledMercenaries.git`). `Source/FabledMercenaries/Sim/`에 **오래된 Sim 사본**(SimTesst와 갈라짐).
- ⚠️ **두 사본이 어긋나 있음.** 집에서 UE 작업하려면 **SimTesst의 최신 Sim 파일을 UE 프로젝트로 복사·동기화**해야 함. (`docs/`도 git 밖이라 같이 옮겨야 집에서 보임.)

**파일 목록(SimTesst → UE로 복사 대상, 12개)**: `Vec3.h, Command.h, Skill.h, Brain.h, Class.h, Unit.h, Commander.h, Projectile.h, CombatSim.h, CombatSim.cpp, AIBrain.h, AIBrain.cpp`. (`main.cpp`는 콘솔 테스트용이라 제외) — **2026-06-30 동기화 완료.**

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

- **탱커 생존력 보강(선택)** — 방어 태세는 정면 한정 85% 차단이라 **협공(암살자 측면 백스탭)에 우회**됨. "방어 중 가장 가까운 위협을 바라보기" / 협공 시 블록 보정 등으로 더 단단하게(현재도 시간 벌어 팀 승리엔 충분).
- **충돌/접촉 규칙**(combat_class_design.md §7) — 전사/탱커 밀치고 전진, 방어 태세 탱커 충돌 시 반격뎀+스턴→명령 해제, 암살자 백스탭→모든 명령 취소·정지.
- **상태이상 등급 체계** — 짧은 스턴=명령 해제 / 상급=명령 취소. (현재 `stunRemaining` 기본형만 있음)
- **지휘관 유형** — 전투형/지휘형 분리.
- **용병 코스트 소프트캡** — 코스트 초과 고용 시 명령 불복종 확률 상승.
- **전직 트리 / 강림(메타)** — 전사→양손검사/도끼전사 분기, 2~3차 전직 후 강림(전설 용병=영혼이 그릇에 빙의).
- 기타 액티브 스킬·투사체 V2(이동 회피, 포물선 차단 규칙).

---

## 5. ❓ 미해결 설계 결정 (combat_class_design.md §8)
교전 결과 모델 세부, 클래스별 수치(방어 확률·크리 배율·선후딜 등), 백어택 각도·방어무시, MP 수치, Defend 효과, 전직 트리/강림 상세, 지휘관 유형 수치, 불복종 확률 곡선, 충돌 판정 방식 — **P0 재미검증하며 튜닝**.
