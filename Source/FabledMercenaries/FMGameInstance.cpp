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


TArray<FFMMercInfo> UFMGameInstance::GetRoster() const
{
	TArray<FFMMercInfo> Out;
	for (const Mercenary& M : Meta.roster)
	{
		FFMMercInfo I;
		I.MercId    = (int32)M.id;
		I.UnitClass = FromSimClass(M.unitClass);
		I.ClassName = FText::FromString(ClassLabel(M.unitClass));
		I.Hp        = M.hp;
		I.MaxHp     = M.maxHp;
		I.bAlive    = M.alive;
		Out.Add(I);
	}
	return Out;
	
}
void UFMGameInstance::Init()
{
	Super::Init();

	// 초기 지급 — 예전엔 FMSimManager::BeginPlay에 있었다.
	// 레벨마다 다시 실행되면 컴뱃맵에 들어갈 때마다 포션이 리필되므로 여기로 옮겼다.
	Meta.id = 1;
	Meta.Add((uint32)ItemType::HealPotion, 5);

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
			? FString::Printf(TEXT("%.0f/%.0f"), M.hp, M.maxHp)
			: FString(TEXT("사망"));
		Line += FString::Printf(TEXT(" [#%u %s %s]"), M.id, ClassLabel(M.unitClass), *HpText);
	}
	UE_LOG(LogTemp, Warning, TEXT("[FM] 로스터(%s):%s"), Context, *Line);
}
