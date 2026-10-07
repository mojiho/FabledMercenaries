#pragma once

#include "CoreMinimal.h"
#include "Sim/Item.h"
#include "FMItemTypes.generated.h"

/**
 * UI용 아이템 정보 — 전투 중 아이템 목록(SimManager)과 캐릭터 창 인벤토리(GameInstance)가 같이 쓴다.
 * 주의: 엔진 Slate(STreeView.h)에 이미 FItemInfo가 있어 이름 충돌 → FM 접두사 필수
 */
USTRUCT(BlueprintType)
struct FFMItemInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32   ItemId = 0;     // Sim ItemType 값. 0 = 빈 장비 칸
	UPROPERTY(BlueprintReadOnly) int32   Category = 0;   // 0=소비 1=장비 (아이콘/정렬용)
	UPROPERTY(BlueprintReadOnly) int32   Count = 0;
	UPROPERTY(BlueprintReadOnly) int32   Slot = 0;       // 장비 칸 (EquipSlot 값: 1=무기 2=방어구 3=장신구), 소비 아이템은 0
	UPROPERTY(BlueprintReadOnly) float   AtkBonus = 0.f;
	UPROPERTY(BlueprintReadOnly) float   HpBonus = 0.f;
};

/** 아이템 표시 이름 */
inline FString FMItemName(uint32 ItemId)
{
	switch ((ItemType)ItemId)
	{
	case ItemType::HealPotion:   return TEXT("회복 포션");
	case ItemType::IronSword:    return TEXT("철검");
	case ItemType::LeatherArmor: return TEXT("가죽 갑옷");
	case ItemType::PowerRing:    return TEXT("힘의 반지");
	default:                     return TEXT("아이템");
	}
}

/** 장비 칸 이름 (EquipSlot 값) */
inline FString FMSlotName(int32 Slot)
{
	switch ((EquipSlot)Slot)
	{
	case EquipSlot::Weapon:    return TEXT("무기");
	case EquipSlot::Armor:     return TEXT("방어구");
	case EquipSlot::Accessory: return TEXT("장신구");
	default:                   return TEXT("");
	}
}

/** Sim 아이템 정의 → UI 정보. ItemId가 0이면 빈 칸 */
inline FFMItemInfo FMMakeItemInfo(uint32 ItemId, int32 Count)
{
	FFMItemInfo Info;
	if (ItemId == 0) return Info;

	const ItemDef Def = GetItemDef(ItemId);
	Info.ItemId   = (int32)ItemId;
	Info.Count    = Count;
	Info.Name     = FMItemName(ItemId);
	Info.Category = (Def.category == ItemCategory::Equipment) ? 1 : 0;
	Info.Slot     = (int32)Def.slot;
	Info.AtkBonus = Def.atkBonus;
	Info.HpBonus  = Def.hpBonus;
	return Info;
}
