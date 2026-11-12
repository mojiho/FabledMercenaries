#include "FMEquipSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "FMItemDragOp.h"
#include "FMItemHostWidget.h"
#include "FMCharPanelWidget.h"

void UFMEquipSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 드래그를 받으려면 마우스 이벤트가 이 위젯까지 와야 한다 (디자이너에서 빼먹기 쉬운 설정)
	SetVisibility(ESlateVisibility::Visible);
	SetHighlight(IdleTint);
	SetItem(Current);
}

void UFMEquipSlotWidget::SetItem(const FFMItemInfo& InItem)
{
	Current = InItem;

	const bool bEmpty = (Current.ItemId == 0);
	if (SlotNameText)
	{
		SlotNameText->SetText(FText::FromString(bEmpty ? FMSlotName(EquipSlotValue) : Current.Name));
		SlotNameText->SetOpacity(bEmpty ? 0.45f : 1.f);
	}

	if (SlotIcon)
	{
		UTexture2D* Icon = nullptr;
		if (!bEmpty)
		{
			if (const UFMItemHostWidget* Host = UFMItemHostWidget::FindOwnerHost(this))
				Icon = Host->GetItemIcon(Current.ItemId);
		}
		if (!Icon) Icon = EmptyIcon;

		SlotIcon->SetBrushFromTexture(Icon);
		SlotIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

FReply UFMEquipSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// 좌/우 아무 버튼이나 — 눌러서 벗는다
	if (Current.ItemId != 0)
	{
		// 장비 칸은 캐릭터 창 안에만 산다 — 해제는 그 창이 맡는다
		if (UFMCharPanelWidget* Panel = Cast<UFMCharPanelWidget>(UFMItemHostWidget::FindOwnerHost(this)))
		{
			Panel->UnequipSlot(EquipSlotValue);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

bool UFMEquipSlotWidget::Accepts(const UDragDropOperation* Op) const
{
	// 장비면 다 받는다 — 칸이 달라도 제 칸으로 보내준다(색으로만 구분).
	// 소비 아이템은 여기서 안 받고 창(UFMCharPanelWidget)으로 흘려보낸다 = 마시기
	const UFMItemDragOp* Drag = Cast<UFMItemDragOp>(Op);
	return Drag && Drag->Item.Category == 1 && Drag->Item.ItemId != 0;
}

bool UFMEquipSlotWidget::IsExactSlot(const UDragDropOperation* Op) const
{
	const UFMItemDragOp* Drag = Cast<UFMItemDragOp>(Op);
	return Drag && Drag->Item.Slot == EquipSlotValue;
}

void UFMEquipSlotWidget::SetHighlight(const FLinearColor& Tint)
{
	if (SlotBorder) SlotBorder->SetBrushColor(Tint);
}

void UFMEquipSlotWidget::NativeOnDragEnter(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op)
{
	Super::NativeOnDragEnter(Geometry, Event, Op);
	if (Accepts(Op))
		SetHighlight(IsExactSlot(Op) ? AcceptTint : ElsewhereTint);
}

void UFMEquipSlotWidget::NativeOnDragLeave(const FDragDropEvent& Event, UDragDropOperation* Op)
{
	Super::NativeOnDragLeave(Event, Op);
	SetHighlight(IdleTint);
}

bool UFMEquipSlotWidget::NativeOnDragOver(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op)
{
	Super::NativeOnDragOver(Geometry, Event, Op);
	return Accepts(Op);   // true여야 이 칸이 드롭 지점으로 잡힌다
}

bool UFMEquipSlotWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op)
{
	SetHighlight(IdleTint);
	if (!Accepts(Op)) return false;

	UFMItemHostWidget* Panel = UFMItemHostWidget::FindOwnerHost(this);
	if (!Panel) return false;

	// 끌기 시작했을 때의 용병에게 꽂는다 — 끄는 도중 창이 다른 용병으로 바뀌었어도
	const UFMItemDragOp* Drag = CastChecked<UFMItemDragOp>(Op);
	const int32 MercId = Drag->SourceMercId != 0 ? Drag->SourceMercId : Panel->GetMercId();
	return Panel->EquipOn(MercId, Drag->Item.ItemId);
}
