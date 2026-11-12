#include "FMWorldHUDWidget.h"
#include "FMSimManager.h"
#include "FMCharPanelWidget.h"
#include "FMInvPanelWidget.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** 창 하나 토글 — 닫혀 있으면 열고, 열려 있으면 닫는다 */
	void ToggleWindow(UUserWidget* Window)
	{
		if (!Window) return;
		const bool bOpen = Window->GetVisibility() == ESlateVisibility::Visible;
		Window->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

AFMSimManager* UFMWorldHUDWidget::FindSimManager() const
{
	return Cast<AFMSimManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
}

void UFMWorldHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ExploreVisibility = GetVisibility();

	if (AFMSimManager* Mgr = FindSimManager())
	{
		Mgr->OnCombatStateChanged.AddUniqueDynamic(this, &UFMWorldHUDWidget::HandleCombatStateChanged);

		// 전투가 이미 돌고 있는데 HUD가 늦게 만들어졌을 수도 있다 (디버그 인카운터 자동 시작 등)
		HandleCombatStateChanged(Mgr->IsInCombat());
	}
}

void UFMWorldHUDWidget::NativeDestruct()
{
	if (AFMSimManager* Mgr = FindSimManager())
		Mgr->OnCombatStateChanged.RemoveDynamic(this, &UFMWorldHUDWidget::HandleCombatStateChanged);

	Super::NativeDestruct();
}

void UFMWorldHUDWidget::HandleCombatStateChanged(bool bNowInCombat)
{
	SetVisibility(bNowInCombat ? ESlateVisibility::Collapsed : ExploreVisibility);
}

void UFMWorldHUDWidget::ToggleCharPanel() { ToggleWindow(CharPanel); }
void UFMWorldHUDWidget::ToggleInvPanel()  { ToggleWindow(InvPanel);  }

void UFMWorldHUDWidget::TogglePanel(int32 Index)
{
	switch (Index)
	{
	case 0: ToggleCharPanel(); break;
	case 1: ToggleInvPanel();  break;
	default: break;   // 옵션 등 아직 창이 없는 버튼
	}
}
