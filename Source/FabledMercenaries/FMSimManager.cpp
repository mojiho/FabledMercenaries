#include "FMSimManager.h"
#include "DrawDebugHelpers.h"

AFMSimManager::AFMSimManager()
{
	PrimaryActorTick.bCanEverTick = true;		// 매 프레임마다 Tick() 호출
}

void AFMSimManager::BeginPlay()
{
	Super::BeginPlay();
	Sim.AddCommander(1, CommanderType::Command);

	Class team[] = { Class::Warrior, Class::Mage, Class::Archer };
	for (int32 i = 0; i < 3; ++i)
	{
		Sim.AddUnit(100 + i, 1, Faction::Player, team[i], Vec3{ 0.f, (float)(i - 1) * 100.f, 0.f }, nullptr);
	};

	// 매니저가 실제로 도는지 + 유닛 몇 개 스폰됐는지
	UE_LOG(LogTemp, Warning, TEXT("[FM] SimManager BeginPlay, units=%d"), (int32)Sim.Units().size());
}

void AFMSimManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Sim.Tick(DeltaSeconds);			// Sim 한 스텝 진행
	// 디버그용 : 모든 유닛 위치 표시
	for (const auto& Pair : Sim.Units())
	{
		const Unit& U = Pair.second;
		FVector Loc(U.pos.x, U.pos.y, U.pos.z + GroundZ + 50.f);   // 바닥(GroundZ) 위로 올려 그림

		bool bSel = (Pair.first == SelectedUnitId);   // ◀ 선택됐나?

		FColor Col = bSel ? FColor::White              // 선택 = 흰색
			: (U.unitClass == Class::Warrior) ? FColor::Red
			: (U.unitClass == Class::Mage) ? FColor::Cyan
			: FColor::Green;
		float R = bSel ? 60.f : 40.f;                  // 선택 = 크게

		DrawDebugSphere(GetWorld(), Loc, R, 12, Col, false, -1.f, 0, 2.f);
	}
}

void AFMSimManager::IssueMoveCommand(uint64 UnitId, const FVector& WorldPos)
{
	Command mv;
	mv.type = CommandType::Move;
	// Z는 Sim 지면(0)으로 고정 — 클릭 지점 Z(바닥 높이)를 그대로 넣으면 유닛이 위로 떠버림
	mv.waypoints.push_back(Vec3{ (float)WorldPos.X, (float)WorldPos.Y, 0.f });
	Sim.IssueCommand(UnitId, mv, false);
}


// 클릭 지점 근처(반경 안) 유닛 중 가장 가까운 것의 id. 없으면 0.
uint64 AFMSimManager::FindUnitNear(const FVector& WorldPos, float Radius) const
{
	uint64 Best = 0;
	float  BestDist = Radius;
	for (const auto& Pair : Sim.Units())
	{
		const Unit& U = Pair.second;
		float D = FVector::Dist(WorldPos, FVector(U.pos.x, U.pos.y, U.pos.z));
		if (D < BestDist) { BestDist = D; Best = Pair.first; }   // Pair.first = 유닛 id
	}
	return Best;
}

void AFMSimManager::HandleClick(const FVector& WorldPos)
{
	uint64 HitUnit = FindUnitNear(WorldPos, 50.f);		// 클릭 지점 근처 유닛 찾기

	if (HitUnit != 0)
	{
		SelectedUnitId = HitUnit;		// 선택된 유닛 id 갱신
	}
	else
	{
		IssueMoveCommand(SelectedUnitId, WorldPos);	// 선택된 유닛이 있으면 클릭 지점으로 이동 명령
	}
}