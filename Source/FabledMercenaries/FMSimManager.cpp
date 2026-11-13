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
#include "Camera/PlayerCameraManager.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"
#include "FMChatSubsystem.h"
#include "Engine/GameInstance.h"

/** 스킬 이름 — 링 UI와 전투 로그가 같은 표를 본다 */
static FString SkillLabel(int32 TypeValue)
{
	switch ((SkillType)TypeValue)
	{
	case SkillType::Charge:    return TEXT("돌진");
	case SkillType::MagicBolt: return TEXT("마법탄");
	case SkillType::Meteor:    return TEXT("메테오");
	case SkillType::Heal:      return TEXT("힐");
	case SkillType::Defense:   return TEXT("방어 태세");
	default:                   return TEXT("스킬");
	}
}

/** 방어 방식 이름 — 팝업에 그대로 뜬다 */
static FString DefenseLabel(int32 KindValue)
{
	switch ((DefenseKind)KindValue)
	{
	case DefenseKind::Parry: return TEXT("패링!");
	case DefenseKind::Block: return TEXT("막음!");
	case DefenseKind::Dodge: return TEXT("회피!");
	default:                 return TEXT("방어!");
	}
}

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

	// 무기 외형 테이블 기본값 — 레벨마다 지정하지 않아도 되게. (레벨에서 다른 테이블로 바꿔 끼울 수는 있다)
	static ConstructorHelpers::FObjectFinder<UDataTable> WeaponTableFinder(TEXT("/Game/Data/DT_WeaponVisuals.DT_WeaponVisuals"));
	if (WeaponTableFinder.Succeeded()) WeaponVisualTable = WeaponTableFinder.Object;

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

	// 아바타 이동은 전투 명령이 아니다 — 게이지를 쓰지 않게 사실상 무한으로.
	// (안 그러면 드래그로 목적지를 계속 갱신할 때 게이지가 금방 바닥나 명령이 조용히 거부된다)
	Commander& AvatarCmd = Sim.AddCommander(AVATAR_COMMANDER_ID, CommanderType::Command);
	AvatarCmd.cmdMax   = 100000.f;
	AvatarCmd.cmdGauge = 100000.f;
	AvatarCmd.cmdRate  = 100000.f;

	if (!UnitClass)
		UE_LOG(LogTemp, Error, TEXT("[FM] UnitClass가 None! FMSimManager 디테일에서 Unit Class를 BP_Unit으로 지정하세요."));

	if (bSpawnExplorationAvatar)
	{
		// 매니저 액터가 놓인 자리에서 출발. 전투 유닛과 달리 CombatUnitIds에 넣지 않는다.
		const FVector Home = GetActorLocation();
		AvatarUnitId = SpawnSimUnit(EFMClass::Warrior, AVATAR_COMMANDER_ID, Faction::Player, FVector2D(Home.X, Home.Y));
		if (Unit* Av = Sim.GetUnit(AvatarUnitId))
		{
			Av->moveSpeed = AvatarMoveSpeed;   // 직업 이속(전사 300=걷기) 대신 달리기 속도

			// 탐험 중엔 턴 제한 없음. 월드맵 드래그 이동이 0.1초마다 Move 명령을 새로 넣는데
			// 턴테이블에 걸리면 조작이 통째로 씹힌다. 전투에 들어갈 때만 켠다(StartEncounter).
			Av->turnMax = 0.f;
		}
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

	Sim.OnDamaged = [this](uint64_t Id, float Damage, bool bBehind, bool bCrit)
	{
		FSimEvent E{ ESimEvt::Damaged, Id, 0, bBehind, bCrit };
		E.Damage = Damage;
		PendingEvents.Add(E);
	};

	Sim.OnDefended = [this](uint64_t Id, DefenseKind Kind)
	{ PendingEvents.Add({ ESimEvt::Defended, Id, (int32)Kind }); };

	Sim.OnDeath = [this](uint64_t Id)
	{ PendingEvents.Add({ ESimEvt::Death, Id }); };

	Sim.OnSkillCast = [this](uint64_t Id, SkillType T)
	{
		FSimEvent E{ ESimEvt::SkillCast, Id, (int32)T };

		// 지점 지정 스킬이면 터진 자리와 반경을 지금 담아 둔다.
		// 이벤트를 소진하는 시점에는 후딜이 짧은 명령이 이미 끝나 current가 비어 있을 수 있다.
		if (const Unit* U = Sim.GetUnit(Id))
		{
			if (U->current.hasTargetPos)
			{
				for (const Skill& S : U->skills)
					if (S.category == SkillCategory::Active && S.type == T) { E.Radius = S.radius; break; }

				if (E.Radius > 0.f)
					E.Pos = FVector(U->current.targetPos.x, U->current.targetPos.y,
						GroundZAt(U->current.targetPos.x, U->current.targetPos.y));
			}
		}
		PendingEvents.Add(E);
	};

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

		case ESimEvt::Damaged:
			A->NotifyDamaged(E.bFromBehind, E.bCrit);
			ShowHitPopup(E.UnitId, FString::FromInt(FMath::RoundToInt(E.Damage)), E.bCrit);
			if (bLogEveryHit)
			{
				LogCombat(FString::Printf(TEXT("%s %d 피해%s"), *UnitLabel(E.UnitId),
					FMath::RoundToInt(E.Damage), E.bCrit ? TEXT(" (후면)") : TEXT("")));
			}
			break;

		case ESimEvt::Defended:
			ShowHitPopup(E.UnitId, DefenseLabel(E.Param), false);
			if (bLogEveryHit)
				LogCombat(FString::Printf(TEXT("%s %s"), *UnitLabel(E.UnitId), *DefenseLabel(E.Param)));
			break;
		case ESimEvt::Death:
			A->NotifyDeath();
			LogCombat(FString::Printf(TEXT("%s 쓰러짐"), *UnitLabel(E.UnitId)));
			break;
		case ESimEvt::SkillCast:
			A->NotifySkillCast(E.Param);
			if (E.Radius > 0.f) DrawSkillBlast(E.Pos, E.Radius);   // 범위 마법이 터진 자리
			LogCombat(FString::Printf(TEXT("%s — %s"), *UnitLabel(E.UnitId), *SkillLabel(E.Param)));
			break;
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

void AFMSimManager::BlockAvatarAtLedges(float PrevX, float PrevY)
{
	if (bInCombat && !bIsWorldMap) return;            // 전투 중엔 막지 않는다 (점프 불가)

	Unit* Av = Sim.GetUnit(AvatarUnitId);
	TObjectPtr<AFMUnit>* Found = UnitActors.Find(AvatarUnitId);
	AFMUnit* A = Found ? Found->Get() : nullptr;
	float FootZ = 0.f;
	if (!Av || !Av->alive || !A || !A->GetFootZ(FootZ)) return;

	const float NewX = Av->pos.x, NewY = Av->pos.y;
	if (NewX == PrevX && NewY == PrevY) return;

	// 이동 경로를 촘촘히(10cm) 따라가며 바닥 높이를 잰다 — 한 칸에서 '오를 수 있는 높이 + 그 칸 길이만큼의 경사'를
	// 넘게 솟으면 턱이다. 한 프레임 이동량 전체로 경사를 허용하면 프레임이 떨어질 때(창이 뒤에 있을 때 등)
	// 한 번에 수십 cm를 가므로 절벽도 '경사'로 통과해 버린다 → 칸 단위로 끊어서 프레임 속도와 무관하게.
	constexpr float ProbeSpacing = 10.f;
	const float SlopeTan = FMath::Tan(FMath::DegreesToRadians(A->WalkableSlopeDeg));
	auto CanEnter = [&](float X, float Y)
	{
		const float DX = X - PrevX, DY = Y - PrevY;
		const float Dist  = FMath::Sqrt(DX * DX + DY * DY);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Dist / ProbeSpacing));
		const float Allow = MaxStepUpHeight + (Dist / Steps) * SlopeTan;

		float RefZ = FootZ;                                   // 공중이면 발 높이 기준 (점프로 턱을 넘는 경우)
		for (int32 i = 1; i <= Steps; ++i)
		{
			const float T = (float)i / Steps;
			const float G = GroundZAt(PrevX + DX * T, PrevY + DY * T);
			if (G - RefZ > Allow) return false;
			RefZ = FMath::Max(G, FootZ);                      // 경사를 오르면 기준도 따라 올라간다
		}
		return true;
	};

	if (CanEnter(NewX, NewY)) return;

	// 막힘 — 한 축씩 다시 시도해서 턱을 따라 미끄러지게 (대각선으로 벽에 붙어 멈추지 않게)
	if      (CanEnter(NewX, PrevY)) { Av->pos.y = PrevY; }
	else if (CanEnter(PrevX, NewY)) { Av->pos.x = PrevX; }
	else                            { Av->pos.x = PrevX; Av->pos.y = PrevY; }
}

bool AFMSimManager::JumpAvatar()
{
	const Unit* Av = Sim.GetUnit(AvatarUnitId);
	if (!Av || !Av->alive) return false;

	TObjectPtr<AFMUnit>* Found = UnitActors.Find(AvatarUnitId);
	AFMUnit* A = Found ? Found->Get() : nullptr;
	return A && A->StartJump();
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

	// 아바타(지휘관)도 아군 줄에 같이 선다. 조우 지점에 그대로 두면 혼자 진형 앞에 튀어나온 채로
	// 전투가 시작되고, 등장 연출에서도 혼자 제자리에 남아 합류하는 느낌이 나지 않는다.
	// 아바타가 줄의 0번 자리를 쓰므로 용병들은 한 칸씩 밀린다 — 줄 전체는 그대로 가운데 정렬.
	const bool  bHasAvatar  = (AvatarUnitId != 0) && (Sim.GetUnit(AvatarUnitId) != nullptr);
	const int32 AvatarSlots = bHasAvatar ? 1 : 0;

	// 아군 — 용병 로스터가 우선. Def.Allies는 로스터가 없을 때(GameInstance 미지정 등)만 쓴다.
	int32 AllyCount = 0, AliveAllyCount = 0;
	MetaPlayer* MP = MetaPtr();
	if (MP && !MP->roster.empty())
	{
		const int32 N    = (int32)MP->roster.size();
		const int32 Line = N + AvatarSlots;   // 줄 전체 인원(아바타 포함)
		for (int32 i = 0; i < N; ++i)
		{
			const Mercenary& M = MP->roster[i];

			// 로스터엔 진형 정보가 없으므로 가운데 정렬로 한 줄로 세운다
			FFMUnitSpawn S;
			S.UnitClass = FromSimClass(M.unitClass);
			S.Offset    = FVector2D(0.f, (i + AvatarSlots - (Line - 1) * 0.5f) * 100.f);

			const uint64 Id = SpawnSimUnit(S.UnitClass, 1, Faction::Player, Place(AllyBase, Fwd, S));
			CombatUnitIds.Add(Id);
			UnitToMercId.Add(Id, M.id);

			// 장비 보너스 적용 (공격력·최대체력) — 체력 복원보다 먼저 해야 상한이 맞는다
			Sim.ApplyEquipment(Id, M.equip);

			// 무기 외형 — 스탯(ApplyEquipment)과 같은 장비 데이터에서
			const uint32 WeaponId = M.equip[(int)EquipSlot::Weapon - 1];
			const FFMWeaponVisual* Visual = FindWeaponVisual(WeaponId);
			if (WeaponId != 0 && !Visual)
				UE_LOG(LogTemp, Warning, TEXT("[FM]   용병 #%u 무기(%s) 외형 없음 — WeaponVisualTable 미지정이거나 DT에 ItemId %u 행이 없음"),
					M.id, *FMItemName(WeaponId), WeaponId);
			if (TObjectPtr<AFMUnit>* Found = UnitActors.Find(Id))
				if (AFMUnit* A = Found->Get())
					A->SetWeaponVisual(Visual);
			
			// 지난 전투 결과를 그대로 복원 — 죽었으면 죽은 채로 선다(시체로 스폰)
			if (Unit* U = Sim.GetUnit(Id))
			{
				U->alive = M.alive;
				U->hp    = M.alive ? FMath::Min(M.hp, U->maxHp) : 0.f;
				UE_LOG(LogTemp, Warning, TEXT("[FM]   용병 #%u 스폰 — HP %.0f/%.0f 공격 %.0f"),
					M.id, U->hp, U->maxHp, U->attackDamage);
			}

			++AllyCount;
			if (M.alive) ++AliveAllyCount;
		}

		if (AliveAllyCount == 0)
			UE_LOG(LogTemp, Warning, TEXT("[FM] 살아있는 용병이 없습니다 — 월드맵 휴식 지점에서 회복하세요 (아바타 혼자 싸웁니다)"));
	}
	else
	{
		for (const FFMUnitSpawn& S : Def.Allies)
			CombatUnitIds.Add(SpawnSimUnit(S.UnitClass, 1, Faction::Player, Place(AllyBase, Fwd, S)));
		AllyCount = AliveAllyCount = Def.Allies.Num();
	}

	// 아바타를 줄 왼쪽 끝(0번 자리)으로 옮긴다. CombatUnitIds에는 넣지 않는다 —
	// 거기 들어간 유닛은 EndEncounter가 Sim과 액터를 통째로 지우므로 아바타가 사라진다.
	if (bHasAvatar)
	{
		const int32 Line = AllyCount + AvatarSlots;
		FFMUnitSpawn S;
		S.Offset = FVector2D(0.f, (0 - (Line - 1) * 0.5f) * 100.f);
		const FVector2D P = Place(AllyBase, Fwd, S);

		SetAvatarWorldPos(FVector(P.X, P.Y, 0.f));   // 이동 명령·예약도 같이 비운다
		if (Unit* Av = Sim.GetUnit(AvatarUnitId))
		{
			Av->facing = Vec3{ (float)Fwd.X, (float)Fwd.Y, 0.f };   // 적 쪽을 보고 선다

			// 전투 중엔 지휘관도 턴테이블을 탄다 — 용병과 같은 규칙. 가득 채워 시작한다.
			Av->turnMax   = AvatarTurnMax;
			Av->turnRate  = AvatarTurnRate;
			Av->turnGauge = Av->turnMax;
		}
	}

	for (const FFMUnitSpawn& S : Def.Enemies)
		CombatUnitIds.Add(SpawnSimUnit(S.UnitClass, ENEMY_COMMANDER_ID, Faction::Hostile, Place(EnemyBase, -Fwd, S)));

	SetCombatActive(true);

	UE_LOG(LogTemp, Warning, TEXT("[FM] 인카운터 시작 '%s' — 아군 %d기(생존 %d), 적 %d기 (중점 %s)"),
		*Def.EncounterId.ToString(), AllyCount, AliveAllyCount, Def.Enemies.Num(), *Center.ToCompactString());

	LogSystem(FString::Printf(TEXT("전투 시작 — 아군 %d기(생존 %d) 대 적 %d기"),
		AllyCount, AliveAllyCount, Def.Enemies.Num()));
}

void AFMSimManager::RequestEncounter(const FFMEncounterDef& Def, const FVector& Center, const FVector& FacingDir, AActor* Symbol)
{
	if (bInCombat || bEncounterTransition) return;

	bEncounterTransition = true;
	PendingDef    = Def;
	PendingCenter = Center;
	PendingDir    = FacingDir;
	PendingSymbol = Symbol;

	// 아바타 정지 — 어두워지는 동안 계속 걸어가지 않게
	if (Unit* Av = Sim.GetUnit(AvatarUnitId))
	{
		Av->executing = false;
		Av->reserveQueue.clear();
	}

	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		Cam->StartCameraFade(0.f, 1.f, IntroFadeOutTime, FLinearColor::Black, false, true);   // 끝나도 검은 화면 유지

	GetWorldTimerManager().SetTimer(IntroTimer, this, &AFMSimManager::OnEncounterFadedOut,
		FMath::Max(IntroFadeOutTime, 0.01f), false);
}

void AFMSimManager::OnEncounterFadedOut()
{
	bEncounterTransition = false;

	if (AActor* Sym = PendingSymbol.Get())
		Sym->SetActorHiddenInGame(true);   // 전투 동안 숨김 (심볼이 결과에 따라 Destroy/복귀)
	PendingSymbol.Reset();

	StartEncounter(PendingDef, PendingCenter, PendingDir);

	// 등장 — 아군은 진형 뒤쪽(-Fwd), 적은 반대편(+Fwd)에서 달려 들어온다. Sim 위치는 이미 진형 자리이고,
	// 액터만 뒤에서 출발해 따라붙는다 (화면 연출 — Sim/서버엔 영향 없음)
	FVector Fwd = PendingDir.GetSafeNormal2D();
	if (Fwd.IsNearlyZero()) Fwd = FVector::ForwardVector;

	float MaxDelay = 0.f;
	auto RunIn = [&](uint64 Id)
	{
		const Unit* U = Sim.GetUnit(Id);
		TObjectPtr<AFMUnit>* Found = UnitActors.Find(Id);
		AFMUnit* A = Found ? Found->Get() : nullptr;
		if (!U || !A) return;

		const FVector Back  = (U->faction == Faction::Hostile) ? Fwd : -Fwd;
		const float   Delay = FMath::FRandRange(0.f, IntroStagger);
		// 시체로 스폰된 용병은 달려오지 않고 제자리에서 나타나기만
		A->PlayEntrance(U->alive ? Back * IntroEntryDistance : FVector::ZeroVector, IntroRunTime, Delay);
		MaxDelay = FMath::Max(MaxDelay, Delay);
	};

	for (uint64 Id : CombatUnitIds) RunIn(Id);
	// 아바타(지휘관)도 같이 달려 들어온다. CombatUnitIds에 없으므로 따로 불러준다
	if (AvatarUnitId != 0) RunIn(AvatarUnitId);
	IntroEndTime = GetWorld()->GetTimeSeconds() + MaxDelay + IntroRunTime;

	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		Cam->StartCameraFade(1.f, 0.f, IntroFadeInTime, FLinearColor::Black, false, false);
}

bool AFMSimManager::IsEncounterIntroPlaying() const
{
	return bEncounterTransition || (GetWorld() && GetWorld()->GetTimeSeconds() < IntroEndTime);
}

void AFMSimManager::EndEncounter()
{
	if (!bInCombat) return;

	// 전투 결과를 로스터에 기록 — 유닛을 Sim에서 지우기 전에 해야 한다
	if (MetaPlayer* MP = MetaPtr())
	{
		for (const TPair<uint64, uint32>& Pair : UnitToMercId)
		{
			Mercenary*  M = MP->FindMerc(Pair.Value);
			const Unit* U = Sim.GetUnit(Pair.Key);
			if (!M || !U) continue;

			M->alive = U->alive;
			M->hp    = U->alive ? FMath::Clamp(U->hp, 0.f, M->TotalMaxHp()) : 0.f;   // 장비 포함 상한
		}

		if (UFMGameInstance* GI = GetWorld()->GetGameInstance<UFMGameInstance>())
			GI->LogRoster(TEXT("전투 결과 기록"));
	}
	UnitToMercId.Reset();

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

	// 탐험 복귀 — 아바타 턴 제한 해제. 안 풀면 월드맵 드래그 이동이 씹힌다
	if (Unit* Av = Sim.GetUnit(AvatarUnitId))
	{
		Av->turnMax   = 0.f;
		Av->turnGauge = 0.f;
	}

	SelectedUnitId = 0;
	bTargeting     = false;
	bMenuOpen      = false;
	SetCombatActive(false);

	UE_LOG(LogTemp, Warning, TEXT("[FM] 인카운터 종료 — 탐험 상태로 복귀"));
	LogSystem(TEXT("전투 종료 — 탐험으로 돌아갑니다"));
}

void AFMSimManager::SetCombatActive(bool bActive)
{
	if (bInCombat == bActive) return;
	bInCombat = bActive;
	OnCombatStateChanged.Broadcast(bInCombat);
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
	LogSystem(bVictory ? TEXT("승리했습니다") : TEXT("패배했습니다"));

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

	// 턱 판정용 — Sim이 아바타를 옮기기 전 위치
	const Unit* AvBefore = Sim.GetUnit(AvatarUnitId);
	const float AvPrevX = AvBefore ? AvBefore->pos.x : 0.f;
	const float AvPrevY = AvBefore ? AvBefore->pos.y : 0.f;

	Sim.Tick(DeltaSeconds);			// Sim 한 스텝 진행
	if (AvBefore) BlockAvatarAtLedges(AvPrevX, AvPrevY);
	DrainSimEvents();
	CheckCombatResolution();   // 이벤트 소진 뒤에 — 액터를 파괴하기 전에 알림이 먼저 나가야 한다
	// HP/MP 바를 카메라 쪽으로 세워 두기 위한 기준 축 (빌보드).
	// 월드 축에 고정해 그리면 카메라를 QE로 돌릴 때 바가 같이 돌아서 비스듬하거나 선으로 보인다.
	// 루프 밖에서 한 번만 구한다 — 유닛마다 같은 값이다.
	FVector CamRight(0.f, 1.f, 0.f);
	FVector CamUp(0.f, 0.f, 1.f);
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const APlayerCameraManager* Cam = PC->PlayerCameraManager)
		{
			const FRotationMatrix CamBasis(Cam->GetCameraRotation());
			CamRight = CamBasis.GetUnitAxis(EAxis::Y);
			CamUp    = CamBasis.GetUnitAxis(EAxis::Z);
		}
	}

	// 디버그용 : 모든 유닛 위치 표시
	for (const auto& Pair : Sim.Units())
	{
		const Unit& U = Pair.second;

		// 화면 액터 갱신 — 죽어도 실행(Dead 애님 전달 → 쓰러지는 모션, 시체 남음)
		if (TObjectPtr<AFMUnit>* Found = UnitActors.Find(Pair.first))
		{
			// 등장 연출 중이면 액터가 Sim 위치에서 떨어져 있다 — 그 자리 바닥 높이를 쓴다
			const FVector Off = (*Found)->GetEntranceOffset();
			FVector FloorLoc(U.pos.x, U.pos.y, GroundZAt(U.pos.x + Off.X, U.pos.y + Off.Y));
			FVector FacingDir(U.facing.x, U.facing.y, 0.f);
			(*Found)->UpdateFromSim(FloorLoc, FacingDir, ToAnim(U.GetActionState()), DeltaSeconds);

			// 턴테이블이 돈 아군만 외곽선. 탐험 중(turnMax==0)에는 늘 명령 가능하므로 표시하지 않는다
			(*Found)->SetCommandReady(bInCombat && U.alive && U.turnMax > 0.f
				&& U.faction == Faction::Player && U.TurnReady());

			// 등장 연출로 달려오는 중엔 발밑 링/HP바를 그리지 않는다 — Sim 자리(진형)에 빈 링이 먼저 떠 보이므로
			if (!(*Found)->GetEntranceOffset().IsNearlyZero()) continue;
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
		
		// 바라보는 방향 화살표 — 전투 중에만. 도착 방향을 확인하려고 두는 것이라
		// 월드맵·탐험맵에서는 아바타 혼자 노란 화살표를 달고 다녀서 거슬린다.
		if (bInCombat)
		{
			FVector F(U.facing.x, U.facing.y, 0.f);
			if (!F.IsNearlyZero())
				DrawDebugDirectionalArrow(GetWorld(), Loc, Loc + F.GetSafeNormal() * 60.f,
					60.f, FColor::Yellow, false, -1.f, 0, 3.f);
		}
		
		DrawCastCircle(U);   // 범위 마법 시전 중이면 지정한 바닥에 마법진

		// HP/MP 바 — 발밑 링 아래(화면 기준)에 깐다.
		// 길이 방향은 CamRight, 위아래 간격은 CamUp — 둘 다 카메라를 따라가므로 카메라를 돌려도
		// 화면에서는 늘 가로로 눕고 간격도 그대로다.
		// SDPG_Foreground로 그린다: 그냥 그리면 제 몸이나 앞 유닛, 지형 턱에 가려 안 보인다.
		const float BarW = 80.f;
		const FVector BarPos = Feet - CamUp * 45.f;    // 링 바로 아래
		const FVector MpBase = Feet - CamUp * 58.f;    // HP 바 아래

		const float Ratio = FMath::Clamp(U.hp / U.maxHp, 0.f, 1.f);
		const FVector Left  = BarPos - CamRight * (BarW * 0.5f);
		const FVector Fill  = Left + CamRight * (BarW * Ratio);
		DrawDebugLine(GetWorld(), Left, BarPos + CamRight * (BarW * 0.5f), FColor(40, 40, 40), false, -1.f, SDPG_Foreground, 6.f);  // 배경(빈 체력)
		DrawDebugLine(GetWorld(), Left, Fill,                              FColor::Green,      false, -1.f, SDPG_Foreground, 6.f);  // 현재 체력

		const float MpRatio = (U.mpMax > 0.f) ? FMath::Clamp(U.mp / U.mpMax, 0.f, 1.f) : 0.f;
		const FVector MpLeft = MpBase - CamRight * (BarW * 0.5f);
		DrawDebugLine(GetWorld(), MpLeft, MpBase + CamRight * (BarW * 0.5f),      FColor(30,30,60), false, -1.f, SDPG_Foreground, 5.f);
		DrawDebugLine(GetWorld(), MpLeft, MpLeft + CamRight * (BarW * MpRatio),   FColor::Blue,     false, -1.f, SDPG_Foreground, 5.f);

		// 턴 게이지 — MP 바 아래. 가득 차면(= 명령 수령 가능) 금색, 차는 중엔 흐린 회색.
		// 턴 제한이 없는 유닛(탐험 중 아바타)은 늘 가득이라 바를 그리지 않는다
		if (U.turnMax > 0.f)
		{
			const bool    bReady  = U.TurnReady();
			const FVector TurnBase = MpBase - CamUp * 11.f;
			const FVector TurnLeft = TurnBase - CamRight * (BarW * 0.5f);
			DrawDebugLine(GetWorld(), TurnLeft, TurnBase + CamRight * (BarW * 0.5f), FColor(45,40,25), false, -1.f, SDPG_Foreground, 5.f);
			DrawDebugLine(GetWorld(), TurnLeft, TurnLeft + CamRight * (BarW * U.TurnRatio()),
				bReady ? FColor(255, 215, 70) : FColor(130, 115, 60), false, -1.f, SDPG_Foreground, 5.f);
		}

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

UFMChatSubsystem* AFMSimManager::ChatSys() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFMChatSubsystem>() : nullptr;
}

void AFMSimManager::LogCombat(const FString& Text) const
{
	if (UFMChatSubsystem* C = ChatSys()) C->PostCombat(Text);
}

void AFMSimManager::LogSystem(const FString& Text) const
{
	if (UFMChatSubsystem* C = ChatSys()) C->PostSystem(Text);
}

void AFMSimManager::ShowHitPopup(uint64 UnitId, const FString& Text, bool bCrit)
{
	if (!OnUnitHitDisplay.IsBound()) return;

	auto It = Sim.Units().find(UnitId);
	if (It == Sim.Units().end()) return;

	const Unit& U = It->second;

	// 머리 위에서 출발. 같은 유닛이 연달아 맞으면 숫자가 겹치므로 좌우로 조금 흩어 놓는다
	const float Jitter = FMath::FRandRange(-25.f, 25.f);
	const FVector At(U.pos.x + Jitter, U.pos.y, GroundZAt(U.pos.x, U.pos.y) + 150.f);

	OnUnitHitDisplay.Broadcast(At, Text, bCrit, U.faction == Faction::Player);
}

FString AFMSimManager::UnitLabel(uint64 UnitId) const
{
	if (UnitId == AvatarUnitId) return TEXT("지휘관");

	auto It = Sim.Units().find(UnitId);
	if (It == Sim.Units().end()) return TEXT("?");

	const TCHAR* Cls = FMClassLabel(It->second.unitClass);

	// 내 용병이면 로스터 번호를 붙여 누가 누군지 가린다 (같은 직업이 여럿일 수 있다)
	if (const uint32* MercId = UnitToMercId.Find(UnitId))
		return FString::Printf(TEXT("%s #%u"), Cls, *MercId);

	return (It->second.faction == Faction::Hostile)
		? FString::Printf(TEXT("적 %s"), Cls)
		: FString(Cls);
}

void AFMSimManager::DrawCastCircle(const Unit& U) const
{
	if (!U.executing || U.attackFired) return;                                  // 아직 시전 중일 때만
	if (U.current.type != CommandType::Skill || !U.current.hasTargetPos) return;

	const SkillType Want = (SkillType)U.current.skillId;
	const Skill* Sk = nullptr;
	for (const Skill& S : U.skills)
		if (S.category == SkillCategory::Active && S.type == Want) { Sk = &S; break; }
	if (!Sk || Sk->radius <= 0.f) return;   // 범위 없는 지점 스킬이면 마법진도 없다

	const float CX = U.current.targetPos.x;
	const float CY = U.current.targetPos.y;
	const FVector C(CX, CY, GroundZAt(CX, CY) + 6.f);

	// 안쪽 원이 바깥까지 차오르면 발동 — 남은 시전 시간이 눈으로 보인다
	const float Progress = (Sk->preDelay > 0.f)
		? FMath::Clamp(U.execGauge / Sk->preDelay, 0.f, 1.f) : 1.f;

	const FColor Rim  = FColor(170, 90, 255);   // 보라 — 실제 피해 범위
	const FColor Fill = FColor(255, 150, 50);   // 주황 — 차오르는 시전 게이지

	DrawDebugCircle(GetWorld(), C, Sk->radius, 48, Rim, false, -1.f, SDPG_World, 4.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);
	DrawDebugCircle(GetWorld(), C, Sk->radius * Progress, 48, Fill, false, -1.f, SDPG_World, 3.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);

	// 마법진 살 — 바깥 테두리로 뻗는 선 8개
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = (2.f * PI * i) / 8.f;
		const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
		DrawDebugLine(GetWorld(), C + D * (Sk->radius * 0.55f), C + D * Sk->radius,
			Rim, false, -1.f, SDPG_World, 2.f);
	}

	// 누가 시전하고 있는지 — 시전자에서 마법진으로 잇는 선
	DrawDebugLine(GetWorld(), FVector(U.pos.x, U.pos.y, GroundZAt(U.pos.x, U.pos.y) + 60.f), C,
		Rim, false, -1.f, SDPG_World, 1.5f);
}

void AFMSimManager::DrawSkillBlast(const FVector& Center, float Radius) const
{
	if (Radius <= 0.f) return;

	const float   Life = 0.45f;   // 디버그 드로우가 알아서 지워준다 — 따로 치울 필요 없음
	const FVector C    = Center + FVector(0, 0, 8.f);

	DrawDebugCircle(GetWorld(), C, Radius, 48, FColor(255, 170, 60), false, Life, SDPG_World, 8.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);
	DrawDebugCircle(GetWorld(), C, Radius * 0.6f, 40, FColor(255, 240, 150), false, Life, SDPG_World, 5.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);
	DrawDebugSphere(GetWorld(), Center + FVector(0, 0, Radius * 0.35f), Radius * 0.5f, 16,
		FColor(255, 120, 40), false, Life, SDPG_World, 3.f);
}

float AFMSimManager::GroundZAt(float X, float Y) const
{
	FHitResult Hit;
	const FVector From(X, Y, GroundZ + GroundTraceRange);
	const FVector To  (X, Y, GroundZ - GroundTraceRange);
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
		// 전투 중 아이템 목록 — 장비는 전투 중에 갈아입지 않으므로 소비 아이템만
		if (GetItemDef(S.itemId).category != ItemCategory::Consumable) continue;

		Out.Add(FMMakeItemInfo(S.itemId, S.count));
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
		Info.Radius       = s.radius;
		// MP 부족하거나 쿨다운 중이면 시전 불가 → UI가 버튼을 회색으로
		Info.bCanCast     = (It->second.mp >= (float)s.mpCost) && (s.cdRemaining <= 0.f);
		Info.Name = SkillLabel((int32)s.type);
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

const struct FFMWeaponVisual* AFMSimManager::FindWeaponVisual(uint32 ItemId) const
{
	if (!WeaponVisualTable || ItemId == 0) return nullptr;
	TArray<FFMWeaponVisual*> Rows;
	WeaponVisualTable->GetAllRows(TEXT("WeaponVisual"),Rows);
	for (const FFMWeaponVisual* Row : Rows)
		if (Row && Row->ItemId == (int32)ItemId) return Row;
	return nullptr;
}
