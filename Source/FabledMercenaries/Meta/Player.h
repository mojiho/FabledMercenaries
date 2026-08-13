#pragma once

#include <cstdint>
#include <vector>

// <summary>
// 메타 플레이어 데이터 — 전투 Sim 밖(로스터·재화·아이템).
// 설계 근거: docs/prototype_phase_p0_design.md — "메타 Player는 Sim 폴더 밖에 둔다(전투 Sim 순수성 유지)".
// Sim은 인벤토리를 모르고 itemId만 받는다. 수량 차감은 여기서.
// P0에서는 인벤토리만 있으면 충분 — 재화/로스터는 골격만 잡고 비워둔다.
// </summary>
struct ItemStack
{
	uint32_t itemId = 0;   // Sim/Item.h의 ItemType 값
	int32_t  count  = 0;
};

struct MetaPlayer
{
	uint64_t id = 0;                      // Commander.id(playerId)와 동일 키
	std::vector<ItemStack> inventory;

	int32_t CountOf(uint32_t itemId) const
	{
		for (const auto& s : inventory)
			if (s.itemId == itemId) return s.count;
		return 0;
	}

	void Add(uint32_t itemId, int32_t n)
	{
		for (auto& s : inventory)
			if (s.itemId == itemId) { s.count += n; return; }
		inventory.push_back(ItemStack{ itemId, n });
	}

	/** 1개 소모. 재고 없으면 false */
	bool Consume(uint32_t itemId)
	{
		for (auto& s : inventory)
			if (s.itemId == itemId && s.count > 0) { --s.count; return true; }
		return false;
	}
};
