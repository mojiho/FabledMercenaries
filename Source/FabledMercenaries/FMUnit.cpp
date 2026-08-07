#include "FMUnit.h"
#include "Components/SkeletalMeshComponent.h"

AFMUnit::AFMUnit()
{
	PrimaryActorTick.bCanEverTick = false;   // 매니저가 갱신하니 자체 Tick 불필요
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void AFMUnit::UpdateFromSim(const FVector& Loc, const FVector& FacingDir, EUnitAnim NewAnim, float DeltaSeconds)
{
	// 속도 = 위치 변화량 / dt (이전 위치가 있을 때만)
	if (bHasPrev && DeltaSeconds > 0.f)
		Speed = FVector::Dist2D(Loc, PrevLoc) / DeltaSeconds;
	PrevLoc = Loc;
	bHasPrev = true;

	SetActorLocation(Loc);
	if (!FacingDir.IsNearlyZero())
	{
		FRotator R = FacingDir.Rotation();
		R.Yaw += MeshYawOffset;
		SetActorRotation(R);
	}
	Anim = NewAnim;
}