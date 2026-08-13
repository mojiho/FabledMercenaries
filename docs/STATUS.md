# Fabled Mercenaries — 진행 현황 & 다음 할 일 (STATUS)

> 단일 "지금 어디까지 했고 다음에 뭐 할지" 참조. 최종 갱신: **2026-08-11**.
> 관련 문서: 전투/클래스 설계 `combat_class_design.md`, P0 프로토타입 설계 `prototype_phase_p0_design.md`, 서버 설계 `server_technical_design_v2.md`, **아트 파이프라인 `art_pipeline.md`**.
>
> 제약 메모: 1인 **프로그래머** — 코드/개발 시간 충분, **병목은 아트(특히 3D 모델링)**. 전략은 `art_pipeline.md` 참조(베이스 에셋+Mixamo+툰 셰이더+모듈러로 우회).

---

## 0. 한 줄 요약
P0 싱글 프로토타입의 **엔진 비의존 전투 Sim 코어** 검증 완료 + **UE 클라이언트 와이어링(STEP 1) 완료.** Sim이 UE 화면에서 굴러가고, 유닛 선택·이동·예약경유지·도착방향·지형추적·원형 커맨드 UI·카메라(줌/궤도/WASD/QE)까지 동작. **현재 작업: 전투(적 스폰 완료 → 클릭 공격 붙이는 중).**

---

## 0.5 ✅ UE 클라이언트 진행 (2026-07 세션)

> 위치: `D:\MMO\Client` (한글 경로가 UE 빌드를 깨서 **`D:\MMO`로 이전**). 빌드는 **VS "빌드" 금지**(MSB4018) → **`Build.bat`(UBT)** 또는 Rider.

**핵심 아키텍처 (UE 측):**
- **`AFMSimManager`**(AActor) — `CombatSim Sim` 1개 소유. `BeginPlay`에서 지휘관+아군3(원점)·적3(x=600, `Faction::Hostile`) 스폰. `Tick`에서 `Sim.Tick()` + 유닛을 **DrawDebugSphere**로 렌더(진영/클래스별 색, 선택=흰색·확대, 바라보는 방향 노란 화살표). 유닛은 Sim이 위치 권위, 액터는 "멍청한" 표시자.
- **`AFabledMercenariesPlayerController`**(`UCLASS(abstract)`, BP_TopDownController가 상속) — 입력 전부 처리.
- **`AFM_CameraPawn`**(APawn) — SpringArm 쿼터뷰 카메라. Default Pawn(캐릭터 제거함). `ZoomCamera`/`MoveCamera`(WASD)/`OrbitCamera`(우클릭 드래그)/`RotateCamera`(QE).
- **`WBP_RadialMenu`**(UMG) — 유닛 클릭 시 머리 위 원형 커맨드 링. 매 Tick `GetSelectedUnitWorldPos` → `Project World Location to Widget Position`으로 화면 좌표 투영해 따라다님. Ring 앵커=좌상단·정렬=0.5/0.5. "이동" 버튼 → 컨트롤러 `EnterMoveMode()`.

**완료된 조작 (전부 동작 확인):**
- **선택**: 좌클릭=유닛 선택(`HandleClick`→`FindUnitNear` XY거리), 우클릭=해제(`ClearSelection`, 이동모드도 취소). 링은 `HasSelectedUnit`(+`bRingHidden`)로 표시.
- **이동**: 링 이동버튼→이동모드(링 숨김·선택 유지, 커서 고스트+지형따라가는 초록 네비선). 좌클릭=이동, `MoveSelectedTo`/`MoveSelectedAlong`.
- **예약 이동**: **Ctrl+클릭**=경유지 누적(선 이어짐), Ctrl 없이=경로 실행. `PendingWaypoints` → `Command.waypoints` 다중.
- **드래그 도착방향**: 이동 확정 클릭을 **누른 채 드래그**하면 도착지 고정+방향 화살표, 떼면 그 방향. Sim `Command`에 `arriveFacing`/`hasArriveFacing` 추가, 최종 웨이포인트 도달 시 `u.facing=arriveFacing`. (예약은 **마지막 도착지만** 방향 적용)
- **지형 추적**: `GroundZAt(x,y)`(아래로 라인트레이스)로 구체·네비선을 실제 지형 높이에 얹음(경사면 대응).
- **카메라**: 휠=줌, 우클릭 드래그=궤도회전, WASD=중심점 이동(카메라 기준), Q/E=회전. (Enhanced Input: `IA_CameraMove` Axis2D + swizzle/negate, `IA_CameraRotate` Axis1D; **BP_TopDownController 디폴트에 IA 지정 필수**, Q에 Negate 있어야 Q/E 반대방향.)

**원형 커맨드 링 — 버튼 배치 (2026-08-11):**

| 버튼 | 기능 | 호출 | 상태 |
|---|---|---|---|
| Button_0 | 이동 | `EnterMoveMode()` | ✅ |
| Button_1 | 방어 | `CmdDefend()` | ✅ |
| Button_2 | 정신집중 | `CmdFocus()` | ✅ |
| Button_3 | 정지 | `CmdStop()` | ✅ |
| Button_4 | **스킬** | `WBP_SkillList` 토글 → 행 클릭 시 `ChooseSkill(SkillType)` | C++·UI 완료, **링 버튼 배선 남음** |
| Button_5 | **아이템(회복 포션)** | `CmdUseHealPotion()` | C++ 완료, **링 버튼 배선 남음** |

- **스킬 UI**: `WBP_SkillRow.SetData(InName, InCost, InSkillType)` + 버튼 클릭 → `ChooseSkill`. `WBP_SkillList.Refresh()` = SkillBox 비우기 → `GetSelectedUnitSkills()` → ForEach → 행 생성/채우기/담기. `Event Construct → Refresh` 연결됨.
- **아이템**: 대상 선택 없이 **선택 유닛 자신에게 즉시 사용**. `AFMSimManager::UseItemOnSelected(ItemId)`가 메타 재고 차감 → `CommandType::Item` 발행. `BeginPlay`에서 포션 5개 지급(테스트용). 목록 UI(`WBP_ItemList`)는 미구현 — 종류가 1개라 버튼 직결.
- 설계 근거·분리 원칙은 `prototype_phase_p0_design.md`의 메타 `Player` 절 참조.

**전투 (진행 중):**
- 적 3기(`Faction::Hostile`, id 200~, 브레인 없음=표적) 스폰·빨강 렌더 ✅.
- `FMSimManager`에 `FindEnemyNear`/`AttackTarget`(=`CommandType::Attack`+`targetId`) 헤더 추가됨 → **컨트롤러 `OnLeftReleased`에서 적 클릭 판정→공격 라우팅 연결 중.** 다음: HP바/사망 시각화, 적에 `ChaseAttackBrain` 붙여 반격.

**⚠️ 환경/함정 (하드-원 교훈):**
- **`/utf-8` 프로젝트 전역 필수** — 한글 주석 cp949 오독 → ODR 크래시(한 세션 날림). 서버/클라 vcxproj 모든 구성 AdditionalOptions.
- **UE 빌드**: `& "…UE_5.8\…\Build.bat" --% FabledMercenariesEditor Win64 Development -Project="D:\MMO\Client\FabledMercenaries.uproject" -WaitMutex`. 헤더(UFUNCTION/멤버/시그니처) 변경 시 **에디터 닫고 풀 리빌드**(Live Coding 불가). 본체만이면 Live Coding OK.
- **에디터: "컴파일"≠"저장".** BP는 자동저장 안 됨 → 닫기 전 **모두 저장**. (자동저장본은 `Saved/Autosaves/…`에서 복원 가능 — 이번에 WBP/컨트롤러 복구함.)
- **편집기: Rider** 사용(VS 2026 업데이트가 WebView2에서 깨짐). Rider는 UE5.8용 **.NET 10 런타임 필요**(없으면 프로젝트 모델 생성 실패). UBT 프로젝트 재생성: `Build.bat -projectfiles -project=…`.
- **종료 시 D3D11 크래시**(Intel UHD 770 iGPU, SM6 미지원 폴백) — 작업엔 무해, `Close Without Sending`. 근본 해결은 외장 GPU.
- **동기화**: 이번에 Sim `Command.h`/`CombatSim.cpp`에 도착방향 로직 추가 → **서버/SimTesst 사본과 어긋남**(동기화 시 반영 필요). 게임시스템(코스트/지휘관/불복종)은 **Server/GameServer/Sim이 정본**.
  - **2026-08-11 추가 분기**: `Command.h`에 `CommandType::Item` + `Command.itemId`, `Unit.h`의 `GetActionState()`에 `Item` 케이스, `CombatSim.cpp`에 `CommandType::Item` 처리, **신규 `Sim/Item.h`**. 동기화 시 이 4건도 같이 옮길 것. (`Meta/Player.h`는 Sim 밖이라 서버 사본과 무관)

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

### STEP 1: UE 와이어링 (P0 마일스톤 2) — ✅ **완료** (상세: §0.5)
> 목표: "Sim이 굴리는 유닛이 UE 화면에서 움직이는 것"부터. → 달성. 이동/선택/카메라/원형UI까지 확장됨. 현재 STEP 2(전투 시각화) 진행 중.
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
