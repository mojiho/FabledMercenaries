#include "FMSimManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"          // ← GetWorld(), LineTraceSingleByChannel
#include "Engine/EngineTypes.h"    // ← ECC_Visibility, FHitResult
#include "CollisionQueryParams.h"  // ← 트레이스 파라미터
#include "Sim/AIBrain.h"
#include "Sim/Item.h"
#include "FMUnit.h"
#include "FMGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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

	// 기존에 BeginPlay에 하드코딩돼 있던 3v3을 그대로 데이터로 옮긴 것.
	// 심볼 인카운터가 붙기 전까지 전투를 바로 띄워보는 용도.
	DebugEncounter.EncounterId = TEXT("Debug_3v3");
	DebugEncounter.Separation  = 600.f;

	auto Add = [](TArray<FFMUnitSpawn>& Out, EFMClass C, float Y)
	{
		FFMUnitSpawn S;
		S.UnitClass = C;
		S.Offset    = FVector2D(0.f, Y);
		Out.Add(S);
	};
	Add(DebugEncounter.Allies,  EFMClass::Warrior, -100.f);
	Add(DebugEncounter.Allies,  EFMClass::Mage,       0.f);
	Add(DebugEncounter.Allies,  EFMClass::Archer,   100.f);
	Add(DebugEncounter.Enemies, EFMClass::Warrior, -100.f);
	Add(DebugEncounter.Enemies, EFMClass::Tanker,     0.f);
	Add(DebugEncounter.Enemies, EFMClass::Archer,   100.f);
}

MetaPlayer* AFMSimManager::MetaPtr() const
{
	UWorld* W = GetWorld();
	UFMGameInstance* GI = W ? W->GetGameInstance<UFMGameInstance>() : nullptr;
	if (!GI)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[FM] GameInstance가 UFMGameInstance가 아닙니다 — 프로젝트 세팅 > 맵&모드 > Game Instance Class를 FMGameInstance로 지정하세요. 인벤토리가 비어 보입니다."));
		return nullptr;
	}
	return &GI->GetMeta();
}

void AFMSimManager::BeginPlay()
{
	Super::BeginPlay();
	BindSimCallbacks();   // 유닛 등록보다 먼저 — 첫 Tick 전에 콜백이 걸려 있어야 함

	// 지휘관은 레벨 수명 내내 유지된다 (유닛만 인카운터마다 들락날락).
	// 적 지휘관이 없으면 적의 명령이 전부 Rejected 되므로 여기서 같이 등록.
	Sim.AddCommander(1, CommanderType::Command);
	Sim.AddCommander(ENEMY_COMMANDER_ID, CommanderType::Command);

	if (!UnitClass)
		UE_LOG(LogTemp, Error, TEXT("[FM] UnitClass가 None! FMSimManager 디테일에서 Unit Class를 BP_Unit으로 지정하세요."));

	if (bSpawnExplorationAvatar)
	{
		// 매니저 액터가 놓인 자리에서 출발. 전투 유닛과 달리 CombatUnitIds에 넣지 않는다.
		const FVector Home = GetActorLocation();
		AvatarUnitId = SpawnSimUnit(EFMClass::Warrior, 1, Faction::Player, FVector2D(Home.X, Home.Y));
		UE_LOG(LogTemp, Warning, TEXT("[FM] 탐험 아바타 스폰 id=%llu"), AvatarUnitId);
	}

	if (bAutoStartDebugEncounter)
	{
		// 예전 배치를 그대로 재현: 아군 x=0, 적 x=600 → 중점 x=300
		StartEncounter(DebugEncounter, FVector(300.f, 0.f, GroundZ), FVector::ForwardVector);
	}

	UE_LOG(LogTemp, Warning, TEXT("[FM] SimManager BeginPlay, units=%d"), (int32)Sim.Units().size());
}

void AFMSimManager::BindSimCallbacks()
{
	Sim.OnAttackFired = [this](uint64_t Id)
	{ PendingEvents.Add({ ESimEvt::AttackFired, Id }); };

	Sim.OnDamaged = [this](uint64_t Id, bool bBehind, bool bCrit)
	{ PendingEvents.Add({ ESimEvt::Damaged, Id, 0, bBehind, bCrit }); };

	Sim.OnDeath = [this](uint64_t Id)
	{ PendingEvents.Add({ ESimEvt::Death, Id }); };

	Sim.OnSkillCast = [this](uint64_t Id, SkillType T)
	{ PendingEvents.Add({ ESimEvt::SkillCast, Id, (int32)T }); };

	Sim.OnCommandComplete = [this](uint64_t Id, uint32_t Slot)
	{ PendingEvents.Add({ ESimEvt::CmdComplete, Id, (int32)Slot }); };
}

void AFMSimManager::DrainSimEvents()
{
	for (const FSimEvent& E : PendingEvents)
	{
		TObjectPtr<AFMUnit>* Found = UnitActors.Find(E.UnitId);
		if (!Found || !*Found) continue;
		AFMUnit* A = Found->Get();

		switch (E.Kind)
		{
		case ESimEvt::AttackFired: A->NotifyAttackFired();                    break;
		case ESimEvt::Damaged:     A->NotifyDamaged(E.bFromBehind, E.bCrit);  break;
		case ESimEvt::Death:       A->NotifyDeath();                          break;
		case ESimEvt::SkillCast:   A->NotifySkillCast(E.Param);               break;
		case ESimEvt::CmdComplete: /* 지금은 무시 (나중에 UI 피드백) */       break;
		}
	}
	PendingEvents.Reset();
}

// ─────────────────────────────────────────────────────────────────────────────
//  인카운터 — 탐험(아바타만) ↔ 전투(용병+적 스폰)
// ─────────────────────────────────────────────────────────────────────────────

uint64 AFMSimManager::SpawnSimUnit(EFMClass Cls, uint64 OwnerId, Faction Fac, const FVector2D& PlanarPos)
{
	const uint64 Id = NextUnitId++;

	Sim.AddUnit(Id, OwnerId, Fac, ToSimClass(Cls),
		Vec3{ (float)PlanarPos.X, (float)PlanarPos.Y, 0.f },   // Z는 Sim 지면(0) 고정
		std::make_unique<GuardBrain>());                        // 명령 없을 때 근처 적에게 자동 반격

	if (UnitClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AFMUnit* A = GetWorld()->SpawnActor<AFMUnit>(UnitClass, FVector::ZeroVector, FRotator::ZeroRotator, Params))
			UnitActors.Add(Id, A);
	}
	return Id;
}

bool AFMSimManager::GetAvatarWorldPos(FVector& OutPos) const
{
	auto It = Sim.Units().find(AvatarUnitId);
	if (It == Sim.Units().end() || !It->second.alive) return false;

	const Unit& U = It->second;
	OutPos = FVector(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y));
	return true;
}

void AFMSimManager::SetAvatarWorldPos(const FVector& WorldPos)
{
	if (Unit* U = Sim.GetUnit(AvatarUnitId))
	{
		// Z는 Sim 지면(0) 고정 — 월드 Z를 그대로 넣으면 유닛이 공중에 뜬다
		U->pos = Vec3{ (float)WorldPos.X, (float)WorldPos.Y, 0.f };
		U->executing = false;          // 이동 중이었다면 취소 — 순간이동 후 원래 목적지로 되돌아가면 안 된다
		U->reserveQueue.clear();
	}
}

void AFMSimManager::ReturnToWorldMap()
{
	UFMGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UFMGameInstance>() : nullptr;
	if (!GI || GI->ReturnWorldMap.IsNone())
	{
		// 월드맵을 거치지 않고 컴뱃맵에서 바로 플레이한 경우 — 돌아갈 곳이 없다
		UE_LOG(LogTemp, Warning, TEXT("[FM] 복귀할 월드맵이 없습니다 — 컴뱃맵에 그대로 남습니다"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[FM] 월드맵 복귀 → %s (노드 %s)"),
		*GI->ReturnWorldMap.ToString(), *GI->LastWorldNodeId.ToString());

	UGameplayStatics::OpenLevel(this, GI->ReturnWorldMap);
}

void AFMSimManager::StartEncounter(const FFMEncounterDef& Def, const FVector& Center, const FVector& FacingDir)
{
	if (bInCombat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] 이미 전투 중 — 인카운터 '%s' 무시"), *Def.EncounterId.ToString());
		return;
	}

	// 아군이 바라볼 방향. 0벡터가 들어오면(심볼과 아바타가 겹친 경우) +X로 뭉갠다.
	FVector Fwd = FacingDir.GetSafeNormal2D();
	if (Fwd.IsNearlyZero()) Fwd = FVector::ForwardVector;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);   // UE 기준 오른쪽(+Y)

	const float   Half      = Def.Separation * 0.5f;
	const FVector AllyBase  = Center - Fwd * Half;   // 조우 지점에서 절반씩 물러선다
	const FVector EnemyBase = Center + Fwd * Half;

	// Offset.X = 상대 진영 쪽으로 나아간 거리, Offset.Y = 좌우(양 진영 공통 축, 미러링 없음)
	auto Place = [&Right](const FVector& Base, const FVector& Toward, const FFMUnitSpawn& S)
	{
		const FVector P = Base + Toward * S.Offset.X + Right * S.Offset.Y;
		return FVector2D(P.X, P.Y);
	};

	for (const FFMUnitSpawn& S : Def.Allies)
		CombatUnitIds.Add(SpawnSimUnit(S.UnitClass, 1, Faction::Player, Place(AllyBase, Fwd, S)));

	for (const FFMUnitSpawn& S : Def.Enemies)
		CombatUnitIds.Add(SpawnSimUnit(S.UnitClass, ENEMY_COMMANDER_ID, Faction::Hostile, Place(EnemyBase, -Fwd, S)));

	bInCombat = true;

	UE_LOG(LogTemp, Warning, TEXT("[FM] 인카운터 시작 '%s' — 아군 %d기, 적 %d기 (중점 %s)"),
		*Def.EncounterId.ToString(), Def.Allies.Num(), Def.Enemies.Num(), *Center.ToCompactString());
}

void AFMSimManager::EndEncounter()
{
	if (!bInCombat) return;

	for (uint64 Id : CombatUnitIds)
	{
		Sim.RemoveUnit(Id);

		if (TObjectPtr<AFMUnit>* Found = UnitActors.Find(Id))
		{
			if (AFMUnit* A = Found->Get()) A->Destroy();
			UnitActors.Remove(Id);
		}
	}
	CombatUnitIds.Reset();

	// 사라진 유닛을 가리키는 이벤트가 다음 프레임에 소진되면 안 된다
	PendingEvents.Reset();

	SelectedUnitId = 0;
	bTargeting     = false;
	bMenuOpen      = false;
	bInCombat      = false;

	UE_LOG(LogTemp, Warning, TEXT("[FM] 인카운터 종료 — 탐험 상태로 복귀"));
}

void AFMSimManager::CheckCombatResolution()
{
	if (!bInCombat) return;

	bool bAnyAlly = false, bAnyEnemy = false;
	for (uint64 Id : CombatUnitIds)
	{
		const Unit* U = Sim.GetUnit(Id);
		if (!U || !U->alive) continue;
		if (U->faction == Faction::Hostile) bAnyEnemy = true;
		else                                bAnyAlly  = true;
	}

	// 아바타는 CombatUnitIds 밖이지만 엄연히 아군 — 빼먹으면 용병 없이 붙었을 때
	// 시작하자마자 패배로 판정된다.
	if (const Unit* Av = Sim.GetUnit(AvatarUnitId))
		if (Av->alive) bAnyAlly = true;

	if (bAnyAlly && bAnyEnemy) return;   // 아직 진행 중

	const bool bVictory = !bAnyEnemy;
	UE_LOG(LogTemp, Warning, TEXT("[FM] 전투 종료 — %s"), bVictory ? TEXT("승리") : TEXT("패배"));

	EndEncounter();
	OnCombatEnded.Broadcast(bVictory);   // 정리 후에 알린다 — 구독자가 바로 다음 인카운터를 걸 수 있게

	if (bReturnToWorldMapAfterCombat)
	{
		// 즉시 OpenLevel 하면 승패 연출을 볼 틈이 없고, 같은 프레임에 레벨이 날아가
		// 구독자들이 정리 중에 파괴된다. 한 박자 뒤로 미룬다.
		if (ReturnDelay > 0.f)
			GetWorldTimerManager().SetTimer(ReturnTimer, this, &AFMSimManager::ReturnToWorldMap, ReturnDelay, false);
		else
			ReturnToWorldMap();
	}
}

void AFMSimManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Sim.Tick(DeltaSeconds);			// Sim 한 스텝 진행
	DrainSimEvents();
	CheckCombatResolution();   // 이벤트 소진 뒤에 — 액터를 파괴하기 전에 알림이 먼저 나가야 한다
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
		if (!U.alive) continue;   // 시체는 선택 대상 아님 (액터는 남아도 클릭은 통과)
		// 탑다운 선택 → Z 무시하고 XY 평면 거리만 비교 (클릭은 바닥 Z, 유닛은 Sim z=0이라 3D로 하면 절대 안 잡힘)
		float D = FVector::Dist2D(WorldPos, FVector(U.pos.x, U.pos.y, U.pos.z));
		if (D < BestDist) { BestDist = D; Best = Pair.first; }   // Pair.first = 유닛 id
	}
	return Best;
}

void AFMSimManager::HandleClick(const FVector& WorldPos)
{
	const uint64 Prev = SelectedUnitId;
	const uint64 Hit = FindUnitNear(WorldPos, 50.f);

	UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] HandleClick hit=%llu prev=%llu"), Hit, Prev);

	if (Hit == 0) return;

	SelectedUnitId = Hit;
	bTargeting = false;
	
	// 리스트가 떠 있는데 다른 유닛을 골랐으면 → 리스트 닫고 링 복귀
	if (bMenuOpen && SelectedUnitId != Prev)
		OnMenuCancel.Broadcast();
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
	bTargeting = false;
}

void AFMSimManager::MoveSelectedTo(const FVector& WorldPos)
{
	if (SelectedUnitId != 0)
	{
		IssueMoveCommand(SelectedUnitId, WorldPos);
		SelectedUnitId = 0;
		bTargeting = false;
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
	bTargeting = false;
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
	bTargeting = false;
}

void AFMSimManager::CastSkill(int32 SkillType, uint64 TargetId)
{
	if (SelectedUnitId == 0) return;

	Command c;
	c.type = CommandType::Skill;
	c.skillId = (uint32)SkillType;   // Sim SkillType 값
	c.targetId = TargetId;           // 대상(공격=적, 힐=아군, 자기강화=자신)

	const CommandResult R = Sim.IssueCommand(SelectedUnitId, c, false);
	if (const Unit* U = Sim.GetUnit(SelectedUnitId))
		UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] CastSkill unit=%llu skill=%d target=%llu result=%d mp=%.1f"),
			SelectedUnitId, SkillType, TargetId, (int32)R, U->mp);

	SelectedUnitId = 0;
	bTargeting = false;
	CancelMenu();
}

bool AFMSimManager::ActivateItem(int32 ItemId)
{
	const ItemDef Def = GetItemDef((uint32)ItemId);
	UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] ActivateItem id=%d cat=%d sel=%llu"),
		ItemId, (int32)Def.category, SelectedUnitId);
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

	MetaPlayer* M = MetaPtr();
	if (!M || !M->Consume((uint32)ItemId))   // 재고 없음
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
	bTargeting = false;
	CancelMenu();          // 사용 후 목록 창 닫기
	return true;
}

bool AFMSimManager::EquipItem(int32 ItemId)
{
	return false;
}


TArray<FFMItemInfo> AFMSimManager::GetInventory() const
{
	TArray<FFMItemInfo> Out;
	const MetaPlayer* M = MetaPtr();
	if (!M) return Out;

	for (const ItemStack& S : M->inventory)
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
	bTargeting = false;
}

void AFMSimManager::IssueStopSelected()
{
	if (SelectedUnitId == 0) return;
	Command c;
	c.type = CommandType::Stop;
	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;   // 명령 후 선택 해제 → 링 사라짐
	bTargeting = false;
}
                  

void AFMSimManager::IssueFocusSelected()
{
	if (SelectedUnitId == 0) return;
	Command c;
	c.type = CommandType::Focus;
	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;   // 명령 후 선택 해제 → 링 사라짐
	bTargeting = false;
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
		Info.TargetMode   = (int32)s.targetMode;
		Info.TargetFilter = (int32)s.targetFilter;
		// MP 부족하거나 쿨다운 중이면 시전 불가 → UI가 버튼을 회색으로
		Info.bCanCast     = (It->second.mp >= (float)s.mpCost) && (s.cdRemaining <= 0.f);
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

FSkillInfo AFMSimManager::FindSkillInfo(int32 SkillType) const
{
	for (const FSkillInfo& Info : GetSelectedUnitSkills())
		if (Info.SkillType == SkillType)
			return Info;
	return FSkillInfo();   // 못 찾음 (SkillType=0)
}

uint64 AFMSimManager::FindUnitNearFiltered(const FVector& WorldPos, float Radius, int32 Filter) const
{
	// 기준 진영 = 시전자(선택 유닛)의 진영
	auto SelIt = Sim.Units().find(SelectedUnitId);
	if (SelIt == Sim.Units().end()) return 0;
	const Faction MyFaction = SelIt->second.faction;

	uint64 Best = 0;
	float  BestDist = Radius;
	for (const auto& Pair : Sim.Units())
	{
		const Unit& U = Pair.second;
		if (!U.alive) continue;

		// 진영 필터
		if (Filter == (int32)TargetFilter::Ally  && U.faction != MyFaction) continue;
		if (Filter == (int32)TargetFilter::Enemy && U.faction == MyFaction) continue;

		float D = FVector::Dist2D(WorldPos, FVector(U.pos.x, U.pos.y, U.pos.z));
		if (D < BestDist) { BestDist = D; Best = Pair.first; }
	}
	return Best;
}

void AFMSimManager::CastSkillAtPoint(int32 SkillType, const FVector& WorldPos)
{
	if (SelectedUnitId == 0) return;

	Command c;
	c.type          = CommandType::Skill;
	c.skillId       = (uint32)SkillType;
	c.targetPos     = Vec3{ (float)WorldPos.X, (float)WorldPos.Y, 0.f };   // Z는 Sim 지면
	c.hasTargetPos  = true;

	Sim.IssueCommand(SelectedUnitId, c, false);
	SelectedUnitId = 0;
	bTargeting     = false;
}
