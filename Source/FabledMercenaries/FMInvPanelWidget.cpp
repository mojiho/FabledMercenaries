#include "FMInvPanelWidget.h"
#include "Components/TextBlock.h"
#include "FMGameInstance.h"

void UFMInvPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UFMGameInstance* GI = GetFMGI())
	{
		GI->OnRosterChanged.AddUniqueDynamic(this, &UFMInvPanelWidget::HandleRosterChanged);
		GI->OnMercSelected.AddUniqueDynamic(this, &UFMInvPanelWidget::HandleMercSelected);
	}

	if (TitleText) TitleText->SetText(FText::FromString(TEXT("인벤토리")));

	Refresh();
}

void UFMInvPanelWidget::NativeDestruct()
{
	if (UFMGameInstance* GI = GetFMGI())
	{
		GI->OnRosterChanged.RemoveDynamic(this, &UFMInvPanelWidget::HandleRosterChanged);
		GI->OnMercSelected.RemoveDynamic(this, &UFMInvPanelWidget::HandleMercSelected);
	}
	Super::NativeDestruct();
}

void UFMInvPanelWidget::Refresh()
{
	UFMGameInstance* GI = GetFMGI();
	if (!GI) return;

	if (TargetText)
	{
		bool bFound = false;
		const FFMMercInfo Info = GI->GetMercInfo(GetMercId(), bFound);
		TargetText->SetText(bFound
			? FText::FromString(FString::Printf(TEXT("우클릭 → %s 에게"), *Info.ClassName.ToString()))
			: FText::FromString(TEXT("대상 없음 — 용병 카드를 먼저 고르세요")));
	}

	RebuildInventory(GI->GetInventoryItems());
}
