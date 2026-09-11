#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FMEncounter.h"
#include "FMEncounterSymbol.generated.h"

/**
 * 컴뱃맵을 배회하는 적 심볼 (유니콘 오버로드식 조우).
 *
 * 탐험 중 아바타가 TriggerRadius 안에 들어오면 자기가 들고 있는 인카운터로
 * 그 자리에서 전투를 연다. 전투는 레벨 전환 없이 같은 맵에서 벌어지고,
 * 승리하면 심볼이 사라진다.
 */
UCLASS()
class FABLEDMERCENARIES_API AFMEncounterSymbol : public AActor
{
	GENERATED_BODY()

public:
	AFMEncounterSymbol();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** 이 심볼이 열 전투의 편성 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	FFMEncounterDef Encounter;

	/** 아바타가 이 거리 안에 들어오면 전투 시작 (cm) */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float TriggerRadius = 150.f;

	/** 패배 후 이 시간 동안은 다시 걸리지 않는다 — 재접촉 무한루프 방지 (초) */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float RetriggerCooldown = 3.f;

	/** 아바타를 발견하고 쫓기 시작하는 거리. TriggerRadius보다 커야 의미가 있다 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float ChaseRadius = 700.f;

	/** 시작 지점 주변 이 반경을 배회. 0이면 제자리에 선다 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float MoveSpeed = 220.f;

	/** 아트가 붙기 전까지 조우 반경을 화면에 그린다 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugRadius = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> Mesh;

private:
	/** SimManager의 OnCombatEnded 구독 — 내가 연 전투일 때만 반응한다 */
	UFUNCTION()
	void HandleCombatEnded(bool bVictory);

	void PickPatrolTarget();
	void MoveTowardXY(const FVector& Target, float DeltaSeconds);

	TWeakObjectPtr<class AFMSimManager> Mgr;

	FVector HomeLoc      = FVector::ZeroVector;   // 배회 중심 (배치된 자리)
	FVector PatrolTarget = FVector::ZeroVector;
	bool    bEngaged     = false;                 // 이 심볼이 연 전투가 진행 중인가
	float   CooldownLeft = 0.f;
};
