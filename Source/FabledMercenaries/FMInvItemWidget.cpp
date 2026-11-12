#include "FMInvItemWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "FMItemDragOp.h"
#include "FMItemHostWidget.h"

namespace
{
	/** "무기 공격+8" / "회복 60" — 줄 오른쪽에 붙는 한 줄 설명 */
	FString DescribeItem(const FFMItemInfo& Item)
	{
		if (Item.Category != 1)
			return FString::Printf(TEXT("회복 %.0f"), GetItemDef((uint32)Item.ItemId).amount);

		FString Desc = FMSlotName(Item.Slot);
		if (Item.AtkBonus > 0.f) Desc += FString::Printf(TEXT(" 공격+%.0f"), Item.AtkBonus);
		if (Item.HpBonus  > 0.f) Desc += FString::Printf(TEXT(" 체력+%.0f"), Item.HpBonus);
		return Desc;
	}
}

void UFMInvItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 마우스를 받아야 끌 수 있다 — WBP 루트가 SelfHitTestInvisible로 놓이는 걸 막는다
	SetVisibility(ESlateVisibility::Visible);
}

void UFMInvItemWidget::SetData(const FFMItemInfo& InItem)
{
	Item = InItem;

	if (ItemNameText) ItemNameText->SetText(FText::FromString(Item.Name));
	if (DescText)     DescText->SetText(FText::FromString(DescribeItem(Item)));
	if (CountText)
	{
		CountText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), Item.Count)));
		CountText->SetVisibility(Item.Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (ItemIcon)
	{
		UTexture2D* Icon = nullptr;
		if (const UFMItemHostWidget* Host = UFMItemHostWidget::FindOwnerHost(this))
			Icon = Host->GetItemIcon(Item.ItemId);

		ItemIcon->SetBrushFromTexture(Icon);
		ItemIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

FReply UFMInvItemWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// 우클릭 = 바로 장착/사용
		if (UFMItemHostWidget* Host = UFMItemHostWidget::FindOwnerHost(this))
		{
			Host->EquipOn(Host->GetMercId(), Item.ItemId);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// 끌기는 마우스가 실제로 움직일 때 시작된다 (그냥 클릭이면 아무 일도 안 일어남)
		return UWidgetBlueprintLibrary::DetectDragIfPressed(Event, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UFMInvItemWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& OutOp)
{
	Super::NativeOnDragDetected(Geometry, Event, OutOp);

	if (Item.ItemId == 0) return;

	UFMItemDragOp* Drag = Cast<UFMItemDragOp>(
		UWidgetBlueprintLibrary::CreateDragDropOperation(UFMItemDragOp::StaticClass()));
	if (!Drag) return;

	Drag->Item   = Item;
	Drag->Pivot  = EDragPivot::MouseDown;

	if (const UFMItemHostWidget* Host = UFMItemHostWidget::FindOwnerHost(this))
		Drag->SourceMercId = Host->GetMercId();

	// 커서에 붙일 그림 — 끌려가는 동안 원본 줄은 목록에 그대로 남는다
	UClass* VisualClass = DragVisualClass ? DragVisualClass.Get() : GetClass();
	if (UFMInvItemWidget* Visual = CreateWidget<UFMInvItemWidget>(this, VisualClass))
	{
		Visual->SetData(Item);
		Visual->SetRenderOpacity(0.75f);
		Drag->DefaultDragVisual = Visual;
	}

	OutOp = Drag;
}
