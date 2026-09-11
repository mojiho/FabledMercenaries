#include "FMGameInstance.h"
#include "Sim/Item.h"

void UFMGameInstance::Init()
{
	Super::Init();

	// 초기 지급 — 예전엔 FMSimManager::BeginPlay에 있었다.
	// 레벨마다 다시 실행되면 컴뱃맵에 들어갈 때마다 포션이 리필되므로 여기로 옮겼다.
	Meta.id = 1;
	Meta.Add((uint32)ItemType::HealPotion, 5);

	UE_LOG(LogTemp, Warning, TEXT("[FM] GameInstance Init — 메타 데이터 준비"));
}
