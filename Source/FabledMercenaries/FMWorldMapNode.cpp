#include "FMWorldMapNode.h"

#include "FMSimManager.h"
#include "FMGameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AFMWorldMapNode::AFMWorldMapNode()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 진입은 거리 판정으로만
}

void AFMWorldMapNode::BeginPlay()
{
	Super::BeginPlay();

	Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));

	if (!Mgr.IsValid())
		UE_LOG(LogTemp, Error, TEXT("[FM] 월드맵 노드 '%s': 레벨에 FMSimManager가 없습니다"), *GetName());

	if (CombatMap.IsNull())
		UE_LOG(LogTemp, Error, TEXT("[FM] 월드맵 노드 '%s': Combat Map이 비어 있습니다"), *GetName());
}

void AFMWorldMapNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bEntering) return;

	AFMSimManager* M = Mgr.Get();
	if (!M) return;

	// 컴뱃맵에서 막 돌아왔다면 아바타를 이 노드 옆에 세운다.
	// BeginPlay가 아니라 첫 Tick에 하는 이유: 액터 BeginPlay 순서가 보장되지 않아
	// 그 시점엔 매니저가 아직 아바타를 스폰하지 않았을 수 있다.
	if (!bReturnHandled)
	{
		bReturnHandled = true;

		if (UFMGameInstance* GI = GetWorld()->GetGameInstance<UFMGameInstance>())
		{
			if (!NodeId.IsNone() && GI->LastWorldNodeId == NodeId)
			{
				const FVector Spot = GetActorTransform().TransformPosition(ReturnOffset);
				M->SetAvatarWorldPos(Spot);
				GraceLeft = ReturnGrace;

				GI->LastWorldNodeId = NAME_None;   // 한 번만 — 다음 복귀 때 다시 기록된다
				UE_LOG(LogTemp, Warning, TEXT("[FM] 노드 '%s'로 복귀 — 아바타 배치"), *NodeId.ToString());
			}
		}
	}

	if (GraceLeft > 0.f)
	{
		GraceLeft -= DeltaSeconds;   // 유예 중엔 진입 판정을 건너뛴다
		return;
	}

	if (bDrawDebugRadius)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), EnterRadius, 32,
			FColor(80, 220, 120), false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
	}

	FVector AvatarPos;
	if (!M->GetAvatarWorldPos(AvatarPos)) return;

	if (FVector::Dist2D(GetActorLocation(), AvatarPos) > EnterRadius) return;

	if (CombatMap.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[FM] 노드 '%s' 진입 실패 — Combat Map 미지정"), *GetName());
		return;
	}

	// 돌아올 곳을 기억해둔다 — 레벨이 바뀌면 이 액터는 파괴되므로 GameInstance에 남긴다
	if (UFMGameInstance* GI = GetWorld()->GetGameInstance<UFMGameInstance>())
	{
		GI->LastWorldNodeId = NodeId;
		GI->ReturnWorldMap  = FName(*UGameplayStatics::GetCurrentLevelName(this, true));
	}

	bEntering = true;

	const FName MapName(*CombatMap.ToSoftObjectPath().GetLongPackageName());
	UE_LOG(LogTemp, Warning, TEXT("[FM] 월드맵 노드 '%s' 진입 → %s"), *NodeId.ToString(), *MapName.ToString());

	UGameplayStatics::OpenLevel(this, MapName);
}
