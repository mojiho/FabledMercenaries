#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "FMCharPanelWidget.generated.h"

class UTextBlock;
class UPanelWidget;

/** 인벤토리 한 줄 — 누르면 자기 ItemId를 알린다 (UButton OnClicked엔 인자가 없어서 따로 둔다) */
UCLASS()
class FABLEDMERCENARIES_API UFMItemButton : public UButton
{
	GENERATED_BODY()

public:
	int32 ItemId = 0;

	DECLARE_DELEGATE_OneParam(FOnPicked, int32 /*ItemId*/);
	FOnPicked OnPicked;

	UFUNCTION()
	void HandleClicked() { OnPicked.ExecuteIfBound(ItemId); }
};

/**
 * 캐릭터 창 (WBP_CharPanel의 부모) — 선택한 용병의 스탯·장비 3칸 + 공용 인벤토리.
 *
 * - 카드 클릭 → GameInstance.OnMercSelected → 이 창이 그 용병으로 열린다. 같은 카드를 다시 누르면 닫힌다.
 *   창은 HUD의 PanelSwitcher 안에 있으므로 부모 스위처를 직접 켜고 끈다.
 * - 인벤토리 줄 클릭: 장비면 장착(같은 칸 장비는 반납), 소비 아이템이면 사용(포션 = 회복)
 * - 장비 칸 클릭: 해제 → 인벤토리로
 * - 변경은 전부 GameInstance가 처리하고 OnRosterChanged를 쏜다 → 이 창과 카드 목록이 함께 다시 그려진다.
 *
 * 레이아웃은 WBP에서 자유롭게 바꿔도 된다 — 아래 이름의 위젯만 있으면 된다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMCharPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 용병을 보여준다 (창 열기/닫기는 안 함) */
	UFUNCTION(BlueprintCallable, Category = "CharPanel")
	void ShowMerc(int32 MercId);

	/** 지금 데이터로 다시 그린다 */
	UFUNCTION(BlueprintCallable, Category = "CharPanel")
	void Refresh();

	UPROPERTY(BlueprintReadOnly, Category = "CharPanel")
	int32 CurrentMercId = 0;

protected:
	// ── 디자이너 위젯 (이름으로 연결) ──
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock>   TitleText;      // "전사"
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock>   StatText;       // "HP 130 / 170   공격력 37"
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UButton>      WeaponButton;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock>   WeaponText;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UButton>      ArmorButton;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock>   ArmorText;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UButton>      AccessoryButton;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock>   AccessoryText;
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UPanelWidget> InvList;        // 인벤토리 줄이 들어갈 곳 (ScrollBox 등)

	/** 인벤토리 줄 글자 크기 */
	UPROPERTY(EditAnywhere, Category = "CharPanel")
	int32 RowFontSize = 14;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION() void HandleMercSelected(int32 MercId);
	UFUNCTION() void HandleRosterChanged() { Refresh(); }
	UFUNCTION() void HandleWeaponClicked();
	UFUNCTION() void HandleArmorClicked();
	UFUNCTION() void HandleAccessoryClicked();

	void HandleItemPicked(int32 ItemId);
	void Unequip(int32 EquipSlotValue);
	class UFMGameInstance* GetFMGI() const;
};
