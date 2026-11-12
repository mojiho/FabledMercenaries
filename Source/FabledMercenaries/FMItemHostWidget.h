#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMItemTypes.h"
#include "FMItemHostWidget.generated.h"

class UPanelWidget;
class UFMInvItemWidget;

/**
 * 인벤토리 줄(WBP_InvItem)을 담는 창의 공통 부모.
 *
 * 캐릭터 창(UFMCharPanelWidget)과 인벤토리 창(UFMInvPanelWidget)이 같은 줄 위젯을 쓰므로,
 * 줄이 자기를 담은 창을 찾아 아이콘·대상 용병·장착을 물어볼 수 있도록 공통 창구를 여기에 둔다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMItemHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 창이 아이템을 쓸 대상 용병. 기본은 GameInstance가 기억하는 '지금 고른 용병' */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual int32 GetMercId() const;

	/** 장비면 장착(기존 장비는 반납), 소비면 사용. 성공하면 OnRosterChanged로 창이 다시 그려진다 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool EquipOn(int32 MercId, int32 ItemId);

	/** ItemIcons에 등록된 그림. 없으면 nullptr */
	UTexture2D* GetItemIcon(int32 ItemId) const;

	/** 자식 줄·칸이 자기를 담고 있는 창을 찾는 길 (디자이너 배치든 런타임 생성이든 Outer를 거슬러 올라간다) */
	static UFMItemHostWidget* FindOwnerHost(const UWidget* Widget);

protected:
	/** 인벤토리 줄이 들어갈 곳 (ScrollBox 등). 장비창처럼 목록이 없는 창은 비워 둔다 */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> InvList;

	/** 인벤토리 한 줄로 쓸 위젯 (WBP_InvItem) */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UFMInvItemWidget> InvItemClass;

	/** 아이템 그림 — 키는 ItemType 값 (1=회복포션 2=철검 3=가죽갑옷 4=힘의반지 5=검과방패 6=쌍검) */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TMap<int32, TObjectPtr<UTexture2D>> ItemIcons;

	/** InvList를 지금 인벤토리로 다시 채운다 */
	void RebuildInventory(const TArray<FFMItemInfo>& Items);

	class UFMGameInstance* GetFMGI() const;
};
