#pragma once

#include "CoreMinimal.h"
#include "FMItemHostWidget.h"
#include "FMEncounter.h"      // EFMClass — ClassPortraits 키
#include "FMCharPanelWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
class UFMEquipSlotWidget;

/**
 * 캐릭터 창 (WBP_CharPanel의 부모) — 선택한 용병의 스탯·장비칸·공용 인벤토리.
 *
 * - 카드 클릭 → GameInstance.OnMercSelected → 이 창이 그 용병으로 열린다. 같은 카드를 다시 누르면 닫힌다.
 *   창은 HUD의 PanelSwitcher 안에 있으므로 부모 스위처를 직접 켜고 끈다.
 * - 인벤토리 줄을 장비 칸으로 끌어다 놓거나, 줄을 우클릭하면 장착(소비 아이템이면 사용).
 *   장비 칸이 아닌 창 아무 데나 떨어뜨려도 제 칸을 찾아 들어간다.
 * - 장비 칸 클릭: 해제 → 인벤토리로
 * - 장착 대상은 언제나 '지금 이 창이 보여주고 있는 용병'(CurrentMercId)이다.
 * - 변경은 전부 GameInstance가 처리하고 OnRosterChanged를 쏜다 → 이 창과 카드 목록이 함께 다시 그려진다.
 *
 * 레이아웃은 WBP에서 자유롭게 바꿔도 된다 — 아래 이름의 위젯만 있으면 된다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMCharPanelWidget : public UFMItemHostWidget
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

	/** 이 창은 '보여주고 있는 용병'이 곧 대상이다 */
	virtual int32 GetMercId() const override { return CurrentMercId; }

	UFUNCTION(BlueprintCallable, Category = "CharPanel")
	bool UnequipSlot(int32 EquipSlotValue);

protected:
	// ── 디자이너 위젯 (이름으로 연결) ──
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> TitleText;       // "전사"
	UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> StatText;        // "HP 130 / 170   공격력 37"

	// 장비 칸 — 좌우로 나눠 배치하면 페이퍼돌 모양이 된다
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UFMEquipSlotWidget> WeaponSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UFMEquipSlotWidget> ArmorSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UFMEquipSlotWidget> AccessorySlot;

	/** 가운데 캐릭터 그림 — ClassPortraits에서 가져온다 */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PortraitImage;

	// 구(舊) 글자만 있던 장비 칸 — 새 WBP로 옮기고 나면 지워도 된다
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton>    WeaponButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WeaponText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton>    ArmorButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ArmorText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton>    AccessoryButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> AccessoryText;

	/** 클래스별 캐릭터 그림 */
	UPROPERTY(EditDefaultsOnly, Category = "CharPanel")
	TMap<EFMClass, TObjectPtr<UTexture2D>> ClassPortraits;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 장비 칸 밖에 떨어뜨려도 제 칸을 찾아 들어가게
	virtual bool NativeOnDragOver(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Op) override;

	UFUNCTION() void HandleMercSelected(int32 MercId);
	UFUNCTION() void HandleRosterChanged() { Refresh(); }
	UFUNCTION() void HandleWeaponClicked();
	UFUNCTION() void HandleArmorClicked();
	UFUNCTION() void HandleAccessoryClicked();
};
