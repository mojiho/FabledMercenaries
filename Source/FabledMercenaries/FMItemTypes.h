#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sim/Item.h"
#include "FMItemTypes.generated.h"

/**
 * UI용 아이템 정보 — 전투 중 아이템 목록(SimManager)과 캐릭터 창 인벤토리(GameInstance)가 같이 쓴다.
 * 주의: 엔진 Slate(STreeView.h)에 이미 FItemInfo가 있어 이름 충돌 → FM 접두사 필수
 */

class UStaticMesh;
class UAnimMontage;
class UAnimSequenceBase;

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
	case ItemType::SwordAndShield: return TEXT("검과 방패");
	case ItemType::TwinSwords:   return TEXT("쌍검");
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

/** 무기 아이템 → 손에 들 메시·공격 모션. 화면 연출 전용 **/
USTRUCT(BlueprintType)
struct FFMWeaponVisual : public FTableRowBase
{
	GENERATED_BODY()

	/** ItemType 값 (IronSword = 2, SwordAndShield = 5, TwinSwords = 6) */
	UPROPERTY(EditAnywhere) int32 ItemId = 0;

	/** 오른손 메시 — SM_Free_* (BP_Free_* 말고) */
	UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Mesh;

	/** 오른손 소켓 */
	UPROPERTY(EditAnywhere) FName Socket = TEXT("weapon_r");

	UPROPERTY(EditAnywhere) float Scale = 1.f;

	/** 공격 몽타주 (DefaultSlot). AttackAnims가 있으면 그쪽이 우선. 둘 다 비우면 맨손 모션 */
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * 공격 동작 시퀀스 — 타격(AttackFired) 순간 DefaultSlot으로 재생. 여러 개면 공격할 때마다 차례로 (콤보).
	 * Kwang 같은 호환 스켈레톤의 시퀀스를 바로 넣을 수 있다 (UpperBody 슬롯을 쓰는 원본 몽타주는 못 씀)
	 */
	UPROPERTY(EditAnywhere) TArray<TObjectPtr<UAnimSequenceBase>> AttackAnims;

	/** 공격 동작 재생 속도 */
	UPROPERTY(EditAnywhere) float AttackAnimRate = 1.f;

	// ── 왼손 (세트 무기: 검+방패, 쌍검) — 비우면 왼손은 빈손 ──

	/** 왼손 메시 — 방패면 SM_Free_Shield_*, 쌍검이면 SM_Free_Sword_* */
	UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> OffhandMesh;

	/** 왼손 소켓 — 방패는 shield_l (팔뚝), 무기는 weapon_l (손) */
	UPROPERTY(EditAnywhere) FName OffhandSocket = TEXT("weapon_l");

	UPROPERTY(EditAnywhere) float OffhandScale = 1.f;
};