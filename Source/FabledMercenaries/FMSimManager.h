#pragma once
#include "GameFramework/Actor.h"
#include "Sim/CombatSim.h"
#include "FMSimManager.generated.h"

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
};
