#pragma once
#include "GameFramework/Actor.h"
#include "Sim/CombatSim.h"
#include "Meta/Player.h"
#include "FMEncounter.h"
#include "FMItemTypes.h"
#include "FMSimManager.generated.h"

enum class ESimEvt : uint8 {AttackFired, Damaged, Defended, Death, SkillCast, CmdComplete};

struct FSimEvent
{
	ESimEvt Kind;
	uint64 UnitId = 0;
	int32 Param = 0;
	bool bFromBehind = false;
	bool bCrit = false;

	// SkillCast 중 지점 지정 스킬일 때만 채워진다 — 마법이 터진 자리와 반경(폭발 연출용).
	// 이벤트를 흘릴 때 담아 둔다: 소진 시점에는 명령이 이미 끝나 current가 비어 있을 수 있다.
	FVector Pos = FVector::ZeroVector;
	float   Radius = 0.f;

	/** Damaged: 실제로 깎인 피해량 (방어·크리 배율 적용 후) */
	float Damage = 0.f;
};

/**
 * 유닛이 피해를 입었다 / 막아냈다 — 데미지 팝업이 구독한다.
 * Text는 그대로 띄울 문자열("128", "패링!"), bCrit이면 크게 띄운다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FFMUnitHitDisplay,
	FVector, WorldPos, const FString&, Text, bool, bCrit, bool, bAlly);

USTRUCT(BlueprintType)
struct FSkillInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32   SkillType = 0;   // Sim SkillType 값 (시전 시 skillId)
	UPROPERTY(BlueprintReadOnly) int32   MpCost = 0;
	UPROPERTY(BlueprintReadOnly) float   Cooldown = 0.f;
	UPROPERTY(BlueprintReadOnly) float   CdRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 TargetMode   = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetFilter = 0;

	/** 지점 지정 스킬의 피해 반경 (0 = 범위 피해 없음). 조준 미리보기 원의 크기 */
	UPROPERTY(BlueprintReadOnly) float Radius = 0.f;

	/**
	 * 지금 시전 가능한가 (MP 충분 + 쿨다운 끝). false면 UI에서 버튼 비활성.
	 * 기본값 false — FindSkillInfo가 "못 찾음"으로 빈 값을 돌려줄 때(선택 해제 등)
	 * 버튼이 잘못 활성화되는 걸 막는다. GetSelectedUnitSkills는 항상 명시적으로 채움.
	 */
	UPROPERTY(BlueprintReadOnly) bool  bCanCast = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFMMenuCancel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFMCombatEnded, bool, bVictory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFMCombatStateChanged, bool, bNowInCombat);
UCLASS()
class FABLEDMERCENARIES_API AFMSimManager : public AActor
{
	GENERATED_BODY()

public:
	AFMSimManager();
	virtual void Tick(float DeltaSeconds) override;
	void IssueMoveCommand(uint64 UnitId, const FVector& WorldPos);

	void HandleClick(const FVector& WorldPos);
	uint64 FindUnitNear(const FVector& WorldPos, float Radius) const;

	/** 선택된 유닛의 월드 위치. 있으면 true + OutPos 채움 */
	UFUNCTION(BlueprintPure, Category = "Sim")
	bool GetSelectedUnitWorldPos(FVector& OutPos) const;

	/** 특정 id 유닛의 월드 위치 (살아있을 때만) */
	bool GetUnitWorldPos(uint64 Id, FVector& OutPos) const;
	
	void ClearSelection();
	
	/** 여러 경유지로 이동 (예약 경로). 마지막 도착지에서 ArriveFacing 방향을 봄 */
	void MoveSelectedAlong(const TArray<FVector>& Waypoints, const FVector& ArriveFacing, bool bHasFacing);
	
	/** 클릭 지점 근처의 '적(Hostile)' 유닛 id. 없으면 0 */
	uint64 FindEnemyNear(const FVector& WorldPos, float Radius) const;

	/** 선택 유닛 → 대상에게 공격 명령 */
	void AttackTarget(uint64 TargetId);

	/** 선택 유닛 → 스킬 시전 (SkillType=Sim SkillType 값, 대상 유닛 id) */
	void CastSkill(int32 SkillType, uint64 TargetId);

	/** 선택 유닛이 아이템 사용(자신에게). 인벤 수량 차감 후 Sim에 Item 명령 발행. 성공 시 true */
	UFUNCTION(BlueprintCallable, Category = "Command")
	bool ActivateItem(int32 ItemId);

	UFUNCTION(BlueprintCallable, Category = "Command")
	bool UseConsumable(int32 ItemId);
	
	UFUNCTION(BlueprintCallable, Category = "Command")
	bool EquipItem(int32 ItemId);
	
	/** 메타 인벤토리 목록 (UI용) */
	UFUNCTION(BlueprintCallable, Category = "Command")
	TArray<FFMItemInfo> GetInventory() const;
	
	void MoveSelectedTo(const FVector& WorldPos);
	
	UFUNCTION(BlueprintPure, Category = "Sim")
	bool HasSelectedUnit() const { return SelectedUnitId != 0 && !bMenuOpen && !bTargeting; }

	/** 링 표시 숨김/복구 — 스킬·아이템 목록이 열릴 때 숨기고 닫힐 때 되돌린다 */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	void SetRingHidden(bool bInHidden) { bMenuOpen = bInHidden; }

	/** 대상 클릭 대기 상태 (c++ 전용)*/
	void SetTargeting(bool bIn) {bTargeting = bIn;};
	
	/** 선택 유닛 방어 태세 */
	void IssueDefendSelected();
	
	/** 선택 유닛 유닛 정지 */
	void IssueStopSelected();
	
	/** 선택 유닛 정신 집중 */
	void IssueFocusSelected();

	/** 스폰할 유닛 액터(BP_Unit) — 에디터에서 지정 */
	UPROPERTY(EditAnywhere, Category = "Unit")
	TSubclassOf<class AFMUnit> UnitClass;

	/**
	 * 이 XY 지점의 지형 높이 (UE 월드 Z).
	 * Sim 밖에 있는 액터(배회 심볼 등)가 바닥에 붙으려면 같은 기준이 필요하다.
	 */
	UFUNCTION(BlueprintPure, Category = "World")
	float GetGroundZAt(float X, float Y) const { return GroundZAt(X, Y); }
	
	/** 선택 유닛의 액티브(시전 가능) 스킬 목록 */
	UFUNCTION(BlueprintCallable, Category = "Command")
	TArray<FSkillInfo> GetSelectedUnitSkills() const;
	
	/** 무기 아이템 -> 손에 들 외형 **/
	UPROPERTY(EditAnywhere, Category="Unit")
	TObjectPtr<class UDataTable> WeaponVisualTable;
	
	/**
	 * 선택 유닛의 특정 스킬 정보. 없으면 SkillType=0인 빈 값.
	 * CdRemaining/bCanCast는 호출 시점 기준 실시간 값 → WBP_SkillRow가 Tick에서 불러
	 * 쿨다운 표시·버튼 활성화를 갱신한다 (목록 전체를 다시 만들 필요 없음).
	 */
	UFUNCTION(BlueprintPure, Category = "Command")
	FSkillInfo FindSkillInfo(int32 SkillType) const;

	/** 진영 필터를 적용한 유닛 탐색 (0=Any 1=Ally 2=Enemy) */
	uint64 FindUnitNearFiltered(const FVector& WorldPos, float Radius, int32 Filter) const;

	/** 지면 좌표를 대상으로 스킬 시전 (Point 모드) */
	void CastSkillAtPoint(int32 SkillType, const FVector& WorldPos);

	/** 열려 있는 목록 창을 닫으라는 신호 (UMG가 구독) */
	UPROPERTY(BlueprintAssignable, Category = "Sim")
	FFMMenuCancel OnMenuCancel;

	UFUNCTION(BlueprintPure, Category = "Sim")
	bool IsMenuOpen() const { return bMenuOpen; }

	/** 목록 창 닫기 요청 — 선택은 유지된다 */
	void CancelMenu() { if (bMenuOpen) OnMenuCancel.Broadcast(); }
	
	TArray<FSimEvent> PendingEvents;
	void BindSimCallbacks();
	void DrainSimEvents();

	// ─────────────────────────────────────────────────────────────
	//  인카운터 — 탐험(아바타만) ↔ 전투(용병+적 스폰) 전환
	// ─────────────────────────────────────────────────────────────

	/**
	 * 심볼 접촉 → 전투 시작. Center를 가운데 두고 양 진영을 Separation만큼 벌려 세운다.
	 * FacingDir은 아군이 바라볼 방향(대개 아바타 → 심볼 방향). 이미 전투 중이면 무시.
	 */
	void StartEncounter(const FFMEncounterDef& Def, const FVector& Center, const FVector& FacingDir);

	/**
	 * 연출 붙은 전투 시작 — 화면 암전 → (완전히 어두워지면) 심볼 숨김 + 전투 배치 → 밝아지며
	 * 양 진영이 각자 뒤쪽 맵 끝에서 달려 들어오고 서서히 나타난다. 등장이 끝날 때까지 조작은 막힌다.
	 * 심볼 접촉은 이걸 부른다. (StartEncounter는 연출 없이 즉시 — 디버그 인카운터용)
	 */
	void RequestEncounter(const FFMEncounterDef& Def, const FVector& Center, const FVector& FacingDir, AActor* Symbol);

	/** 암전 중 (요청 ~ 전투 배치 전) */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsEncounterTransitioning() const { return bEncounterTransition; }

	/** 암전 또는 등장 연출 중 — 이 동안 플레이어 명령을 받지 않는다 */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsEncounterIntroPlaying() const;

	/** 화면이 어두워지는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Encounter|Intro")
	float IntroFadeOutTime = 0.35f;

	/** 화면이 다시 밝아지는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Encounter|Intro")
	float IntroFadeInTime = 0.6f;

	/** 유닛이 진형 자리에서 이만큼 뒤(자기 진영 쪽 맵 끝)에서 출발한다 (cm) */
	UPROPERTY(EditAnywhere, Category = "Encounter|Intro")
	float IntroEntryDistance = 900.f;

	/** 출발점에서 진형 자리까지 달려오는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Encounter|Intro")
	float IntroRunTime = 1.6f;

	/** 유닛마다 출발을 0~이 값(초) 사이로 어긋나게 — 한 줄로 동시에 들어오면 기계적으로 보인다 */
	UPROPERTY(EditAnywhere, Category = "Encounter|Intro")
	float IntroStagger = 0.3f;

	/** 전투 종료 — 이번 인카운터로 스폰된 유닛과 액터를 전부 걷어내고 탐험 상태로 되돌린다 */
	void EndEncounter();

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsInCombat() const { return bInCombat; }

	/** 전투 종료 통지 — true=승리(적 전멸), false=패배(아군 전멸). UI/월드맵이 구독 */
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FFMCombatEnded OnCombatEnded;

	/**
	 * 전투 시작/종료 통지 — 전투 밖에서만 쓰는 UI(캐릭터 창·인벤토리)가 숨었다 나오는 데 쓴다.
	 * OnCombatEnded와 달리 '시작'도 알리고 승패는 모른다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FFMCombatStateChanged OnCombatStateChanged;

	/** 피격·방어가 일어난 자리 — 데미지 팝업 레이어가 구독한다 */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FFMUnitHitDisplay OnUnitHitDisplay;

	/**
	 * 타격 하나하나를 전투 로그에도 남길지. 기본 꺼둠 —
	 * 평타가 0.6초마다 들어가는 전투에서는 채팅창이 숫자로 뒤덮인다.
	 * 숫자는 머리 위 팝업으로 보는 게 기본이고, 이건 밸런스 확인용 스위치다.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bLogEveryHit = false;

	/** 에디터에서 지정하는 디버그 인카운터 — BeginPlay에서 바로 시작할 때 쓴다 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	FFMEncounterDef DebugEncounter;

	/** 켜두면 BeginPlay에서 DebugEncounter를 즉시 시작 (심볼 없이 전투만 보고 싶을 때) */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bAutoStartDebugEncounter = true;

	/**
	 * 탐험 아바타(지휘관)를 BeginPlay에서 스폰할지.
	 * 서버 설계상 탐험맵에 상시 존재하는 건 이 아바타 1기뿐이고,
	 * 용병·적은 인카운터 동안에만 Sim에 들어왔다 나간다.
	 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bSpawnExplorationAvatar = true;

	/** 아바타의 Sim 유닛 id. 0 = 아직 없음 */
	uint64 GetAvatarUnitId() const { return AvatarUnitId; }

	/** 탐험 아바타의 월드 위치. 살아있을 때만 true — 심볼이 거리 판정에 쓴다 */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool GetAvatarWorldPos(FVector& OutPos) const;

	/** 아바타를 특정 지점으로 옮긴다 (월드맵 복귀 시 노드 앞에 세우는 용도) */
	void SetAvatarWorldPos(const FVector& WorldPos);

	/**
	 * 아바타 이동 속도(cm/s). BS_Idle_Walk_Run 기준 300=걷기, 600=달리기(조깅).
	 * 전투 유닛은 직업 이속을 그대로 쓴다 — 탐험 아바타만 달린다.
	 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float AvatarMoveSpeed = 600.f;

	/**
	 * 아바타가 걸어서 오를 수 있는 턱 높이(cm). 발 높이보다 이만큼 넘게 높은 곳으로는 못 들어간다 → 점프해야 오른다.
	 * (경사면은 기울기만큼 추가 허용 — BP_Unit의 WalkableSlopeDeg)
	 * 월드맵·탐험(전투 밖)에서만 적용. 전투 유닛은 점프를 못 하므로 막지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float MaxStepUpHeight = 40.f;

	/** 아바타 점프 (화면 연출만 — Sim 위치는 지면 그대로). 공중이거나 아바타가 없으면 false */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	bool JumpAvatar();

	/** 전투가 끝나면 월드맵으로 자동 복귀할지. 끄면 컴뱃맵에 남아 탐험을 계속한다 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bReturnToWorldMapAfterCombat = true;

	/** 이 레벨이 월드맵인가 — 월드맵에서만 휴식(용병 부활·회복)이 허용된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bIsWorldMap = false;

	/** 복귀까지 대기 시간(초) — 0이면 즉시. 승패 결과를 볼 틈을 준다 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float ReturnDelay = 1.5f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	/** (X,Y) 지점의 실제 지형 높이를 트레이스로 구함 */
	float GroundZAt(float X, float Y) const;

	/** 이번 틱에 아바타가 너무 높은 턱으로 들어갔으면 되돌린다 (축별로 다시 시도해 턱을 따라 미끄러지게) */
	void BlockAvatarAtLedges(float PrevX, float PrevY);
	CombatSim Sim;
	/**
	 * 전투 밖 데이터(인벤토리 등)는 UFMGameInstance가 소유한다 — 레벨을 넘어가도 살아남게.
	 * GameInstance 클래스가 지정돼 있지 않으면 null을 돌려주므로 호출부에서 반드시 검사할 것.
	 */
	struct MetaPlayer* MetaPtr() const;
	uint64 SelectedUnitId = 0;			// 0 = 선택된 유닛 없음

public:
	/**
	 * 이 레벨의 기준 바닥 높이 (UE 월드 Z). Sim 지면(z=0)을 이 높이에 얹어서 그린다.
	 * GroundZAt의 트레이스도 이 값을 중심으로 쏘므로, 지형이 올라가 있는 맵에서는 반드시 맞춰야 한다
	 * (평면 테스트 맵 ≈ 210, 마을 맵 ≈ 930). 못 맞히면 유닛이 이 높이로 가라앉는다.
	 */
	UPROPERTY(EditAnywhere, Category = "World")
	float GroundZ = 210.f;

	/** GroundZ 위/아래로 이 거리까지 지형을 찾는다 — 기복이 큰 맵이면 넉넉히 */
	UPROPERTY(EditAnywhere, Category = "World")
	float GroundTraceRange = 5000.f;

	/**
	 * 전투 중 지휘관(아바타)의 턴테이블 한 바퀴 분량. 탐험 중에는 0(턴 제한 없음)으로 두고
	 * StartEncounter에서 이 값을 넣는다 — 월드맵 드래그 이동이 턴에 걸리면 안 되기 때문.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float AvatarTurnMax = 2.0f;

	/** 지휘관 턴테이블 초당 충전량 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float AvatarTurnRate = 1.0f;

	/**
	 * 유닛의 몸 반경을 바닥에 원으로 그린다. 디버그용.
	 *
	 * 이 원이 서로 파고들면 밀어내기가 안 돌고 있는 것이다 —
	 * 겹침 해소가 제대로 돌면 원끼리 닿기만 하고 겹치지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bDrawBodies = false;

	// ──────────────────────────── 장애물 격자 ────────────────────────────
	// Sim은 UE 월드를 모르므로 지형을 직접 물어볼 수 없다. 여기서 레벨을 훑어
	// '못 서는 칸' 격자를 구워 Sim에 포인터로 넘긴다 (CombatSim::SetNavGrid).
	// 서버에서는 같은 격자를 파일로 읽어 쓰면 된다 (NavGrid::Deserialize).

	/** BeginPlay에서 격자를 자동으로 굽는다. 끄면 장애물이 없는 상태(예전 동작)로 돈다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	bool bBakeNavOnBeginPlay = true;

	/** 한 칸 크기(cm). 작을수록 벽을 정밀하게 따지만 베이크가 제곱으로 느려진다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavCellSize = 50.f;

	/** 베이크 범위 — 이 액터를 중심으로 한 정사각형의 한 변(cm). 전장을 덮을 만큼만 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavExtent = 6000.f;

	/**
	 * 격자를 부풀릴 반경(cm). 유닛이 이만큼 벽에서 떨어져 걷는다 — 벽에 몸이 박혀 보이는 걸 막는다.
	 *
	 * 가장 덩치 큰 클래스의 bodyRadius(Class.h, 현재 탱커 44) 이상으로 둘 것.
	 * 이보다 작으면 탱커만 몸이 벽에 끼는 통로가 생긴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavAgentRadius = 45.f;

	/** 유닛 키(cm). 이 높이 안에 걸리는 게 있으면 못 서는 칸으로 본다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavAgentHeight = 180.f;

	/** 걸어 오를 수 있는 턱 높이(cm). 이웃 칸과의 높이차가 여기에 경사 허용분을 더한 값을 넘으면 막는다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavMaxStepUp = 45.f;

	/** 걸어 오를 수 있는 경사(도). 칸 하나를 건너는 동안 이 기울기까지는 턱으로 보지 않는다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavWalkableSlopeDeg = 45.f;

	/** 절벽·턱도 막을지. 끄면 벽과 건물만 막고 높이차는 무시한다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	bool bNavBlockCliffs = true;

	/** 격자를 화면에 그린다 (벽=빨강, 여유칸=주황). 디버그용 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	bool bDrawNavGrid = false;

	/** 격자 디버그를 카메라 주변 이 반경(cm)만 그린다 — 전부 그리면 수만 개가 된다 */
	UPROPERTY(EditAnywhere, Category = "Nav")
	float NavDrawRadius = 1500.f;

	/** 레벨을 훑어 장애물 격자를 다시 굽는다. 지형이나 건물을 옮긴 뒤에 누르면 된다 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Nav")
	void BakeNavGrid();

	/** 격자를 비운다 — 장애물 없는 상태로 되돌린다 */
	UFUNCTION(BlueprintCallable, Category = "Nav")
	void ClearNavGrid();

	/** 이 지점에 유닛이 설 수 있는가 (UI에서 이동 가능 여부를 미리 보여줄 때) */
	UFUNCTION(BlueprintPure, Category = "Nav")
	bool IsNavWalkable(const FVector& WorldPos) const;

private:
	/**
	 * 장애물 격자. Sim에는 포인터로만 넘기므로 수명은 이 액터가 들고 있어야 한다
	 * (EndPlay에서 Sim.SetNavGrid(nullptr)로 끊는다).
	 */
	NavGrid Nav;

	void DrawNavGridDebug() const;

	bool bMenuOpen = false;	// 링 표시 숨김 여부 (UMG에서 설정)
	
	bool bTargeting = false;	// 대상, 목적지 클릭 대기 중 (컨트롤러가 설정)
	
	TMap<uint64, TObjectPtr<class AFMUnit>> UnitActors;   // Sim id → 화면 액터

	bool   bInCombat = false;        // 전투 중인가 (false = 탐험)

	/** bInCombat을 바꾸는 유일한 통로 — 값이 실제로 바뀔 때만 OnCombatStateChanged를 쏜다 */
	void SetCombatActive(bool bActive);

	/**
	 * 시전 중인 지점 지정 스킬의 마법진을 바닥에 그린다.
	 * Sim 상태(current/execGauge/attackFired)만 읽어 매 프레임 다시 그리므로 수명 관리가 필요 없다 —
	 * 명령이 시작되면 나타나고 터지는 순간 사라진다. 명령이 취소돼도 저절로 사라진다.
	 */
	void DrawCastCircle(const class Unit& U) const;

	/** 지정한 반경의 스킬이 터진 자리에 잠깐 남는 폭발 표시 */
	void DrawSkillBlast(const FVector& Center, float Radius) const;

	/** 전투 로그에 쓸 유닛 이름 — "지휘관" / "전사 #2" / "적 궁수" */
	FString UnitLabel(uint64 UnitId) const;

	/** 유닛 머리 위에 팝업 한 개를 띄우라고 알린다 (구독자가 없으면 아무 일도 없다) */
	void ShowHitPopup(uint64 UnitId, const FString& Text, bool bCrit);

	/** 채팅 서브시스템 (없을 수 있다 — GameInstance가 없는 테스트 맵 등) */
	class UFMChatSubsystem* ChatSys() const;

	/** 전투 줄기에 한 줄 */
	void LogCombat(const FString& Text) const;

	/** 시스템 줄기에 한 줄 */
	void LogSystem(const FString& Text) const;
	uint64 NextUnitId = 100;         // 유닛 id 발급기 (아바타는 1번대를 쓴다)
	TArray<uint64> CombatUnitIds;    // 이번 인카운터로 스폰된 유닛 — 종료 시 이 목록만 지운다
	TMap<uint64, uint32> UnitToMercId;   // 로스터에서 스폰된 유닛 id → 용병 id (전투 결과를 되돌려 적는 용도)

	/**
	 * Sim에 유닛 1기 + 화면 액터 1개를 만든다. 반환값은 발급된 id.
	 *
	 * @param bSnapToNav  스폰 지점이 막힌 칸이면 가장 가까운 설 수 있는 칸으로 당긴다.
	 *                    전투 유닛은 켠다 — 진형 좌표가 지형을 안 보고 계산되므로 엄폐물 위에
	 *                    떨어질 수 있고, 그러면 그 유닛은 한 발도 못 움직인다.
	 *                    탐험 아바타는 끈다 — 격자를 따르지 않는 유닛이라(점프로 턱을 넘는다)
	 *                    당기면 멀쩡히 설 수 있는 자리에서 엉뚱한 데로 순간이동한다.
	 */
	uint64 SpawnSimUnit(EFMClass Cls, uint64 OwnerId, Faction Fac, const FVector2D& PlanarPos,
	                    bool bSnapToNav = true);

	/** 전투 중 승패가 갈렸는지 검사 — 갈렸으면 EndEncounter까지 수행 */
	void CheckCombatResolution();

	/** ReturnDelay 뒤에 호출 — GameInstance에 적어둔 월드맵으로 되돌아간다 */
	UFUNCTION()
	void ReturnToWorldMap();

	FTimerHandle ReturnTimer;

	// ── 인카운터 연출 ──
	UFUNCTION()
	void OnEncounterFadedOut();          // 화면이 완전히 어두워진 순간 — 여기서 전투를 배치한다

	bool              bEncounterTransition = false;
	float             IntroEndTime = 0.f;   // 등장 연출이 끝나는 월드 시간
	FFMEncounterDef   PendingDef;
	FVector           PendingCenter = FVector::ZeroVector;
	FVector           PendingDir    = FVector::ForwardVector;
	TWeakObjectPtr<AActor> PendingSymbol;
	FTimerHandle      IntroTimer;

	uint64 AvatarUnitId = 0;         // 탐험 아바타 — CombatUnitIds에 넣지 않아 전투 종료에도 살아남는다

	static constexpr uint64 ENEMY_COMMANDER_ID = 2;

	/**
	 * 아바타 전용 지휘관. 아바타 이동은 탐험 조작이지 전투 지휘가 아니므로 게이지를 무한으로 둔다.
	 * 용병과 같은 지휘관(1)에 묶으면 드래그 이동이 게이지를 바닥내 명령이 조용히 거부되고,
	 * 아바타 코스트가 용병단 예산에 합산돼 불복종 확률까지 올라간다.
	 */
	static constexpr uint64 AVATAR_COMMANDER_ID = 3;

	const struct  FFMWeaponVisual* FindWeaponVisual(uint32 ItemId) const;
	
};
