#pragma once

#include "CoreMinimal.h"
#include "FMItemHostWidget.h"
#include "FMInvPanelWidget.generated.h"

class UTextBlock;

/**
 * 인벤토리 창 (WBP_InvPanel의 부모) — 용병 하나에 매이지 않고 소지품 전체만 본다.
 *
 * - 메뉴바 '인벤토리' 버튼이 HUD의 PanelSwitcher에서 이 창으로 넘긴다.
 * - 줄을 우클릭하면 '지금 고른 용병'(GameInstance.ActiveMercId)에게 장착/사용된다.
 *   누구인지는 TargetText에 적어 둬서 헷갈리지 않게 한다.
 * - 끌어다 놓을 장비 칸은 여기 없다 — 장비를 갈아입히려면 캐릭터 창을 쓴다.
 *
 * WBP에 있어야 하는 위젯: InvList(ScrollBox 등). TitleText/TargetText는 있으면 채운다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMInvPanelWidget : public UFMItemHostWidget
{
	GENERATED_BODY()

public:
	/** 지금 데이터로 다시 그린다 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Refresh();

protected:
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;    // "인벤토리"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TargetText;   // "우클릭 → 전사에게"

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION() void HandleRosterChanged() { Refresh(); }
	UFUNCTION() void HandleMercSelected(int32 MercId) { Refresh(); }
};
