#pragma once

#include <cstdint>
#include <vector>
#include "Sim/Class.h"

// <summary>
// 메타 플레이어 데이터 — 전투 Sim 밖(로스터·재화·아이템).
// 설계 근거: docs/prototype_phase_p0_design.md — "메타 Player는 Sim 폴더 밖에 둔다(전투 Sim 순수성 유지)".
// Sim은 인벤토리를 모르고 itemId만 받는다. 수량 차감은 여기서.
// 용병 로스터도 여기 산다 — 전투 결과(HP/사망)가 기록되고, 다음 전투 스폰 때 그대로 복원된다.
// </summary>
struct ItemStack
{
	uint32_t itemId = 0;   // Sim/Item.h의 ItemType 값
	int32_t  count  = 0;
};

// 용병 1명 — 전투가 끝나도 남는 상태.
// 사망은 영구적이다가 월드맵 휴식(RestAll)으로만 되돌린다.
// (전투 중 힐로는 부활하지 않는다 — Sim의 Heal은 alive 유닛만 회복)
struct Mercenary
{
	uint32_t id        = 0;              // 로스터 내 고유 번호 (1부터)
	Class    unitClass = Class::None;
	float    maxHp     = 0.f;            // 고용 시점의 클래스 최대 체력
	float    hp        = 0.f;            // 지난 전투가 끝났을 때의 체력
	bool     alive     = true;
};

struct MetaPlayer
{
	uint64_t id = 0;                      // Commander.id(playerId)와 동일 키
	std::vector<ItemStack> inventory;
	std::vector<Mercenary> roster;        // 보유 용병 — 전투 시 이 순서대로 한 줄로 선다
	uint32_t nextMercId = 1;

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

	/** 용병 고용 — 풀체력으로 로스터 끝에 붙는다. 발급된 id 반환
	 *  (참조를 돌려주지 않는 이유: 이후 push_back 시 vector 재할당으로 무효화된다) */
	uint32_t Hire(Class cls)
	{
		Mercenary m;
		m.id        = nextMercId++;
		m.unitClass = cls;
		m.maxHp     = GetClassStats(cls).maxHp;
		m.hp        = m.maxHp;
		roster.push_back(m);
		return m.id;
	}

	Mercenary* FindMerc(uint32_t mercId)
	{
		for (auto& m : roster)
			if (m.id == mercId) return &m;
		return nullptr;
	}

	int32_t AliveMercCount() const
	{
		int32_t n = 0;
		for (const auto& m : roster)
			if (m.alive) ++n;
		return n;
	}

	/** 월드맵 휴식 — 사망자 부활 + 전원 체력 최대치 */
	void RestAll()
	{
		for (auto& m : roster)
		{
			m.alive = true;
			m.hp    = m.maxHp;
		}
	}
};
