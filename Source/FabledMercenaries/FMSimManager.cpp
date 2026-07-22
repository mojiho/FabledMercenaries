#include "FMSimManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"          // ← GetWorld(), LineTraceSingleByChannel
#include "Engine/EngineTypes.h"    // ← ECC_Visibility, FHitResult
#include "CollisionQueryParams.h"  // ← 트레이스 파라미터
#include "Sim/AIBrain.h"

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
		Sim.AddUnit(100 + i, 1, Faction::Player, team[i], Vec3{ 0.f, (float)(i - 1) * 100.f, 0.f },
			std::make_unique<GuardBrain>());   // 명령 없을 때 근처 적에게 자동 반격
	};
	
	Sim.AddCommander(2, CommanderType::Command);   // 적 진영 지휘관 (owner=2) — 없으면 명령이 전부 Rejected
	// ── 적(Hostile) 스폰: 반대편에 3기, 브레인 없음(가만히 있는 표적) ──
	Class enemyTeam[] = { Class::Warrior, Class::Tanker, Class::Archer };
	for (int32 i = 0; i < 3; ++i)
	{
		Sim.AddUnit(200 + i, 2, Faction::Hostile, enemyTeam[i],
			Vec3{ 600.f, (float)(i - 1) * 100.f, 0.f },std::make_unique<ChaseAttackBrain>());
	}
	
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
		if (!U.alive) continue;   // 죽으면 화면에서 사라짐
		FVector Loc(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y) + 50.f);   // 실제 지형 높이 위로

		bool bSel = (Pair.first == SelectedUnitId);   // ◀ 선택됐나?

		FColor Col;
		if (bSel)
			Col = FColor::White;                        // 선택 = 흰색
		else if (U.faction == Faction::Hostile)
			Col = FColor(255, 60, 60);                  // 적 = 진한 빨강
		else                                            // 아군 = 클래스별
			Col = (U.unitClass == Class::Warrior) ? FColor::Orange
				: (U.unitClass == Class::Mage)    ? FColor::Cyan
				:                                    FColor::Green;
		float R = bSel ? 60.f : 40.f;                  // 선택 = 크게

		DrawDebugSphere(GetWorld(), Loc, R, 12, Col, false, -1.f, 0, 2.f);

		// 바라보는 방향 화살표 (도착 방향 확인용)
		FVector F(U.facing.x, U.facing.y, 0.f);
		if (!F.IsNearlyZero())
			DrawDebugDirectionalArrow(GetWorld(), Loc, Loc + F.GetSafeNormal() * 60.f,
				60.f, FColor::Yellow, false, -1.f, 0, 3.f);
		
		// HP 바 (구체 위, 월드 Y축 방향)
		const float BarW = 80.f;
		const float Ratio = FMath::Clamp(U.hp / U.maxHp, 0.f, 1.f);
		const FVector BarPos = Loc + FVector(0, 0, 70.f);          // 구체 위
		const FVector Left  = BarPos - FVector(0, BarW * 0.5f, 0);
		const FVector Right = BarPos + FVector(0, BarW * 0.5f, 0);
		const FVector Fill  = Left + FVector(0, BarW * Ratio, 0);
		DrawDebugLine(GetWorld(), Left, Right, FColor(40, 40, 40), false, -1.f, 0, 6.f);  // 배경(빈 체력)
		DrawDebugLine(GetWorld(), Left, Fill,  FColor::Green,      false, -1.f, 0, 6.f);  // 현재 체력
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
		// 탑다운 선택 → Z 무시하고 XY 평면 거리만 비교 (클릭은 바닥 Z, 유닛은 Sim z=0이라 3D로 하면 절대 안 잡힘)
		float D = FVector::Dist2D(WorldPos, FVector(U.pos.x, U.pos.y, U.pos.z));
		if (D < BestDist) { BestDist = D; Best = Pair.first; }   // Pair.first = 유닛 id
	}
	return Best;
}

void AFMSimManager::HandleClick(const FVector& WorldPos)
{
	SelectedUnitId = FindUnitNear(WorldPos, 50.f);   // 근처 유닛 선택, 없으면 0(해제)
	bRingHidden = false;
}

bool AFMSimManager::GetSelectedUnitWorldPos(FVector& OutPos) const
{
	if (SelectedUnitId == 0) return false;

	auto It = Sim.Units().find(SelectedUnitId);   // 선택 id로 유닛 찾기
	if (It == Sim.Units().end()) return false;    // (죽었거나 사라짐)

	const Unit& U = It->second;
	OutPos = FVector(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y) + 50.f);
	return true;
}


void AFMSimManager::ClearSelection()
{
	SelectedUnitId = 0;
	bRingHidden = false;
}

void AFMSimManager::MoveSelectedTo(const FVector& WorldPos)
{
	if (SelectedUnitId != 0)
	{
		IssueMoveCommand(SelectedUnitId, WorldPos);
		SelectedUnitId = 0;
		bRingHidden = false;
	}
}

float AFMSimManager::GroundZAt(float X, float Y) const
{
	FHitResult Hit;
	const FVector From(X, Y, GroundZ + 1000.f);
	const FVector To  (X, Y, GroundZ - 2000.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility))
		return Hit.Location.Z;
	return GroundZ;   // 못 찾으면 기본 바닥
}

void AFMSimManager::MoveSelectedAlong(const TArray<FVector>& Waypoints, const FVector& ArriveFacing, bool bHasFacing)
{
	if (SelectedUnitId == 0 || Waypoints.Num() == 0) return;

	Command mv;
	mv.type = CommandType::Move;
	for (const FVector& WP : Waypoints)
		mv.waypoints.push_back(Vec3{ (float)WP.X, (float)WP.Y, 0.f });   // Z는 Sim 지면(0)

	if (bHasFacing)   // 드래그로 도착 방향을 지정했으면 실어 보냄
	{
		mv.arriveFacing = Vec3{ (float)ArriveFacing.X, (float)ArriveFacing.Y, 0.f };
		mv.hasArriveFacing = true;
	}

	Sim.IssueCommand(SelectedUnitId, mv, false);
	SelectedUnitId = 0;
	bRingHidden = false;
}

uint64 AFMSimManager::FindEnemyNear(const FVector& WorldPos, float Radius) const
{
	uint64 Best = 0;
	float  BestDist = Radius;
	for (const auto& Pair : Sim.Units())
	{
		const Unit& U = Pair.second;
		if (U.faction != Faction::Hostile || !U.alive) continue;   // 적 + 살아있음만
		float D = FVector::Dist2D(WorldPos, FVector(U.pos.x, U.pos.y, U.pos.z));
		if (D < BestDist) { BestDist = D; Best = Pair.first; }
	}
	return Best;
}

void AFMSimManager::AttackTarget(uint64 TargetId)
{
	if (SelectedUnitId == 0 || TargetId == 0) return;

	Command atk;
	atk.type = CommandType::Attack;
	atk.targetId = TargetId;

	Sim.IssueCommand(SelectedUnitId, atk, false);
	SelectedUnitId = 0;
	bRingHidden = false;
}

bool AFMSimManager::GetUnitWorldPos(uint64 Id, FVector& OutPos) const
{
	auto It = Sim.Units().find(Id);
	if (It == Sim.Units().end() || !It->second.alive) return false;
	const Unit& U = It->second;
	OutPos = FVector(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y));
	return true;
}