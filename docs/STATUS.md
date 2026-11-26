# Fabled Mercenaries — 진행 현황 & 다음 할 일 (STATUS)

> 단일 "지금 어디까지 했고 다음에 뭐 할지" 참조. 최종 갱신: **2026-11-26**.
> 관련 문서: 전투/클래스 설계 `combat_class_design.md`, P0 프로토타입 설계 `prototype_phase_p0_design.md`, 서버 설계 `server_technical_design_v2.md`, **아트 파이프라인 `art_pipeline.md`**.
>
> 제약 메모: 1인 **프로그래머** — 코드/개발 시간 충분, **병목은 아트(특히 3D 모델링)**. 전략은 `art_pipeline.md` 참조(베이스 에셋+Mixamo+툰 셰이더+모듈러로 우회).
>
> ⚠️ **이 문서를 갱신할 때**: 코드에서 확인한 것과 "돌려봐야 아는 것"을 구분해 적을 것.
> 2026-09-08판은 2개월 반 방치돼 "적 클릭 공격 붙이는 중"이라고 거짓말을 하고 있었다.

---

## 0. 한 줄 요약

P0 싱글 프로토타입이 **"탐험 → 조우 → 전투 → 복귀"의 한 바퀴가 도는 상태**까지 왔다.
엔진 비의존 전투 Sim 코어 + UE 와이어링 + **장애물 내비게이션(A*)** + **턴테이블 전투** +
인카운터 연출 + 월드맵/탐험맵 전환 + 인벤토리·장비·캐릭터창 + 채팅·말풍선·데미지 팝업까지 동작.

**지금 작업 중**: 전투 연출 · NPC 대화.
**최근에 끝낸 것**: 사망 연출(래그돌 — 플레이 확인 완료) · 턴 카메라 연출(구현 완료, 플레이 확인 대기).

---

## 0.5 ✅ UE 클라이언트 현황 (2026-11-26 기준, 코드에서 확인)

> 위치: `D:\MMO\Client` (한글 경로가 UE 빌드를 깨서 **`D:\MMO`로 이전**). 빌드는 **VS "빌드" 금지**(MSB4018) → **`Build.bat`(UBT)** 또는 Rider.

### 핵심 아키텍처

| 클래스 | 역할 |
|---|---|
| **`AFMSimManager`** (AActor) | `CombatSim Sim` 1개 소유. 모든 것의 허브 — Tick에서 `Sim.Tick()` → 액터 갱신 → 이벤트 소진 → 승패 검사. 유닛은 **Sim이 위치 권위**, 액터는 표시자. |
| **`AFabledMercenariesPlayerController`** (`UCLASS(abstract)`, `BP_TopDownController` 상속) | 입력 전부. `PlayerTick`에서 카메라 추적·턴 연출·드래그 이동 처리. |
| **`AFM_CameraPawn`** (APawn) | SpringArm 쿼터뷰. DefaultPawn. 줌/궤도/WASD/QE + **턴 연출 줌아웃**. |
| **`AFM_CommanderAvatar`** | 탐험 아바타(지휘관). 전투 밖 상시 존재. |
| **`AFMUnit`** (`BP_Unit`) | 유닛 표시 액터. `UpdateFromSim(위치, 방향, 애님상태, dt)` 한 통로로만 갱신. 명령 가능 외곽선(`SetCommandReady`). |
| **`UFMGameInstance`** | 전투 밖 데이터(용병 로스터·인벤토리·장비) 소유 — 레벨을 넘어가도 살아남게. |
| **`UFMChatSubsystem`** | 채팅/로그 버스. 전투 줄기·시스템 줄기 분리. 채팅창·말풍선이 구독. |

### 완료된 시스템

**조작**
- **선택**: 좌클릭=유닛 선택(`FindUnitNear`), 우클릭=해제. 링 표시는 `HasSelectedUnit`(+메뉴 열림/타겟팅 상태).
- **이동**: 원형 메뉴 → 이동모드(커서 고스트 + 지형 따라가는 네비선). `MoveSelectedTo`/`MoveSelectedAlong`.
- **예약 이동**: Ctrl+클릭으로 경유지 누적 → Ctrl 없이 실행.
- **드래그 도착방향**: 이동 확정 클릭을 누른 채 드래그 → 도착 시 그 방향을 봄. (예약은 마지막 도착지만)
- **카메라**: 휠=줌, 우클릭 드래그=궤도, WASD=패닝, Q/E=회전. 월드맵 드래그 이동(누르고 있으면 목적지 추적).
- **점프**: 탐험 아바타만. 턱 높이(`MaxStepUpHeight`) 넘는 곳은 걸어 못 오름 → 점프. 버퍼 입력 있음.

**전투**
- **턴테이블** — `Unit.turnGauge/turnMax/turnRate`, `TurnReady()`. 가득 차면 명령 1회분. **행동 중에도 찬다**(끼어들기가 전투 설계의 핵심). 아바타는 탐험 중 `turnMax=0`(제한 없음).
- **명령 가능 외곽선** — `M_PP_CommandReadyOutline` 포스트 프로세스를 **카메라에 직접** 건다(레벨마다 볼륨 놓지 않음). 턴이 찬 아군만 빛난다.
- **턴 카메라 연출** — 턴이 찬 아군이 화면 밖이면 붐을 벌려 담고 잠시 머문 뒤 돌아온다. 조작하면 즉시 포기(`AbortExpand`). ⚠️ **구현 완료·빌드 확인 완료, 플레이 확인 미완.**
- **원형 커맨드 링**(`WBP_RadialMenu`) — 이동/방어/정신집중/정지/스킬/아이템. C++(`EnterMoveMode`/`CmdDefend`/`CmdFocus`/`CmdStop`/`ChooseSkill`/`ActivateItem`)과 위젯은 전부 있다. ⚠️ **스킬·아이템 버튼의 BP 배선은 코드에서 확인 불가** — 2026-09-08판에 "배선 남음"으로 적혀 있었으니 눌러서 확인할 것.
- **스킬** — `WBP_SkillList` + `WBP_SkillRow`. MP·쿨다운 게이팅, 대상 모드(유닛/지점), 진영 필터. 지점 스킬은 바닥 마법진 + 조준 원 + 폭발 표시.
- **아이템** — `WBP_ItemList`/`WBP_ItemRow`. 소모품 즉시 사용, 장비 착용.
- **데미지 팝업**(`WBP_DamagePopup`) — `OnUnitHitDisplay` 구독. 숫자/방어 텍스트, 크리는 크게, 아군/적 구분.
- **사망 연출**(2026-11-26) — `AFMUnit::NotifyDeath` → `PlayDeathPose` → 끝나면 래그돌. ✅ 플레이 확인 완료.
  애님 그래프를 **통째로 우회**한다(`SetAnimationMode(AnimationSingleNode)` + `PlayAnimation`) — 이유는 §5 "애님 슬롯" 함정 참조.
  쓰러지는 동작이 선 자세로 끝나므로 그 뒤를 물리에 넘긴다(`bRagdollOnDeath`, 기본 켜짐).
  전투 시작부터 시체인 용병(`PlayEntrance`)도 같은 경로로 처리 — 곧바로 물리.
  충돌은 엔진 `Ragdoll` 프로파일: `Visibility`를 무시하므로 **바닥 높이 측정·장애물 판정·클릭(전부 `ECC_Visibility`)에 시체가 안 걸린다.** 바닥(WorldStatic)에만 떨어진다.
  ⚠️ Mesh가 루트라 물리가 켜지면 위치를 물리가 쥔다 → `UpdateFromSim`이 중간에 빠진다(페이드 처리 **뒤**에서. 앞에서 빠지면 시체가 영영 안 보인다).
  ⚠️ **되살린 용병 경로(`EndRagdoll`)는 아직 안 타 봤다.** 부활시킨 용병이 시체처럼 끌려다니면 호출 지점이 틀린 것.
- **승패 처리** — 적 전멸=승리, 아군 전멸=패배. `OnCombatEnded` → 월드맵 복귀(`ReturnDelay` 뒤).

**인카운터 / 맵**
- **조우 연출** — 심볼 접촉 → 암전 → 전투 배치 → 밝아지며 양 진영이 맵 끝에서 달려 들어옴(유닛마다 출발 시각 어긋남). 연출 중 조작 차단.
- **배회 심볼**(`AFMEncounterSymbol`) — 탐험맵을 돌아다니다 아바타에 닿으면 전투.
- **진형 배치** — `FFMEncounterDef`(DataTable 행 가능): 적 편성 + 양 진영 간격. 아군은 GameInstance 로스터로 치른다.
- **휴식**(`AFMRestNode`) — 월드맵에서만 용병 부활·회복.
- **월드맵**(`AFMWorldMapNode`, `WBP_WorldHUD`, `WBP_Minimap`) — 노드 이동, 전투 후 복귀 지점 복원.

**내비게이션 (신규 — 10월 이후)**
- **`NavGrid`** — `AFMSimManager`가 `BeginPlay`에서 레벨을 훑어 "못 서는 칸" 격자를 굽고 Sim에 포인터로 넘긴다(`CombatSim::SetNavGrid`). 에디터에서 `BakeNavGrid` 버튼으로 재베이크.
  - 벽·건물 + 절벽/턱(높이차)까지 막는다. 에이전트 반경만큼 부풀려 벽에 몸이 박히지 않게.
  - **서버는 같은 격자를 파일로 읽으면 된다**(`NavGrid::Deserialize`) — 설계상 이미 분리돼 있다.
- **A* 경로 + 재탐색** — 추격 중 `REPATH_INTERVAL`(0.4s) 주기로 다시 탐색. 매 틱 A*는 너무 비쌈.
- **시야/거점**(`HasLineOfSight`, `FindVantagePoint`) — 원거리 유닛이 벽에 막히면 쏠 수 있는 자리를 찾아 옮긴다.
- **몸 겹침 해소**(`SeparateUnits`) + 클래스별 `bodyRadius`(탱커 44 ~ 암살자 28).
- **이동 포기**(`STUCK_GIVEUP`) — 1.2초 동안 못 나아가면 포기. 벽에 끼어 영원히 비비는 걸 막는다.

**UI 전반**
`WBP_MenuBar`, `WBP_CharPanel`, `WBP_InvPanel`/`WBP_InvItem`/`WBP_EquipSlot`(드래그 앤 드롭), `WBP_MercCard`, `WBP_Chat`, `WBP_SpeechBubble`, `WBP_Minimap`, `WBP_WorldHUD`.

**무기 외형** — `DT_WeaponVisuals` + `FindWeaponVisual`로 장비 아이템 → 손에 들 메시.

### 레벨

| 레벨 | 용도 |
|---|---|
| `Lvl_World` | 월드맵 |
| `Lvl_WorldVillage` | 마을(탐험) — 가장 최근 작업 |
| `Lvl_TopDown` | 전투/기능 테스트 |
| `map_village_*`, `Main`, `Overview` | 구매 에셋 데모 맵(참고용) |

---

## 1. ✅ Sim 코어 (엔진 비의존, 순수 C++)

전부 `UWorld`/`AActor` 비의존. **`Client/Source/FabledMercenaries/Sim/`가 정본.**

- **기반**: `Vec3`, `Command`(slotId/type/waypoints/targetId/skillId/itemId/arriveFacing/targetPos), 결정적 난수 `Rand01`(xorshift).
- **유닛 모델**: `Unit` 단일 타입 — pos/hp/mp/alive/moveSpeed/faction/facing + 수행상태(execGauge/current/executing/reserveQueue) + `vector<Skill>` + 원거리 파라미터 + **턴테이블** + **bodyRadius** + **내비 상태**(navPath/navIndex/repathTimer/vantage/stuck). 다형성은 `Brain`(컴포넌트)로, faction은 데이터(런타임 전향 가능).
- **명령 시스템**: 이동(웨이포인트), 즉시 대체(인터럽트), 명시적 예약 큐, 자동 승격. 2계층 게이지(지휘관 `cmdGauge` × 유닛 수행) + **턴테이블**.
- **클래스/스탯**: 전사·탱커·마법사·궁수·암살자·힐러 — 이속/사거리/원거리/선딜/후딜/공격력/체력/**코스트**/**몸반경**.
- **방어**: Passive(패링·방패·회피 확률 롤 + 쿨다운) + Active(방어 태세 — 차단율↑·공격 둔화·이동 불가).
- **후면 크리**: 피격 방향이 `facing` 뒤면 방어 무시 + 크리 2배. idle 유닛은 피격 후 돌아봄.
- **투사체**: 호밍/포물선 + **몸빵(interception)** — 경로상 적 진영 유닛이 맞고 아군은 관통(탱커가 화살 차단).
- **스킬**: `MakeDefenseStance` / `MakeCharge`(돌진+스턴) / `MakeMagicBolt` / `MakeHeal` / **`MakeMeteor`**(지점 지정 범위 마법, `ApplyAreaDamage`). MP·쿨다운 게이팅.
- **AI Brain**: `ChaseAttackBrain` / `KiteBrain` / `MageBrain` / `HealerBrain` / `TankBrain`(후방 딜러 앞을 방어 태세로 지킴) / `GuardBrain`(어그로 반경 자동 반격).
- **지휘관**: 전투형/지휘형(`CommanderType`), `costBudget`/`usedCost` + **코스트 소프트캡 불복종**(`DISOBEY_SLOPE`/`DISOBEY_MAX`, `CommandResult::Disobeyed`).
- **이벤트 콜백**: `OnCommandComplete` / `OnDamaged` / `OnDefended` / `OnDeath` — UE 쪽 연출·팝업·로그가 여기에 붙는다.
  ⚠️ `OnDeath`는 **`uint64 id` 하나만** 넘긴다. 피격 방향을 모르므로 방향별 사망 연출을 고를 수 없다. 하려면 시그니처를 바꿔야 하고, 그러면 서버 사본과 더 벌어진다(§2).

### ⚙️ 의도적으로 그렇게 둔 값 (버그 아님)

- **`GUARD_AGGRO = 300`** — 조작감을 보려고 일부러 좁게 둠. 떨어져 있는 유닛이 자동 교전하지 않는 건 설계다.
- **턴 게이지가 행동 중에도 찬다** — 끼어들기가 전투 설계의 핵심이라 의도된 것.

---

## 2. 📁 코드 위치 & ⚠️ 동기화 상태 (2026-11-26 실측)

- **`MMO/Client/Source/FabledMercenaries/Sim/`** — **Sim 코어 정본.** UE 와이어링을 하며 계속 앞서간다.
- **`MMO/Server/GameServer/Sim/`** — 서버 사본. **2026-10-07~14에 한 번 동기화됨.** 게임시스템 설계는 서버가 정본이지만 구현은 Client가 앞섬.
- **`MMO/SimTesst/SimTesst/`** — 콘솔 테스트. **2026-06-30 이후 방치(심각하게 stale).** 사실상 버려진 상태 — 되살릴지 폐기할지 결정이 필요하다.

### Client → Server 미반영 목록 (파일 단위 실측)

| 파일 | 상태 | Client에만 있는 것 |
|---|---|---|
| **`NavGrid.h` / `NavGrid.cpp`** | **서버에 없음** | **신규 파일 전체** — 장애물 격자 |
| `CombatSim.h` | 다름 (+80줄) | `SetNavGrid`/`Nav()`, `HasLineOfSight`, `FindVantagePoint`, `BuildPath`/`ResolveStep`/`ChaseStep`/`LastWalkableAlong`, `SeparateUnits`, `TryDefend`, `ApplyAreaDamage`, `OnDamaged`/`OnDefended` 콜백, 상수(`REPATH_INTERVAL`/`PUSH_RATE`/`STUCK_*`/`VANTAGE_*`/`PROJ_HIT_MIN`) |
| `CombatSim.cpp` | 다름 (+477줄) | 위 전부의 구현 |
| `Unit.h` | 다름 (+56줄) | `bodyRadius`, `useNavGrid`, 내비 상태(`navPath`/`navIndex`/`repathTimer`), 거점(`vantagePos`/`hasVantage`/`vantageTimer`), 끼임 감지(`moveStuckTimer`/`moveProgDist`), **턴테이블**(`turnGauge`/`turnMax`/`turnRate`/`TurnReady`/`TurnRatio`) |
| `Class.h` | 다름 (+21/−9) | `ClassStats.bodyRadius` + 클래스별 값, 스탯 수치 조정, 마법사에 Meteor 추가 |
| `Skill.h` | 다름 (+27줄) | `SkillType::Meteor`, `Skill.radius`, `MakeMeteor` |
| `AIBrain.cpp` | 다름 (+17줄) | 내비/거점 연동 |
| 같음 (복사 불필요) | — | `AIBrain.h`, `Brain.h`, `Command.h`, `Commander.h`, `Item.h`, `Projectile.h`, `Vec3.h` |

`Meta/Player.h`는 Sim 밖(메타 데이터)이라 서버 사본과 무관.

> **동기화 방향은 Client → Server/SimTesst다.** 반대로 하면 최신 작업이 날아간다.

---

## 3. 🎯 다음 할 일

### 지금 당장 (작은 것부터)

1. **턴 카메라 플레이 확인** — 구현·빌드는 끝났고 실제로 도는 걸 아직 못 봤다. PIE에서 전투 중 유닛을 멀리 보내고 도착 순간을 보면 된다. 진단용 `[FM][DBG]` 로그가 들어가 있다.
2. **`[FM][DBG]` 로그 제거** — 8곳. 턴 카메라 확인이 끝나면 걷어낸다.
   `FM_CameraPawn.cpp:171,178` / `FMSimManager.cpp:746,860,1246,1257` / `FabledMercenariesPlayerController.cpp:224,262`
3. **턴 카메라 쿨다운 튜닝** — `ExpandCooldown = 2.0`인데 `turnMax = 2.0`이다. **턴 주기와 카메라 쿨다운이 같아서** 3v3에서는 계속 벌어지려 하거나 계속 쿨다운에 걸릴 수 있다. 플레이해 보고 4~5초로 올리거나 "선택한 유닛은 알리지 않음" 조건을 더한다.

### STEP 2: 전투 시각화 (진행 중 — 가장 큰 '보이는' 구멍)

Sim은 선딜→발사→후딜→사망까지 다 돌고 있는데 **화면이 그걸 안 보여 준다.**

- **`ABP_Unit` 애님 전이** — `Move → Cast` 미구현. 시전이 모션으로 안 나온다. ← **우선순위 1**
  (`Cast → Dead`는 **이제 필요 없다** — 사망이 그래프를 우회하므로. 그래프의 `Dead` 상태도 사실상 사문이다.)
- **`HitMontage` / `SkillMontages`가 비어 있다** — 크리 피격 반응과 스킬 시전 모션이 아무것도 안 나온다. 동작 에셋이 필요한 일.
  ⚠️ 붙일 때 §5 "애님 슬롯" 함정을 먼저 읽을 것 — 전신 동작이면 몬타주로는 안 된다.
- HP/MP 바를 디버그 드로우에서 제대로 된 위젯으로.
- 피격 반응(히트 플래시/넉백), 스킬 이펙트(현재는 디버그 원·구).

### NPC 대화 (진행 중)

- `UFMSpeechBubbleWidget`은 완성돼 있다. `ResolveSpeaker()`가 지금은 "내 아바타"만 찾는다 → **이름 → 액터 조회로 바꾸면** 남의 말풍선도 그대로 뜬다(그리는 쪽은 손댈 필요 없음).
- 말풍선 꼬리(nub)가 네모다 — 삼각형으로.
- 대화 UI가 `FInputModeUIOnly`를 쓰게 되면 ⚠️ **채팅창이 닫힐 때 항상 `FInputModeGameAndUI`로 되돌려** 입력 모드를 뺏는 버그가 있다. 대화 붙이기 전에 고칠 것.

### 에셋 (아트 병목 — `art_pipeline.md`)

- **끝까지 눕는 사망 동작** — 구하면 `DeathAnims`에 넣고 `bRagdollOnDeath`를 꺼서 물리 비용을 없앨 수 있다. 방향별(앞/뒤/좌/우)로 있으면 더 좋다. 코드는 양쪽 다 되게 남겨 뒀다.
- **피격 반응 / 스킬 시전 동작** — `HitMontage`, `SkillMontages`가 비어 있다.

### 느낌 튜닝 (쌓아둔 것)

- `RightClickDragThreshold` 14 → 20~25 (궤도 회전이 너무 쉽게 걸림)
- `JumpBufferTime` 0.25 → 0.35
- 미니맵 `YawOffset`, `MeshYawOffset` −90 확인
- `Lvl_World`에 캠프파이어 재배치 (`BP_PROP_campfire_01/02`)

### 나중에

- **Client → SimTesst/Server 동기화** (§2 표). 서버 작업 들어가기 직전에. 지금은 Client가 너무 빨리 움직여서 할 때마다 어긋난다.
- SimTesst를 되살릴지 폐기할지 결정 (5개월 방치).

---

## 4. 🔧 남은 Sim 전투 기능

- **탱커 생존력 보강(선택)** — 방어 태세는 정면 한정이라 협공(암살자 측면 백스탭)에 우회됨. "방어 중 가장 가까운 위협을 바라보기" 등. (현재도 시간 벌어 팀 승리엔 충분)
- **충돌/접촉 규칙**(`combat_class_design.md` §7) — 전사/탱커 밀치고 전진, 방어 태세 탱커 충돌 시 반격뎀+스턴→명령 해제, 암살자 백스탭→모든 명령 취소.
  (※ `SeparateUnits`로 **겹침 해소**는 들어왔지만 **접촉 규칙**은 아직)
- **상태이상 등급 체계** — 짧은 스턴=명령 해제 / 상급=명령 취소. (현재 `stunRemaining` 기본형만)
- **전직 트리 / 강림(메타)** — 전사→양손검사/도끼전사 분기, 2~3차 전직 후 강림.
- 투사체 V2(이동 회피, 포물선 차단 규칙).
- 수치 튜닝: 지휘관 유형, 불복종 확률 곡선.

---

## 5. ⚠️ 환경/함정 (하드-원 교훈)

**빌드**
- **`/utf-8` 프로젝트 전역 필수** — 한글 주석 cp949 오독 → ODR 크래시(한 세션 날림). 서버/클라 vcxproj 모든 구성 `AdditionalOptions`.
- 빌드 명령:
  `& "…UE_5.8\…\Build.bat" --% FabledMercenariesEditor Win64 Development -Project="D:\MMO\Client\FabledMercenaries.uproject" -WaitMutex`
- **헤더(UFUNCTION/멤버/시그니처/신규 UPROPERTY) 변경 → 에디터 닫고 풀 리빌드.** 본체(.cpp)만이면 Live Coding OK.
- **🔴 Live Coding ↔ 풀 빌드 교착** (2026-11-25에 한 시간 날림)
  - 증상: `Ctrl+Alt+F11`은 "**no code changes detected**", 풀 빌드는 "**Unable to build while Live Coding is active**". 양쪽이 서로를 막고 에디터는 옛 DLL을 계속 쓴다.
  - 원인: 어떤 빌드가 **obj만 만들고 링크를 못 했다.** 그러면 obj가 소스보다 새것이 되어 Live Coding은 "이미 컴파일됨"으로 판단하고, UBT는 에디터가 DLL을 잡고 있어 거부한다.
  - 해결: **에디터를 닫고 풀 빌드.** (급하면 소스 파일 타임스탬프만 현재로 올려 Live Coding을 깨우는 방법도 됨)
  - 확인법: `Binaries/Win64/UnrealEditor-FabledMercenaries.dll`의 수정 시각 vs 소스 수정 시각. DLL이 더 낡으면 안 들어간 것이다.
- **🔴 에디터 로그 시각은 UTC다** (KST−9). `Saved/Logs/FabledMercenaries.log`의 "07:43"은 로컬 **16:43**이다. 이걸 모르고 읽으면 "그건 수정 전 기록"이라고 엉뚱하게 진단한다.
- **신규 `UPROPERTY` 반영 확인법** — CDO(`/Script/FabledMercenaries.Default__클래스명`)에 값이 보이는지 본다. CDO엔 있는데 BP 인스턴스만 None이면 풀 빌드를 안 한 것.

**애님 슬롯 / 동작 에셋** (2026-11-26, 사망 연출 붙이다 두 번 헛짚음)
- **🔴 `ABP_Unit`의 `DefaultSlot`은 상체 전용이다** (Layered Blend Per Bone).
  슬롯 몬타주(`PlaySlotAnimationAsDynamicMontage`)로 **전신 동작을 재생하면 다리는 선 채로 상체만 움직인다.**
  공격 동작은 원래 상체 위주라 멀쩡해 보여서 이 제약이 오래 숨어 있었다. `JumpAnim`/`FallAnim`/`WeaponAttackAnims`가 전부 이 슬롯을 쓴다.
  - **전신 동작이 필요하면**: 그래프를 통째로 우회한다 — `Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode)` + `Mesh->PlayAnimation(Anim, false)`. 루프를 끄면 마지막 프레임에서 선다. 되돌리기는 `SetAnimationMode(AnimationBlueprint)`.
  - 정석은 그래프에 전신 슬롯을 하나 추가하는 것. 그건 에디터에서 애님 그래프를 열어야 한다(MCP로는 못 읽고 못 고친다 — `read_graph_dsl`이 애님 그래프엔 빈 값을 준다).
- **🔴 `MM_Death_*` 3종은 "선 채로 허리만 꺾인" 자세로 끝난다.** 래그돌로 넘길 전제로 만든 짧은 리액션(0.93~1.13초)이라 **애니메이션만으로는 시체가 눕지 않는다.** 그래서 `bRagdollOnDeath`가 기본 켜짐이다. 끝까지 눕는 동작을 구하면 끄면 된다.
- **자세는 추측하지 말고 숫자로 확인할 것** — `Mesh->GetBoneTransform(FName("pelvis"/"head"/"foot_l"), RTS_Component)`의 Z를 찍는다.
  서 있음 = 골반 ~95 / 머리 ~165 / 발 ~0. 누움 = 셋 다 20 아래. **상체만 꺾임 = 골반 ~88 / 머리 ~85 / 발 ~11**(실측값).
  이 측정 한 번이 추측 두 번보다 빨랐다.

**에디터**
- **"컴파일" ≠ "저장".** BP는 자동저장 안 됨 → 닫기 전 **모두 저장**. (복구는 `Saved/Autosaves/`)
- **`AFM_CameraPawn`은 BP 서브클래스가 없다** — `BP_TopDownGameMode`의 `DefaultPawnClass`가 C++ 클래스 직접이다. 그래서 `EditAnywhere` 값도 **디테일 패널에서 못 바꾼다**. 숫자 하나 바꾸는 것도 헤더 수정 + 풀 빌드. 튜닝을 자주 할 거면 `BP_CameraPawn`을 만들어 `DefaultPawnClass`를 그쪽으로 돌릴 것.
- **World Partition 레벨은 기존 액터 속성 편집이 디스크에 안 써진다**(MCP로 `save true`를 줘도). 새 액터만 저장된다 — 반영됐는지는 git으로 확인.

**환경**
- **편집기: Rider** (VS 2026 업데이트가 WebView2에서 깨짐). UE5.8용 **.NET 10 런타임 필요**. 프로젝트 재생성: `Build.bat -projectfiles -project=…`.
- **종료 시 D3D11 크래시** (Intel UHD 770 iGPU, SM6 미지원 폴백) — 작업엔 무해, `Close Without Sending`. 근본 해결은 외장 GPU.
- `GroundZ`는 레벨마다 맞춰야 한다 (평면 테스트 맵 ≈ 210, 마을 맵 ≈ 930). 못 맞히면 유닛이 가라앉는다.

---

## 6. ❓ 미해결 설계 결정 (`combat_class_design.md` §8)

교전 결과 모델 세부, 클래스별 수치(방어 확률·크리 배율·선후딜), 백어택 각도·방어무시, MP 수치, Defend 효과, 전직 트리/강림 상세, 지휘관 유형 수치, 불복종 확률 곡선, 충돌 판정 방식 — **P0 재미검증하며 튜닝**.

추가로 쌓인 것:
- 턴테이블 수치(`turnMax` 2.0 / `turnRate` 1.0)가 맞는 템포인가.
- 턴 카메라 연출을 언제 띄울지 — 모든 유닛? 선택 안 한 유닛만? 화면 밖일 때만?
- 시체를 전투 내내 남길 것인가 — 지금은 남는다(래그돌 상태로). 수가 쌓이면 물리 비용과 시야 가림이 문제가 될 수 있다. 일정 시간 뒤 가라앉히기/지우기, 또는 N구 넘으면 오래된 것부터 정리 등.
- 방향별 사망 연출을 할 것인가 — 하려면 Sim의 `OnDeath` 시그니처부터 바꿔야 한다(§1).
