#include "FMGameInstance.h"
#include "Sim/Item.h"

namespace
{
	const TCHAR* ClassLabel(Class C)
	{
		switch (C)
		{
		case Class::Warrior:  return TEXT("전사");
		case Class::Tanker:   return TEXT("탱커");
		case Class::Mage:     return TEXT("마법사");
		case Class::Archer:   return TEXT("궁수");
		case Class::Assassin: return TEXT("암살자");
		case Class::Healer:   return TEXT("힐러");
		default:              return TEXT("?");
		}
	}
}


FFMMercInfo UFMGameInstance::MakeMercInfo(const Mercenary& M)
{
	FFMMercInfo I;
	I.MercId    = (int32)M.id;
	I.UnitClass = FromSimClass(M.unitClass);
	I.ClassName = FText::FromString(ClassLabel(M.unitClass));
	I.Hp        = M.hp;
	I.MaxHp     = M.TotalMaxHp();
	I.bAlive    = M.alive;
	I.Attack    = M.TotalAttack();
	I.Weapon    = FMMakeItemInfo(M.equip[(int)EquipSlot::Weapon    - 1], 1);
	I.Armor     = FMMakeItemInfo(M.equip[(int)EquipSlot::Armor     - 1], 1);
	I.Accessory = FMMakeItemInfo(M.equip[(int)EquipSlot::Accessory - 1], 1);
	return I;
}

TArray<FFMMercInfo> UFMGameInstance::GetRoster() const
{
	TArray<FFMMercInfo> Out;
	for (const Mercenary& M : Meta.roster)
		Out.Add(MakeMercInfo(M));
	return Out;
}

FFMMercInfo UFMGameInstance::GetMercInfo(int32 MercId, bool& bFound) const
{
	for (const Mercenary& M : Meta.roster)
		if ((int32)M.id == MercId) { bFound = true; return MakeMercInfo(M); }
	bFound = false;
	return FFMMercInfo();
}

TArray<FFMItemInfo> UFMGameInstance::GetInventoryItems() const
{
	TArray<FFMItemInfo> Out;
	for (const ItemStack& S : Meta.inventory)
		if (S.count > 0) Out.Add(FMMakeItemInfo(S.itemId, S.count));
	return Out;
}

bool UFMGameInstance::UseItemOnMerc(int32 MercId, int32 ItemId)
{
	const bool bEquip = GetItemDef((uint32)ItemId).category == ItemCategory::Equipment;
	const bool bOk = bEquip ? Meta.Equip((uint32)MercId, (uint32)ItemId)
	                        : Meta.UseOnMerc((uint32)MercId, (uint32)ItemId);

	UE_LOG(LogTemp, Warning, TEXT("[FM] %s %s → 용병 #%d : %s"),
		*FMItemName((uint32)ItemId), bEquip ? TEXT("장착") : TEXT("사용"), MercId, bOk ? TEXT("성공") : TEXT("실패"));
	if (bOk)
	{
		LogRoster(TEXT("아이템 사용 후"));
		OnRosterChanged.Broadcast();
	}
	return bOk;
}

bool UFMGameInstance::UnequipMercSlot(int32 MercId, int32 Slot)
{
	const bool bOk = Meta.Unequip((uint32)MercId, (EquipSlot)Slot);
	UE_LOG(LogTemp, Warning, TEXT("[FM] %s 해제 → 용병 #%d : %s"), *FMSlotName(Slot), MercId, bOk ? TEXT("성공") : TEXT("실패"));
	if (bOk)
	{
		LogRoster(TEXT("장비 해제 후"));
		OnRosterChanged.Broadcast();
	}
	return bOk;
}
void UFMGameInstance::Init()
{
	Super::Init();

	// 초기 지급 — 예전엔 FMSimManager::BeginPlay에 있었다.
	// 레벨마다 다시 실행되면 컴뱃맵에 들어갈 때마다 포션이 리필되므로 여기로 옮겼다.
	Meta.id = 1;
	Meta.Add((uint32)ItemType::HealPotion, 5);
	// 시험용 장비 — 나중에 상점/전리품으로 대체
	Meta.Add((uint32)ItemType::IronSword, 2);
	Meta.Add((uint32)ItemType::LeatherArmor, 1);
	Meta.Add((uint32)ItemType::PowerRing, 1);

	// 초기 용병단 — 나중에 고용/상점으로 대체. 전투엔 이 순서대로 한 줄로 선다.
	Meta.Hire(Class::Warrior);
	Meta.Hire(Class::Mage);
	Meta.Hire(Class::Archer);
	Meta.Hire(Class::Healer);

	UE_LOG(LogTemp, Warning, TEXT("[FM] GameInstance Init — 메타 데이터 준비"));
	LogRoster(TEXT("초기 편성"));
}

void UFMGameInstance::RestAllMercenaries()
{
	const int32 Total   = (int32)Meta.roster.size();
	const int32 Revived = Total - Meta.AliveMercCount();

	Meta.RestAll();

	UE_LOG(LogTemp, Warning, TEXT("[FM] 휴식 — 용병 %d명 전원 회복 (부활 %d명)"), Total, Revived);
	LogRoster(TEXT("휴식 후"));
	
	OnRosterChanged.Broadcast();
}

void UFMGameInstance::LogRoster(const TCHAR* Context) const
{
	FString Line;
	for (const Mercenary& M : Meta.roster)
	{
		const FString HpText = M.alive
			? FString::Printf(TEXT("%.0f/%.0f 공%.0f"), M.hp, M.TotalMaxHp(), M.TotalAttack())
			: FString(TEXT("사망"));
		Line += FString::Printf(TEXT(" [#%u %s %s]"), M.id, ClassLabel(M.unitClass), *HpText);
	}
	UE_LOG(LogTemp, Warning, TEXT("[FM] 로스터(%s):%s"), Context, *Line);
}
