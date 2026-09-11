#include "FMEncounterSymbol.h"

#include "FMSimManager.h"
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
}

void AFMEncounterSymbol::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CooldownLeft > 0.f) CooldownLeft -= DeltaSeconds;

	AFMSimManager* M = Mgr.Get();
	if (!M) return;

	// 전투 중엔 심볼이 멈춘다 — 배회하다 다른 심볼을 끌고 오는 사고 방지
	if (M->IsInCombat()) return;

	if (bDrawDebugRadius)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), TriggerRadius, 24,
			FColor(255, 140, 0), false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
	}

	FVector AvatarPos;
	if (CooldownLeft <= 0.f && M->GetAvatarWorldPos(AvatarPos))
	{
		const float Dist = FVector::Dist2D(GetActorLocation(), AvatarPos);

		if (Dist <= TriggerRadius)
		{
			// 조우 — 두 사람 사이를 전장 중심으로 잡고, 아군은 심볼 쪽을 바라본다
			const FVector Sym = GetActorLocation();
			const FVector Mid = (Sym + AvatarPos) * 0.5f;
			const FVector Dir = FVector(Sym.X - AvatarPos.X, Sym.Y - AvatarPos.Y, 0.f);

			bEngaged = true;
			SetActorHiddenInGame(true);   // 전투 동안 숨김. 승리 시 Destroy, 패배 시 복귀
			M->StartEncounter(Encounter, Mid, Dir);

			UE_LOG(LogTemp, Warning, TEXT("[FM] 심볼 '%s' 조우 → 인카운터 '%s'"),
				*GetName(), *Encounter.EncounterId.ToString());
			return;
		}

		if (Dist <= ChaseRadius)
		{
			MoveTowardXY(AvatarPos, DeltaSeconds);   // 발견 → 추격
			return;
		}
	}

	// 배회
	if (PatrolRadius > 0.f)
	{
		if (FVector::Dist2D(GetActorLocation(), PatrolTarget) < 50.f)
			PickPatrolTarget();

		MoveTowardXY(PatrolTarget, DeltaSeconds);
	}
}

void AFMEncounterSymbol::MoveTowardXY(const FVector& Target, float DeltaSeconds)
{
	const FVector Cur = GetActorLocation();

	FVector Dir = FVector(Target.X - Cur.X, Target.Y - Cur.Y, 0.f);
	if (Dir.IsNearlyZero()) return;
	Dir.Normalize();

	// Z는 건드리지 않는다 — 배치된 높이를 유지 (지형 추적은 아직 필요 없음)
	const FVector Next = Cur + Dir * (MoveSpeed * DeltaSeconds);
	SetActorLocation(FVector(Next.X, Next.Y, Cur.Z));
	SetActorRotation(Dir.Rotation());
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
