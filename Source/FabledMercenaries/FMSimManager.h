#pragma once
#include "GameFramework/Actor.h"
#include "Sim/CombatSim.h"
#include "Meta/Player.h"
#include "FMSimManager.generated.h"

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

	/** 지금 시전 가능한가 (MP 충분 + 쿨다운 끝). false면 UI에서 버튼 비활성 */
	UPROPERTY(BlueprintReadOnly) bool  bCanCast = true;
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
	
	/** 선택 유닛의 특정 스킬 정보. 없으면 SkillType=0인 빈 값 */
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
	
protected:
	virtual void BeginPlay() override;
private:
	/** (X,Y) 지점의 실제 지형 높이를 트레이스로 구함 */
	float GroundZAt(float X, float Y) const;
	CombatSim Sim;
	MetaPlayer Meta;					// 인벤토리 등 전투 밖 데이터 (Sim 순수성 유지)
	uint64 SelectedUnitId = 0;			// 0 = 선택된 유닛 없음

	// UE 바닥 높이(클릭 트레이스 Z≈210). Sim 지면(z=0)을 이 높이에 얹어서 그림
	static constexpr float GroundZ = 210.f;

	bool bMenuOpen = false;	// 링 표시 숨김 여부 (UMG에서 설정)
	
	bool bTargeting = false;	// 대상, 목적지 클릭 대기 중 (컨트롤러가 설정)
	
	TMap<uint64, TObjectPtr<class AFMUnit>> UnitActors;   // Sim id → 화면 액터

};
