#include "FMMercCardWidget.h"
#include "Components/Button.h"
#include "FMGameInstance.h"

void UFMMercCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CardButton)
		CardButton->OnClicked.AddUniqueDynamic(this, &UFMMercCardWidget::HandleCardClicked);
}

void UFMMercCardWidget::HandleCardClicked()
{
	if (MercId == 0) return;   // SetData 전
	if (UFMGameInstance* GI = GetGameInstance<UFMGameInstance>())
		GI->SelectMerc(MercId);
}
