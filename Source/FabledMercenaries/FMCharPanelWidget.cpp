#include "FMCharPanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "FMGameInstance.h"
#include "FMEquipSlotWidget.h"
#include "FMItemDragOp.h"

namespace
{
	FText SlotLine(const TCHAR* SlotLabel, const FFMItemInfo& Item)
	{
		if (Item.ItemId == 0)
			return FText::FromString(FString::Printf(TEXT("%s: 없음"), SlotLabel));

		FString Bonus;
		if (Item.AtkBonus > 0.f) Bonus += FString::Printf(TEXT(" 공격+%.0f"), Item.AtkBonus);
		if (Item.HpBonus  > 0.f) Bonus += FString::Printf(TEXT(" 체력+%.0f"), Item.HpBonus);
		return FText::FromString(FString::Printf(TEXT("%s: %s%s"), SlotLabel, *Item.Name, *Bonus));
	}
}

void UFMCharPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UFMGameInstance* GI = GetFMGI())
	{
		GI->OnMercSelected.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleMercSelected);
		GI->OnRosterChanged.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleRosterChanged);

		// 메뉴바 '상태'로 처음 열었을 때 빈 창이 아니도록 지금 고른 용병을 기본으로
		if (CurrentMercId == 0) CurrentMercId = GI->ActiveMercId;
	}

	if (WeaponButton)    WeaponButton   ->OnClicked.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleWeaponClicked);
	if (ArmorButton)     ArmorButton    ->OnClicked.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleArmorClicked);
	if (AccessoryButton) AccessoryButton->OnClicked.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleAccessoryClicked);

	// 칸마다 자기가 맡은 장비 종류를 못 박아 둔다 — WBP에서 깜빡해도 섞이지 않게
	if (WeaponSlot)    WeaponSlot   ->EquipSlotValue = (int32)EquipSlot::Weapon;
	if (ArmorSlot)     ArmorSlot    ->EquipSlotValue = (int32)EquipSlot::Armor;
	if (AccessorySlot) AccessorySlot->EquipSlotValue = (int32)EquipSlot::Accessory;

	Refresh();
}

void UFMCharPanelWidget::NativeDestruct()
{
	// GameInstance는 레벨이 바뀌어도 살아남는다 — 파괴된 창을 계속 부르지 않게 구독 해제
	if (UFMGameInstance* GI = GetFMGI())
	{
		GI->OnMercSelected.RemoveDynamic(this, &UFMCharPanelWidget::HandleMercSelected);
		GI->OnRosterChanged.RemoveDynamic(this, &UFMCharPanelWidget::HandleRosterChanged);
	}
	Super::NativeDestruct();
}

void UFMCharPanelWidget::HandleMercSelected(int32 MercId)
{
	// 같은 용병 카드를 다시 누르면 닫기
	if (GetVisibility() == ESlateVisibility::Visible && MercId == CurrentMercId)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ShowMerc(MercId);
	SetVisibility(ESlateVisibility::Visible);
}

void UFMCharPanelWidget::ShowMerc(int32 MercId)
{
	CurrentMercId = MercId;
	Refresh();
}

void UFMCharPanelWidget::Refresh()
{
	UFMGameInstance* GI = GetFMGI();
	if (!GI) return;

	bool bFound = false;
	const FFMMercInfo Info = GI->GetMercInfo(CurrentMercId, bFound);

	if (!bFound)
	{
		TitleText->SetText(FText::FromString(TEXT("용병 없음")));
		StatText->SetText(FText::GetEmpty());
	}
	else
	{
		TitleText->SetText(Info.bAlive
			? Info.ClassName
			: FText::FromString(Info.ClassName.ToString() + TEXT("  (사망)")));
		StatText->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f    공격력 %.0f"),
			Info.Hp, Info.MaxHp, Info.Attack)));
	}

	if (PortraitImage)
	{
		const TObjectPtr<UTexture2D>* Portrait = ClassPortraits.Find(Info.UnitClass);
		UTexture2D* Tex = Portrait ? Portrait->Get() : nullptr;
		PortraitImage->SetBrushFromTexture(Tex);
		PortraitImage->SetVisibility(Tex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (WeaponSlot)    WeaponSlot   ->SetItem(Info.Weapon);
	if (ArmorSlot)     ArmorSlot    ->SetItem(Info.Armor);
	if (AccessorySlot) AccessorySlot->SetItem(Info.Accessory);

	if (WeaponText)    WeaponText   ->SetText(SlotLine(TEXT("무기"),   Info.Weapon));
	if (ArmorText)     ArmorText    ->SetText(SlotLine(TEXT("방어구"), Info.Armor));
	if (AccessoryText) AccessoryText->SetText(SlotLine(TEXT("장신구"), Info.Accessory));

	RebuildInventory(GI->GetInventoryItems());
}

bool UFMCharPanelWidget::NativeOnDragOver(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op)
{
	Super::NativeOnDragOver(Geometry, Event, Op);
	return Cast<UFMItemDragOp>(Op) != nullptr;
}

bool UFMCharPanelWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op)
{
	// 장비 칸을 정확히 못 맞췄어도 창 안에 떨어뜨렸으면 제 칸으로 보낸다
	const UFMItemDragOp* Drag = Cast<UFMItemDragOp>(Op);
	if (!Drag) return false;

	const int32 MercId = Drag->SourceMercId != 0 ? Drag->SourceMercId : CurrentMercId;
	return EquipOn(MercId, Drag->Item.ItemId);
}

bool UFMCharPanelWidget::UnequipSlot(int32 EquipSlotValue)
{
	UFMGameInstance* GI = GetFMGI();
	return GI && GI->UnequipMercSlot(CurrentMercId, EquipSlotValue);
}

void UFMCharPanelWidget::HandleWeaponClicked()    { UnequipSlot((int32)EquipSlot::Weapon); }
void UFMCharPanelWidget::HandleArmorClicked()     { UnequipSlot((int32)EquipSlot::Armor); }
void UFMCharPanelWidget::HandleAccessoryClicked() { UnequipSlot((int32)EquipSlot::Accessory); }
