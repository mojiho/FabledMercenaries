#pragma once
#include "GameFramework/Actor.h"
#include "Sim/CombatSim.h"
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
};

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
	
	void MoveSelectedTo(const FVector& WorldPos);
	
	UFUNCTION(BlueprintPure, Category = "Sim")
	bool HasSelectedUnit() const { return SelectedUnitId != 0 && !bRingHidden; }

	void SetRingHidden(bool bInHidden) { bRingHidden = bInHidden; }

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
	
protected:
	virtual void BeginPlay() override;
private:
	/** (X,Y) 지점의 실제 지형 높이를 트레이스로 구함 */
	float GroundZAt(float X, float Y) const;
	CombatSim Sim;
	uint64 SelectedUnitId = 0;			// 0 = 선택된 유닛 없음

	// UE 바닥 높이(클릭 트레이스 Z≈210). Sim 지면(z=0)을 이 높이에 얹어서 그림
	static constexpr float GroundZ = 210.f;

	bool bRingHidden = false;	// 링 표시 숨김 여부 (UMG에서 설정)
	
	TMap<uint64, TObjectPtr<class AFMUnit>> UnitActors;   // Sim id → 화면 액터

};
