#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMWorldHUDWidget.generated.h"

/**
 * 탐험 HUD (WBP_WorldHUD의 부모) — 전투가 시작되면 통째로 숨는다.
 *
 * 월드맵·탐험맵 양쪽에서 항상 만들어지므로(BP_TopDownController.BeginPlay),
 * 전투에 들어갈 때 메뉴바·용병카드·캐릭터창·인벤토리창이 같이 사라져야 한다.
 * 전투가 끝나면 그대로 돌아온다 — 열려 있던 창은 PanelSwitcher가 기억한다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMWorldHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 메뉴바 버튼 하나에 대응하는 창을 열고 닫는다 — WBP_MenuBar.OnMenuClicked가 이 이름으로 묶여 있다.
	 * 0 = 장비창(왼쪽), 1 = 인벤토리창(오른쪽). 그 밖의 번호는 아직 창이 없어 무시한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void TogglePanel(int32 Index);

	/** 장비창(왼쪽) 열고 닫기 — 메뉴바 '상태' 버튼용 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleCharPanel();

	/** 인벤토리창(오른쪽) 열고 닫기 — 메뉴바 '인벤토리' 버튼용 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleInvPanel();

protected:
	// 두 창은 스위처가 아니라 캔버스에 좌/우로 따로 놓인다 — 같이 띄워 놓고 끌어다 옮기기 위해
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UFMCharPanelWidget> CharPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UFMInvPanelWidget>  InvPanel;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleCombatStateChanged(bool bNowInCombat);

private:
	class AFMSimManager* FindSimManager() const;

	/** 전투 밖에서의 비저빌리티 — 전투가 끝나면 이 값으로 되돌린다 */
	ESlateVisibility ExploreVisibility = ESlateVisibility::SelfHitTestInvisible;
};
