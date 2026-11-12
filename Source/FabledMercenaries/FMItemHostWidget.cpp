#include "FMItemHostWidget.h"
#include "Components/PanelWidget.h"
#include "FMGameInstance.h"
#include "FMInvItemWidget.h"

UFMItemHostWidget* UFMItemHostWidget::FindOwnerHost(const UWidget* Widget)
{
	return Widget ? Widget->GetTypedOuter<UFMItemHostWidget>() : nullptr;
}

UFMGameInstance* UFMItemHostWidget::GetFMGI() const
{
	return GetGameInstance<UFMGameInstance>();
}

int32 UFMItemHostWidget::GetMercId() const
{
	const UFMGameInstance* GI = GetFMGI();
	return GI ? GI->ActiveMercId : 0;
}

UTexture2D* UFMItemHostWidget::GetItemIcon(int32 ItemId) const
{
	const TObjectPtr<UTexture2D>* Found = ItemIcons.Find(ItemId);
	return Found ? Found->Get() : nullptr;
}

bool UFMItemHostWidget::EquipOn(int32 MercId, int32 ItemId)
{
	UFMGameInstance* GI = GetFMGI();
	if (!GI || MercId == 0 || ItemId == 0) return false;
	return GI->UseItemOnMerc(MercId, ItemId);   // 성공하면 OnRosterChanged → Refresh
}

void UFMItemHostWidget::RebuildInventory(const TArray<FFMItemInfo>& Items)
{
	// 줄은 매번 새로 만든다 (아이템 몇 개라 비용 무시)
	if (!InvList) return;
	InvList->ClearChildren();

	if (!InvItemClass) return;   // WBP_InvItem 미지정 — 빈 목록

	for (const FFMItemInfo& Item : Items)
	{
		UFMInvItemWidget* Row = CreateWidget<UFMInvItemWidget>(this, InvItemClass);
		if (!Row) continue;
		InvList->AddChild(Row);
		Row->SetData(Item);          // 붙인 뒤에 — NativeConstruct가 끝난 다음 값을 덮어쓰게
	}
}
