#pragma once

#include <cstdint>

// <summary>
// 소비 아이템 정의. 인벤토리/수량은 메타(Meta/Player.h)가 갖고,
// Sim은 itemId → 효과 테이블만 안다(결정적 처리 = 서버/클라 동일).
// </summary>
enum class ItemCategory : uint8_t { Consumable, Equipment };
enum class ItemType : uint8_t { None, HealPotion };
enum class EquipSlot : uint8_t { None, Weapon, Amor, Accessory };

struct ItemDef
{
	ItemCategory category  = ItemCategory::Consumable;
	EquipSlot    slot      = EquipSlot::None;
	float    amount    = 0.f;    // 회복량 등 효과 수치
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
	default:
		break;
	}
	return d;
}
