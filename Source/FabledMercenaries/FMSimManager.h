#pragma once
#include "GameFramework/Actor.h"
#include "Sim/CombatSim.h"
#include "Meta/Player.h"
#include "FMEncounter.h"
#include "FMSimManager.generated.h"

enum class ESimEvt : uint8 {AttackFired, Damaged, Death, SkillCast, CmdComplete};

struct FSimEvent
{
	ESimEvt Kind;
	uint64 UnitId = 0;
	int32 Param = 0;
	bool bFromBehind = false;
	bool bCrit = false;
};

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

	/**
	 * 지금 시전 가능한가 (MP 충분 + 쿨다운 끝). false면 UI에서 버튼 비활성.
	 * 기본값 false — FindSkillInfo가 "못 찾음"으로 빈 값을 돌려줄 때(선택 해제 등)
	 * 버튼이 잘못 활성화되는 걸 막는다. GetSelectedUnitSkills는 항상 명시적으로 채움.
	 */
	UPROPERTY(BlueprintReadOnly) bool  bCanCast = false;
};

// 주의: 엔진 Slate(STreeView.h)에 이미 FItemInfo가 있어 이름 충돌 → FM 접두사 필수
USTRUCT(BlueprintType)
struct FFMItemInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32   ItemId = 0;   // Sim ItemType 값
	UPROPERTY(BlueprintReadOnly) int32   Category = 0;   // 0=소비 1=장착 (아이콘/정렬용)
	UPROPERTY(BlueprintReadOnly) int32   Count = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFMMenuCancel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFMCombatEnded, bool, bVictory);
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
	
	/** 선택 유닛의 액티브(시전 가능) 스킬 목록 */
	UFUNCTION(BlueprintCallable, Category = "Command")
	TArray<FSkillInfo> GetSelectedUnitSkills() const;
	
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

	/** 전투 종료 — 이번 인카운터로 스폰된 유닛과 액터를 전부 걷어내고 탐험 상태로 되돌린다 */
	void EndEncounter();

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsInCombat() const { return bInCombat; }

	/** 전투 종료 통지 — true=승리(적 전멸), false=패배(아군 전멸). UI/월드맵이 구독 */
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FFMCombatEnded OnCombatEnded;

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

	/** 전투가 끝나면 월드맵으로 자동 복귀할지. 끄면 컴뱃맵에 남아 탐험을 계속한다 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bReturnToWorldMapAfterCombat = true;

	/** 복귀까지 대기 시간(초) — 0이면 즉시. 승패 결과를 볼 틈을 준다 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float ReturnDelay = 1.5f;

protected:
	virtual void BeginPlay() override;
private:
	/** (X,Y) 지점의 실제 지형 높이를 트레이스로 구함 */
	float GroundZAt(float X, float Y) const;
	CombatSim Sim;
	/**
	 * 전투 밖 데이터(인벤토리 등)는 UFMGameInstance가 소유한다 — 레벨을 넘어가도 살아남게.
	 * GameInstance 클래스가 지정돼 있지 않으면 null을 돌려주므로 호출부에서 반드시 검사할 것.
	 */
	struct MetaPlayer* MetaPtr() const;
	uint64 SelectedUnitId = 0;			// 0 = 선택된 유닛 없음

	// UE 바닥 높이(클릭 트레이스 Z≈210). Sim 지면(z=0)을 이 높이에 얹어서 그림
	static constexpr float GroundZ = 210.f;

	bool bMenuOpen = false;	// 링 표시 숨김 여부 (UMG에서 설정)
	
	bool bTargeting = false;	// 대상, 목적지 클릭 대기 중 (컨트롤러가 설정)
	
	TMap<uint64, TObjectPtr<class AFMUnit>> UnitActors;   // Sim id → 화면 액터

	bool   bInCombat = false;        // 전투 중인가 (false = 탐험)
	uint64 NextUnitId = 100;         // 유닛 id 발급기 (아바타는 1번대를 쓴다)
	TArray<uint64> CombatUnitIds;    // 이번 인카운터로 스폰된 유닛 — 종료 시 이 목록만 지운다

	/** Sim에 유닛 1기 + 화면 액터 1개를 만든다. 반환값은 발급된 id */
	uint64 SpawnSimUnit(EFMClass Cls, uint64 OwnerId, Faction Fac, const FVector2D& PlanarPos);

	/** 전투 중 승패가 갈렸는지 검사 — 갈렸으면 EndEncounter까지 수행 */
	void CheckCombatResolution();

	/** ReturnDelay 뒤에 호출 — GameInstance에 적어둔 월드맵으로 되돌아간다 */
	UFUNCTION()
	void ReturnToWorldMap();

	FTimerHandle ReturnTimer;

	uint64 AvatarUnitId = 0;         // 탐험 아바타 — CombatUnitIds에 넣지 않아 전투 종료에도 살아남는다

	static constexpr uint64 ENEMY_COMMANDER_ID = 2;

};
