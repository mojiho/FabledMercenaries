#include "FMRestNode.h"

#include "FMSimManager.h"
#include "FMGameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AFMRestNode::AFMRestNode()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 휴식은 거리 판정으로만
}

void AFMRestNode::BeginPlay()
{
	Super::BeginPlay();

	AFMSimManager* M = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));

	if (!M)
	{
		UE_LOG(LogTemp, Error, TEXT("[FM] 휴식 지점 '%s': 레벨에 FMSimManager가 없습니다"), *GetName());
		return;
	}

	if (!M->bIsWorldMap)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[FM] 휴식 지점 '%s': 월드맵이 아닌 레벨에 배치됨 — 동작하지 않습니다 (FMSimManager.bIsWorldMap 확인)"),
			*GetName());
	}

	Mgr = M;
}

void AFMRestNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AFMSimManager* M = Mgr.Get();
	if (!M || !M->bIsWorldMap || M->IsInCombat()) return;

	if (bDrawDebugRadius)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), RestRadius, 32,
			FColor(80, 200, 255), false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
	}

	FVector AvatarPos;
	const bool bInside = M->GetAvatarWorldPos(AvatarPos)
		&& FVector::Dist2D(GetActorLocation(), AvatarPos) <= RestRadius;

	// 들어오는 순간 한 번만 — 머무는 동안 매 프레임 회복/로그가 나가지 않게
	if (bInside && !bAvatarInside)
	{
		if (UFMGameInstance* GI = GetWorld()->GetGameInstance<UFMGameInstance>())
			GI->RestAllMercenaries();
	}

	bAvatarInside = bInside;
}
