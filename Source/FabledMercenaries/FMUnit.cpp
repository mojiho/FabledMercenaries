#include "FMUnit.h"
#include "Components/SkeletalMeshComponent.h"

AFMUnit::AFMUnit()
{
	PrimaryActorTick.bCanEverTick = false;   // 매니저가 갱신하니 자체 Tick 불필요
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void AFMUnit::UpdateFromSim(const FVector& Loc, const FVector& FacingDir)
{
	SetActorLocation(Loc);
	if (!FacingDir.IsNearlyZero())
		SetActorRotation(FacingDir.Rotation());   // XY 방향 → Yaw
}