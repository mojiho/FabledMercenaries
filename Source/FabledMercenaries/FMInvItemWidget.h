#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMItemTypes.h"
#include "FMInvItemWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 인벤토리 한 줄 (WBP_InvItem의 부모) — 아이콘 + 이름 + 수량.
 *
 * - 좌클릭으로 끌면 장비창 칸에 떨어뜨려 장착.
 * - 우클릭하면 바로 장착(장비) / 사용(소비). 알맞은 칸을 아이템이 스스로 안다.
 * - 대상은 '이 줄이 들어 있는 캐릭터 창'의 용병이다.
 *
 * WBP에 있어야 하는 위젯: ItemNameText(TextBlock).
 * ItemIcon(Image), CountText, DescText는 있으면 채운다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMInvItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 줄이 보여줄 아이템 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetData(const FFMItemInfo& InItem);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FFMItemInfo Item;

protected:
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> ItemNameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     ItemIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescText;   // "무기 공격+8" 같은 설명

	/** 끌고 다닐 때 커서에 붙는 그림 — 비우면 이 줄과 같은 WBP를 하나 더 만들어 쓴다 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UFMInvItemWidget> DragVisualClass;

	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& OutOp) override;
};
