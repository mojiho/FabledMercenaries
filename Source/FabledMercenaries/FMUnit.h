#pragma once
#include "GameFramework/Actor.h"
#include "FMUnit.generated.h"

UCLASS()
class FABLEDMERCENARIES_API AFMUnit : public AActor
{
	GENERATED_BODY()
public:
	AFMUnit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TObjectPtr<class USkeletalMeshComponent> Mesh;

	/** Sim이 매 틱 호출: 위치·방향 갱신 */
	void UpdateFromSim(const FVector& Loc, const FVector& FacingDir);
};