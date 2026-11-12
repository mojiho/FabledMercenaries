#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMItemTypes.h"
#include "FMEquipSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UBorder;

/**
 * 장비창의 칸 하나 (WBP_EquipSlot의 부모) — 아이콘 + 이름.
 *
 * - 인벤토리에서 끌어다 놓으면 장착. 칸 종류가 안 맞으면 테두리가 빨개지고 안 받는다.
 * - 클릭(좌/우 아무거나)하면 해제 → 인벤토리로.
 * - 장착 대상은 '이 칸이 들어 있는 캐릭터 창'의 용병이다 (UFMItemHostWidget::FindOwnerHost).
 *
 * WBP에 있어야 하는 위젯: SlotIcon(Image), SlotNameText(TextBlock).
 * SlotBorder(Border)는 있으면 드래그 중 하이라이트에 쓴다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMEquipSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 칸이 맡은 장비 종류 — WBP 인스턴스마다 지정 (1=무기 2=방어구 3=장신구) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EquipSlot", meta = (ClampMin = "1", ClampMax = "3"))
	int32 EquipSlotValue = 1;

	/** 이 칸에 들어 있는 장비를 표시. ItemId가 0이면 빈 칸(칸 이름만) */
	void SetItem(const FFMItemInfo& InItem);

protected:
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UImage>     SlotIcon;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> SlotNameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder>    SlotBorder;

	/** 비어 있을 때 보여줄 그림 — 비우면 아이콘을 숨긴다 */
	UPROPERTY(EditDefaultsOnly, Category = "EquipSlot")
	TObjectPtr<UTexture2D> EmptyIcon;

	/** 테두리 색 — 평소 / 바로 이 칸 / 다른 칸으로 들어갈 장비 */
	UPROPERTY(EditDefaultsOnly, Category = "EquipSlot") FLinearColor IdleTint      = FLinearColor(1.f, 1.f, 1.f, 0.25f);
	UPROPERTY(EditDefaultsOnly, Category = "EquipSlot") FLinearColor AcceptTint    = FLinearColor(0.3f, 1.f, 0.45f, 0.8f);
	UPROPERTY(EditDefaultsOnly, Category = "EquipSlot") FLinearColor ElsewhereTint = FLinearColor(1.f, 0.8f, 0.3f, 0.6f);

	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual bool NativeOnDragOver(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op) override;
	virtual void NativeOnDragEnter(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& Event, UDragDropOperation* Op) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op) override;

private:
	/** 끌고 온 게 장비인가 (칸이 달라도 받아서 제 칸으로 보낸다) */
	bool Accepts(const UDragDropOperation* Op) const;

	/** 끌고 온 장비가 바로 이 칸 것인가 — 색 구분용 */
	bool IsExactSlot(const UDragDropOperation* Op) const;
	void SetHighlight(const FLinearColor& Tint);

	FFMItemInfo Current;
};
