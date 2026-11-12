#include "FMEncounterSymbol.h"

#include "FMSimManager.h"
#include "FMUnit.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AFMEncounterSymbol::AFMEncounterSymbol()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 조우는 거리 판정으로만 — 물리 충돌 불필요
}

void AFMEncounterSymbol::BeginPlay()
{
	Super::BeginPlay();

	HomeLoc = GetActorLocation();
	PickPatrolTarget();

	AFMSimManager* M = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));

	if (!M)
	{
		UE_LOG(LogTemp, Error, TEXT("[FM] 심볼 '%s': 레벨에 FMSimManager가 없습니다"), *GetName());
		return;
	}

	Mgr = M;
	M->OnCombatEnded.AddDynamic(this, &AFMEncounterSymbol::HandleCombatEnded);

	if (bUseAvatar) SpawnAvatar();
}

void AFMEncounterSymbol::SpawnAvatar()
{
	AFMSimManager* M = Mgr.Get();
	UClass* Cls = AvatarClass ? AvatarClass.Get() : (M ? M->UnitClass.Get() : nullptr);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FM] 심볼 '%s': 아바타 클래스가 없습니다 (심볼의 AvatarClass도, 매니저의 UnitClass도 비어 있음) — 메시로 표시"),
			*GetName());
		return;
	}

	FActorSpawnParameters P;
	P.Owner = this;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Avatar = GetWorld()->SpawnActor<AFMUnit>(Cls, GetActorLocation(), GetActorRotation(), P);
	if (!Avatar) return;

	Avatar->SetFadeAlpha(1.f);        // 등장 연출용 디더가 걸린 채로 스폰되지 않게

	// 전투 유닛은 이동 중 무조건 달리기 모션(bAlwaysRunAnim)인데, 배회는 MoveSpeed(220)로 느릿하다.
	// 그대로 두면 발이 미끄러진다 — 실제 속도를 블렌드스페이스에 넣어 걷기로 보이게 한다
	Avatar->bAlwaysRunAnim = false;

	Mesh->SetHiddenInGame(true);      // 구체 대신 아바타가 보인다
}

void AFMEncounterSymbol::EndPlay(const EEndPlayReason::Type Reason)
{
	// 심볼이 격파돼 Destroy되면 아바타도 같이 치운다 — 안 치우면 시체처럼 서 있는다
	if (Avatar) { Avatar->Destroy(); Avatar = nullptr; }
	Super::EndPlay(Reason);
}

void AFMEncounterSymbol::UpdateAvatar(float DeltaSeconds, bool bMoving)
{
	if (!Avatar) return;

	const FVector Loc = GetActorLocation();
	const AFMSimManager* M = Mgr.Get();
	const float GroundZ = M ? M->GetGroundZAt(Loc.X, Loc.Y) : Loc.Z;

	// 유닛 액터가 쓰는 길을 그대로 탄다 — 이동 속도로 블렌드스페이스가 걷기/달리기를 고르고,
	// 경사는 따라 걷고 턱에서는 떨어진다 (AFMUnit::UpdateVertical)
	Avatar->UpdateFromSim(FVector(Loc.X, Loc.Y, GroundZ), AvatarFacing,
		bMoving ? EUnitAnim::Move : EUnitAnim::Idle, DeltaSeconds);
}

void AFMEncounterSymbol::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CooldownLeft > 0.f) CooldownLeft -= DeltaSeconds;

	AFMSimManager* M = Mgr.Get();
	if (!M) return;

	// 전투 중(암전 전환 포함)엔 심볼이 멈춘다 — 배회하다 다른 심볼을 끌고 오는 사고 방지
	if (M->IsInCombat() || M->IsEncounterTransitioning())
	{
		if (Avatar) Avatar->SetActorHiddenInGame(true);   // 심볼이 숨을 때 아바타만 남으면 안 된다
		return;
	}
	if (Avatar) Avatar->SetActorHiddenInGame(IsHidden());

	if (bDrawDebugRadius)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), TriggerRadius, 24,
			FColor(255, 140, 0), false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
	}

	bool bMoved = false;

	// PlayerPos = 플레이어가 조종하는 탐험 아바타 (이 심볼의 Avatar와 헷갈리지 않게)
	FVector PlayerPos;
	if (CooldownLeft <= 0.f && M->GetAvatarWorldPos(PlayerPos))
	{
		const float Dist = FVector::Dist2D(GetActorLocation(), PlayerPos);

		if (Dist <= TriggerRadius)
		{
			// 조우 — 두 사람 사이를 전장 중심으로 잡고, 아군은 심볼 쪽을 바라본다
			const FVector Sym = GetActorLocation();
			const FVector Mid = (Sym + PlayerPos) * 0.5f;
			const FVector Dir = FVector(Sym.X - PlayerPos.X, Sym.Y - PlayerPos.Y, 0.f);

			bEngaged = true;
			// 화면이 완전히 어두워진 순간 매니저가 이 심볼을 숨기고 전투를 배치한다 (승리 시 Destroy, 패배 시 복귀)
			M->RequestEncounter(Encounter, Mid, Dir, this);

			UE_LOG(LogTemp, Warning, TEXT("[FM] 심볼 '%s' 조우 → 인카운터 '%s'"),
				*GetName(), *Encounter.EncounterId.ToString());
			UpdateAvatar(DeltaSeconds, false);
			return;
		}

		if (Dist <= ChaseRadius)
			bMoved = MoveTowardXY(PlayerPos, DeltaSeconds);   // 발견 → 추격
	}

	// 추격하지 않을 때만 배회
	if (!bMoved && PatrolRadius > 0.f)
	{
		if (FVector::Dist2D(GetActorLocation(), PatrolTarget) < 50.f)
			PickPatrolTarget();

		bMoved = MoveTowardXY(PatrolTarget, DeltaSeconds);
	}

	UpdateAvatar(DeltaSeconds, bMoved);
}

bool AFMEncounterSymbol::MoveTowardXY(const FVector& Target, float DeltaSeconds)
{
	const FVector Cur = GetActorLocation();

	FVector Dir = FVector(Target.X - Cur.X, Target.Y - Cur.Y, 0.f);
	if (Dir.IsNearlyZero()) return false;
	Dir.Normalize();

	const FVector Next = Cur + Dir * (MoveSpeed * DeltaSeconds);

	// 지형을 따라간다. 예전엔 배치된 Z를 그대로 유지했는데, 기복 있는 맵에서는
	// 심볼이 공중에 뜨거나 땅에 묻히고 조우 지점(전장 중심) Z도 틀어진다.
	const AFMSimManager* M = Mgr.Get();
	const float NextZ = M ? M->GetGroundZAt(Next.X, Next.Y) : Cur.Z;

	SetActorLocation(FVector(Next.X, Next.Y, NextZ));
	SetActorRotation(Dir.Rotation());

	AvatarFacing = Dir;   // 아바타는 마지막으로 움직인 쪽을 본다
	return true;
}

void AFMEncounterSymbol::PickPatrolTarget()
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float R     = FMath::FRandRange(0.f, PatrolRadius);

	PatrolTarget = HomeLoc + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.f);
}

void AFMEncounterSymbol::HandleCombatEnded(bool bVictory)
{
	// OnCombatEnded는 모든 심볼에게 간다 — 내가 연 전투가 아니면 무시
	if (!bEngaged) return;
	bEngaged = false;

	if (bVictory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] 심볼 '%s' 격파 — 제거"), *GetName());
		Destroy();
		return;
	}

	// 패배 — 심볼은 그대로 남고, 잠깐은 다시 안 걸린다
	SetActorHiddenInGame(false);
	CooldownLeft = RetriggerCooldown;
}
