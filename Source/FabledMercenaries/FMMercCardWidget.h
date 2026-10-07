#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMMercCardWidget.generated.h"

class UButton;

/**
 * 용병 카드(WBP_MercCard)의 부모 클래스.
 * 표시는 BP의 SetData가 하고, 여기선 클릭만 처리한다 — 카드를 누르면 GameInstance에 "이 용병 골랐다"를 알린다.
 * (카드가 HUD나 캐릭터 창을 직접 알 필요가 없게 GameInstance 이벤트로 느슨하게 연결)
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMMercCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 카드가 보여주는 용병 — BP SetData에서 채운다 */
	UPROPERTY(BlueprintReadWrite, Category = "Merc")
	int32 MercId = 0;

protected:
	/** 디자이너의 CardButton과 이름으로 연결된다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CardButton;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleCardClicked();
};
