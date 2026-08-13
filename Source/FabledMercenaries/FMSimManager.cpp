#include "FMSimManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"          // ← GetWorld(), LineTraceSingleByChannel
#include "Engine/EngineTypes.h"    // ← ECC_Visibility, FHitResult
#include "CollisionQueryParams.h"  // ← 트레이스 파라미터
#include "Sim/AIBrain.h"
#include "Sim/Item.h"
#include "FMUnit.h"

static EUnitAnim ToAnim(ActionState s)
{
	switch (s)
	{
	case ActionState::Moving:        return EUnitAnim::Move;
	case ActionState::AttackWindup:
	case ActionState::AttackRecover: return EUnitAnim::Attack;
	case ActionState::Casting:       return EUnitAnim::Cast;
	case ActionState::Defending:     return EUnitAnim::Defend;
	case ActionState::Focusing:      return EUnitAnim::Focus;
	case ActionState::Stunned:       return EUnitAnim::Stun;
	case ActionState::Dead:          return EUnitAnim::Dead;
	default:                         return EUnitAnim::Idle;
	}
}


AFMSimManager::AFMSimManager()
{
	PrimaryActorTick.bCanEverTick = true;		// 매 프레임마다 Tick() 호출
}

void AFMSimManager::BeginPlay()
{
	Super::BeginPlay();
	Sim.AddCommander(1, CommanderType::Command);

	// 메타 인벤토리 초기 지급 (P0 테스트용 — 나중에 로스터/상점으로 대체)
	Meta.id = 1;
	Meta.Add((uint32)ItemType::HealPotion, 5);

	Class team[] = { Class::Warrior, Class::Mage, Class::Archer };
	for (int32 i = 0; i < 3; ++i)
	{
		Sim.AddUnit(100 + i, 1, Faction::Player, team[i], Vec3{ 0.f, (float)(i - 1) * 100.f, 0.f },
			std::make_unique<GuardBrain>());   // 명령 없을 때 근처 적에게 자동 반격
	};
	
	Sim.AddCommander(2, CommanderType::Command);   // 적 진영 지휘관 (owner=2) — 없으면 명령이 전부 Rejected
	// ── 적(Hostile) 스폰: 반대편에 3기
	Class enemyTeam[] = { Class::Warrior, Class::Tanker, Class::Archer };
	for (int32 i = 0; i < 3; ++i)
	{
		Sim.AddUnit(200 + i, 2, Faction::Hostile, enemyTeam[i],
			Vec3{ 600.f, (float)(i - 1) * 100.f, 0.f },std::make_unique<ChaseAttackBrain>());
	}
	
	// Sim 유닛마다 화면 액터 하나씩 스폰
	if (UnitClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;  // 겹쳐도 무조건 스폰
		for (const auto& Pair : Sim.Units())
		{
			AFMUnit* A = GetWorld()->SpawnActor<AFMUnit>(UnitClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (A) UnitActors.Add(Pair.first, A);
		}
		UE_LOG(LogTemp, Warning, TEXT("[FM] 유닛 액터 스폰: %d개 (UnitClass OK)"), UnitActors.Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FM] UnitClass가 None! FMSimManager 디테일에서 Unit Class를 BP_Unit으로 지정하세요."));
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

		// 화면 액터 갱신 — 죽어도 실행(Dead 애님 전달 → 쓰러지는 모션, 시체 남음)
		if (TObjectPtr<AFMUnit>* Found = UnitActors.Find(Pair.first))
		{
			FVector FloorLoc(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y));
			FVector FacingDir(U.facing.x, U.facing.y, 0.f);
			(*Found)->UpdateFromSim(FloorLoc, FacingDir, ToAnim(U.GetActionState()), DeltaSeconds);
		}

		if (!U.alive) continue;   // 죽은 유닛은 아래 UI(링/HP/MP/화살표) 생략

		FVector Loc(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y) + 50.f);   // 실제 지형 높이 위로
		bool bSel = (Pair.first == SelectedUnitId);   // ◀ 선택됐나?

		// 발밑 진영 링: 아군=하늘색, 적=빨강, 선택=흰색·굵게
		const FColor TeamCol = (U.faction == Faction::Hostile) ? FColor(255, 60, 60) : FColor(60, 160, 255);
		const FColor RingCol = bSel ? FColor::White : TeamCol;
		const float  Thick   = bSel ? 6.f : 3.f;
		const FVector Feet    = Loc - FVector(0, 0, 45.f);   // 바닥 근처 (Loc은 +50이라 -45 = +5)
		DrawDebugCircle(GetWorld(), Feet, 55.f, 32, RingCol, false, -1.f, 0, Thick,
			FVector(1, 0, 0), FVector(0, 1, 0), false);   // XY 평면(바닥에 눕힘)
		
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
		
		const float MpRatio = (U.mpMax > 0.f) ? FMath::Clamp(U.mp / U.mpMax, 0.f, 1.f) : 0.f;
		const FVector MpBase = Loc + FVector(0, 0, 60.f);   // HP바(70)보다 살짝 아래
		DrawDebugLine(GetWorld(), MpBase - FVector(0, BarW*0.5f, 0), MpBase + FVector(0, BarW*0.5f, 0), FColor(30,30,60), false, -1.f, 0, 5.f);
		DrawDebugLine(GetWorld(), MpBase - FVector(0, BarW*0.5f, 0), MpBase - FVector(0, BarW*0.5f, 0) + FVector(0, BarW*MpRatio, 0), FColor::Blue, false, -1.f, 0, 5.f);

	}

	// 투사체 그리기 (화살=베이지, 마법=보라). pos.z는 포물선 높이(arc)
	for (const Projectile& P : Sim.Projectiles())
	{
		if (!P.alive) continue;
		const FVector PLoc(P.pos.x, P.pos.y, GroundZAt(P.pos.x, P.pos.y) + 80.f + P.pos.z);  // 몸통 높이 + 포물선
		const FColor  PCol = P.arc ? FColor(210, 190, 130) : FColor(180, 90, 255);
		DrawDebugSphere(GetWorld(), PLoc, 12.f, 8, PCol, false, -1.f, 0, 1.5f);
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

void AFMSimManager::CastSkill(int32 SkillType, uint64 TargetId)
{
	if (SelectedUnitId == 0) return;

	Command c;
	c.type = CommandType::Skill;
	c.skillId = (uint32)SkillType;   // Sim SkillType 값
	c.targetId = TargetId;           // 대상(공격=적, 힐=아군, 자기강화=자신)

	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;
	bRingHidden = false;
}

bool AFMSimManager::ActivateItem(int32 ItemId)
{
	const ItemDef Def = GetItemDef((uint32)ItemId);
	switch (Def.category)
	{
	case ItemCategory::Consumable: return UseConsumable(ItemId);   // 지금 코드 그대로 옮김
	case ItemCategory::Equipment:  return EquipItem(ItemId);       // 지금은 로그만 찍고 false
	}
	return false;
}

bool AFMSimManager::UseConsumable(int32 ItemId)
{
	if (SelectedUnitId == 0) return false;

	const Unit* U = Sim.GetUnit(SelectedUnitId);
	if (!U || !U->alive) return false;

	if (!Meta.Consume((uint32)ItemId))   // 재고 없음
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] 아이템 부족 (id=%d)"), ItemId);
		return false;
	}

	Command c;
	c.type   = CommandType::Item;
	c.itemId = (uint32)ItemId;
	c.targetId = 0;                      // 0 = 자신에게 사용

	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;
	bRingHidden = false;
	return true;
}

bool AFMSimManager::EquipItem(int32 ItemId)
{
	return false;
}


TArray<FFMItemInfo> AFMSimManager::GetInventory() const
{
	TArray<FFMItemInfo> Out;
	for (const ItemStack& S : Meta.inventory)
	{
		if (S.count <= 0) continue;

		FFMItemInfo Info;
		Info.ItemId = (int32)S.itemId;
		Info.Count  = S.count;
		Info.Name   = ((ItemType)S.itemId == ItemType::HealPotion) ? TEXT("회복 포션") : TEXT("아이템");
		Out.Add(Info);
	}
	return Out;
}

bool AFMSimManager::GetUnitWorldPos(uint64 Id, FVector& OutPos) const
{
	auto It = Sim.Units().find(Id);
	if (It == Sim.Units().end() || !It->second.alive) return false;
	const Unit& U = It->second;
	OutPos = FVector(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y));
	return true;
}

void AFMSimManager::IssueDefendSelected()
{
	if (SelectedUnitId == 0) return;
	Command c;
	c.type = CommandType::Defend;
	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;   // 명령 후 선택 해제 → 링 사라짐
	bRingHidden = false;
}

void AFMSimManager::IssueStopSelected()
{
	if (SelectedUnitId == 0) return;
	Command c;
	c.type = CommandType::Stop;
	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;   // 명령 후 선택 해제 → 링 사라짐
	bRingHidden = false;
}
                  

void AFMSimManager::IssueFocusSelected()
{
	if (SelectedUnitId == 0) return;
	Command c;
	c.type = CommandType::Focus;
	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;   // 명령 후 선택 해제 → 링 사라짐
	bRingHidden = false;
}

TArray<FSkillInfo> AFMSimManager::GetSelectedUnitSkills() const
{
	TArray<FSkillInfo> Out;
	if (SelectedUnitId == 0) return Out;

	auto It = Sim.Units().find(SelectedUnitId);
	if (It == Sim.Units().end()) return Out;

	for (const Skill& s : It->second.skills)
	{
		if (s.category != SkillCategory::Active) continue;   // 시전 가능한 액티브만

		FSkillInfo Info;
		Info.SkillType   = (int32)s.type;
		Info.MpCost      = s.mpCost;
		Info.Cooldown    = s.cooldown;
		Info.CdRemaining = s.cdRemaining;
		switch (s.type)
		{
		case SkillType::Charge:    Info.Name = TEXT("돌진");     break;
		case SkillType::MagicBolt: Info.Name = TEXT("마법탄");   break;
		case SkillType::Heal:      Info.Name = TEXT("힐");       break;
		case SkillType::Defense:   Info.Name = TEXT("방어 태세"); break;
		default:                   Info.Name = TEXT("스킬");     break;
		}
		Out.Add(Info);
	}
	return Out;
}