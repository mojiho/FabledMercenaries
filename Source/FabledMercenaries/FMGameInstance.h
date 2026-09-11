#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Meta/Player.h"
#include "FMGameInstance.generated.h"

/**
 * 레벨을 넘나들며 살아남아야 하는 것만 담는다.
 *
 * 월드맵 → 컴뱃맵으로 OpenLevel 하면 레벨 안의 액터는 전부 파괴된다.
 * 인벤토리·로스터 같은 메타 데이터가 SimManager에 붙어 있으면 그때 같이 날아가므로
 * 여기로 올린다. (전투 Sim은 여전히 이 데이터를 모른다 — itemId만 주고받는다)
 */
UCLASS()
class FABLEDMERCENARIES_API UFMGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	/** 전투 밖 데이터 (인벤토리 등) */
	MetaPlayer& GetMeta() { return Meta; }
	const MetaPlayer& GetMeta() const { return Meta; }

	/** 컴뱃맵에 들어가기 직전 밟고 있던 월드맵 노드 — 복귀 시 그 자리에 세우기 위함 */
	UPROPERTY(BlueprintReadWrite, Category = "World")
	FName LastWorldNodeId;

	/** 돌아갈 월드맵 레벨 이름 — 컴뱃맵에서 나올 때 쓴다 */
	UPROPERTY(BlueprintReadWrite, Category = "World")
	FName ReturnWorldMap;

private:
	MetaPlayer Meta;
};
