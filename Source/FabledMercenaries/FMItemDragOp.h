#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "FMItemTypes.h"
#include "FMItemDragOp.generated.h"

/**
 * 아이템 한 개를 끌고 다니는 동안의 꾸러미.
 * 인벤토리 줄(UFMInvItemWidget)이 만들고, 장비 칸(UFMEquipSlotWidget)이 받는다.
 */
UCLASS()
class FABLEDMERCENARIES_API UFMItemDragOp : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** 끌고 있는 아이템 (아이콘·이름·칸 정보까지 통째로 — 받는 쪽이 다시 조회할 필요 없게) */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FFMItemInfo Item;

	/** 어느 용병의 창에서 시작했나 — 끌고 가는 도중 다른 용병이 선택돼도 원래 대상에 꽂힌다 */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 SourceMercId = 0;
};
