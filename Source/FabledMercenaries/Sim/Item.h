#pragma once

#include <cstdint>

// <summary>
// 아이템 정의 (소비 + 장비). 인벤토리/수량은 메타(Meta/Player.h)가 갖고,
// Sim은 itemId → 효과 테이블만 안다(결정적 처리 = 서버/클라 동일).
// </summary>
enum class ItemCategory : uint8_t { Consumable, Equipment };
enum class ItemType : uint8_t { None, HealPotion, IronSword, LeatherArmor, PowerRing };
enum class EquipSlot : uint8_t { None, Weapon, Armor, Accessory };

constexpr int EQUIP_SLOT_COUNT = 3;   // Weapon/Armor/Accessory — 배열 인덱스는 (EquipSlot - 1)

struct ItemDef
{
	ItemCategory category  = ItemCategory::Consumable;
	EquipSlot    slot      = EquipSlot::None;
	float    amount    = 0.f;    // 회복량 등 효과 수치 (소비)
	float    atkBonus  = 0.f;    // 장착 시 공격력 + (장비)
	float    hpBonus   = 0.f;    // 장착 시 최대 체력 + (장비)
	float    preDelay  = 0.f;    // 마시는 모션(선딜)
	float    postDelay = 0.f;    // 경직(후딜)
};

// itemId는 ItemType 값과 1:1 (P0 단순화)
inline ItemDef GetItemDef(uint32_t itemId)
{
	ItemDef d;
	switch ((ItemType)itemId)
	{
	case ItemType::HealPotion:
		d.category  = ItemCategory::Consumable;
		d.amount    = 60.f;
		d.preDelay  = 0.3f;
		d.postDelay = 0.4f;
		break;
	case ItemType::IronSword:
		d.category = ItemCategory::Equipment;
		d.slot     = EquipSlot::Weapon;
		d.atkBonus = 8.f;
		break;
	case ItemType::LeatherArmor:
		d.category = ItemCategory::Equipment;
		d.slot     = EquipSlot::Armor;
		d.hpBonus  = 40.f;
		break;
	case ItemType::PowerRing:
		d.category = ItemCategory::Equipment;
		d.slot     = EquipSlot::Accessory;
		d.atkBonus = 4.f;
		d.hpBonus  = 15.f;
		break;
	default:
		break;
	}
	return d;
}

// 장비 합산 보너스 — 메타(UI 표시·체력 상한)와 Sim(전투 적용)이 같은 계산을 쓰도록 한 곳에 둔다
struct EquipBonus
{
	float atk = 0.f;
	float hp  = 0.f;
};

inline EquipBonus SumEquipBonus(const uint32_t (&equip)[EQUIP_SLOT_COUNT])
{
	EquipBonus b;
	for (uint32_t itemId : equip)
	{
		if (itemId == 0) continue;
		const ItemDef d = GetItemDef(itemId);
		b.atk += d.atkBonus;
		b.hp  += d.hpBonus;
	}
	return b;
}
