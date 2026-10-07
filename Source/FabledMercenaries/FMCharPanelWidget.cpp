#include "FMCharPanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "FMGameInstance.h"

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

	FText InvLine(const FFMItemInfo& Item)
	{
		FString Desc;
		if (Item.Category == 1)
		{
			Desc = FMSlotName(Item.Slot);
			if (Item.AtkBonus > 0.f) Desc += FString::Printf(TEXT(" 공격+%.0f"), Item.AtkBonus);
			if (Item.HpBonus  > 0.f) Desc += FString::Printf(TEXT(" 체력+%.0f"), Item.HpBonus);
		}
		else
		{
			Desc = FString::Printf(TEXT("회복 %.0f"), GetItemDef((uint32)Item.ItemId).amount);
		}
		return FText::FromString(FString::Printf(TEXT("%s  x%d   [%s]"), *Item.Name, Item.Count, *Desc));
	}
}

UFMGameInstance* UFMCharPanelWidget::GetFMGI() const
{
	return GetGameInstance<UFMGameInstance>();
}

void UFMCharPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UFMGameInstance* GI = GetFMGI())
	{
		GI->OnMercSelected.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleMercSelected);
		GI->OnRosterChanged.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleRosterChanged);

		// 메뉴바 '상태'로 처음 열었을 때 빈 창이 아니도록 첫 용병을 기본으로
		const TArray<FFMMercInfo> Roster = GI->GetRoster();
		if (CurrentMercId == 0 && Roster.Num() > 0)
			CurrentMercId = Roster[0].MercId;
	}

	WeaponButton   ->OnClicked.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleWeaponClicked);
	ArmorButton    ->OnClicked.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleArmorClicked);
	AccessoryButton->OnClicked.AddUniqueDynamic(this, &UFMCharPanelWidget::HandleAccessoryClicked);

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
	UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(GetParent());
	const bool bShownNow = Switcher && Switcher->IsVisible() && Switcher->GetActiveWidget() == this;

	// 같은 용병 카드를 다시 누르면 닫기
	if (bShownNow && MercId == CurrentMercId)
	{
		Switcher->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ShowMerc(MercId);
	if (Switcher)
	{
		Switcher->SetActiveWidget(this);
		Switcher->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
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

	WeaponText   ->SetText(SlotLine(TEXT("무기"),   Info.Weapon));
	ArmorText    ->SetText(SlotLine(TEXT("방어구"), Info.Armor));
	AccessoryText->SetText(SlotLine(TEXT("장신구"), Info.Accessory));

	// 인벤토리 줄 — 매번 새로 만든다 (아이템 몇 개라 비용 무시)
	InvList->ClearChildren();
	for (const FFMItemInfo& Item : GI->GetInventoryItems())
	{
		UFMItemButton* Row = WidgetTree->ConstructWidget<UFMItemButton>();
		Row->ItemId = Item.ItemId;
		Row->SetBackgroundColor(FLinearColor(0.12f, 0.13f, 0.17f, 1.f));   // 기본 버튼은 밝은 회색이라 흰 글자가 안 보인다
		Row->OnClicked.AddUniqueDynamic(Row, &UFMItemButton::HandleClicked);
		Row->OnPicked.BindUObject(this, &UFMCharPanelWidget::HandleItemPicked);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(InvLine(Item));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = RowFontSize;
		Label->SetFont(Font);

		Row->AddChild(Label);
		InvList->AddChild(Row);
	}
}

void UFMCharPanelWidget::HandleItemPicked(int32 ItemId)
{
	if (UFMGameInstance* GI = GetFMGI())
		GI->UseItemOnMerc(CurrentMercId, ItemId);   // 성공하면 OnRosterChanged → Refresh
}

void UFMCharPanelWidget::Unequip(int32 EquipSlotValue)
{
	if (UFMGameInstance* GI = GetFMGI())
		GI->UnequipMercSlot(CurrentMercId, EquipSlotValue);
}

void UFMCharPanelWidget::HandleWeaponClicked()    { Unequip((int32)EquipSlot::Weapon); }
void UFMCharPanelWidget::HandleArmorClicked()     { Unequip((int32)EquipSlot::Armor); }
void UFMCharPanelWidget::HandleAccessoryClicked() { Unequip((int32)EquipSlot::Accessory); }
