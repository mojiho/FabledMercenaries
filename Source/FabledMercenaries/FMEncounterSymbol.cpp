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
	HomeRot = GetActorRotation();   // 배회하면서 회전이 바뀌므로 배치 당시 방향을 따로 붙잡아 둔다
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

		// 전장 — 이 심볼의 전투가 열릴 자리. 엄폐물은 이 원 안팎에 깔면 된다.
		// (심볼이 배회로 움직여도 이 원은 집에 붙어 있다 — 그게 전장을 고정하는 이유다)
		const FVector ArenaMark = ArenaCenter();
		if (bArenaAtHome)
		{
			DrawDebugCircle(GetWorld(), ArenaMark, ArenaDrawRadius(), 32,
				FColor(90, 170, 255), false, -1.f, 0, 4.f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);

			// 양 진영이 설 자리 — 이 두 점을 잇는 선이 개전 직후의 사선이다.
			// 엄폐물은 '선 위'에 놓아야 사선에 걸리고, '점 위'에 놓으면 유닛이 그 위에 스폰된다
			if (bArenaAxisFromSymbol)
			{
				FVector Axis = HomeRot.Vector().GetSafeNormal2D();
				if (Axis.IsNearlyZero()) Axis = FVector::ForwardVector;

				const float   Half = Encounter.Separation * 0.5f;
				const FVector A    = ArenaMark - Axis * Half;
				const FVector B    = ArenaMark + Axis * Half;

				DrawDebugLine(GetWorld(), A, B, FColor(60, 110, 180), false, -1.f, 0, 2.f);
				DrawDebugCircle(GetWorld(), A, 120.f, 20, FColor(80, 180, 255), false, -1.f, 0, 3.f,
					FVector(1, 0, 0), FVector(0, 1, 0), false);   // 아군 줄
				DrawDebugCircle(GetWorld(), B, 120.f, 20, FColor(255, 90, 90), false, -1.f, 0, 3.f,
					FVector(1, 0, 0), FVector(0, 1, 0), false);   // 적 줄
			}
		}

		// 리쉬 — 여기를 넘어가면 추격을 포기하고 돌아온다
		if (LeashRadius > 0.f)
			DrawDebugCircle(GetWorld(), ArenaMark, LeashRadius, 32,
				FColor(80, 80, 80), false, -1.f, 0, 1.5f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
	}

	bool bMoved = false;

	// PlayerPos = 플레이어가 조종하는 탐험 아바타 (이 심볼의 Avatar와 헷갈리지 않게)
	FVector PlayerPos;
	if (CooldownLeft <= 0.f && M->GetAvatarWorldPos(PlayerPos))
	{
		const FVector Sym  = GetActorLocation();
		const float   Dist = FVector::Dist2D(Sym, PlayerPos);

		// 접촉은 귀가 중에도 유효하다 — 쫓아온 플레이어에게 등을 보이고 걸어갈 수는 없다
		if (Dist <= TriggerRadius)
		{
			// 전장 중심 — 기본은 이 심볼이 배치된 자리다.
			// 예전엔 심볼과 플레이어의 중간이라 전투가 매번 다른 데서 열렸고,
			// 그래서 레벨에 깔아 둔 엄폐물이 쓰일지가 운이었다.
			const FVector Arena = bArenaAtHome ? ArenaCenter() : (Sym + PlayerPos) * 0.5f;

			// 진형 축 (아군 → 적 방향). 아군은 Center - Dir*절반, 적은 Center + Dir*절반에 선다
			FVector Dir;
			if (bArenaAtHome && bArenaAxisFromSymbol)
			{
				Dir = HomeRot.Vector();
				Dir.Z = 0.f;
				Dir = Dir.GetSafeNormal2D();
				if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;   // 수직으로 배치된 심볼

				// 아군은 언제나 플레이어가 서 있는 쪽에 — 반대편에서 붙잡혔으면 축을 뒤집는다.
				// 축 '선'은 그대로라 엄폐물 배치는 흔들리지 않는다
				const FVector ToPlayer(PlayerPos.X - Arena.X, PlayerPos.Y - Arena.Y, 0.f);
				if (FVector::DotProduct(Dir, ToPlayer) > 0.f) Dir = -Dir;
			}
			else
			{
				// 플레이어가 전장에 들어서는 쪽에서 아군이 선다
				Dir = FVector(Arena.X - PlayerPos.X, Arena.Y - PlayerPos.Y, 0.f);
				if (Dir.IsNearlyZero())   // 플레이어가 이미 전장 한복판에 서 있다
					Dir = FVector(Sym.X - PlayerPos.X, Sym.Y - PlayerPos.Y, 0.f);
			}

			bEngaged = true;
			// 화면이 완전히 어두워진 순간 매니저가 이 심볼을 숨기고 전투를 배치한다 (승리 시 Destroy, 패배 시 복귀)
			M->RequestEncounter(Encounter, Arena, Dir, this);

			UE_LOG(LogTemp, Warning, TEXT("[FM] 심볼 '%s' 조우 → 인카운터 '%s' (전장 %s, 끌려온 거리 %.0f)"),
				*GetName(), *Encounter.EncounterId.ToString(), *Arena.ToCompactString(),
				FVector::Dist2D(Arena, PlayerPos));
			UpdateAvatar(DeltaSeconds, false);
			return;
		}

		// --- 추격 / 리쉬 ---
		// ChaseRadius만으로는 '추격을 시작할 조건'밖에 안 된다. 한 번 걸리면 끝까지 따라가서
		// 전투가 맵 어디서든 열렸다 — 자기 영역 밖으로는 쫓지 않는다.
		//
		// 판정 기준은 '지금 내가 집에서 얼마나 멀리 왔나'가 아니라 '플레이어가 내 영역 안인가'다.
		// 전자는 이동 '전'에만 검사하게 되는데, 프레임이 길면 한 틱에 수십 cm를 건너뛰므로
		// 원을 훌쩍 넘어간 뒤에야 걸린다 — 3FPS 에디터에서 한 틱 73cm로 리쉬를 넘어
		// 조우까지 가버렸다. 후자는 이동량과 무관해서 프레임에 흔들리지 않는다.
		const float HomeDist       = FVector::Dist2D(Sym, HomeLoc);
		const float PlayerHomeDist = FVector::Dist2D(PlayerPos, HomeLoc);
		const bool  bInTerritory   = (LeashRadius <= 0.f) || (PlayerHomeDist <= LeashRadius);

		if (!bInTerritory)
		{
			// 잡으려면 영역을 벗어나야 한다 — 포기하고 돌아간다.
			// 도착 판정은 아래 귀가 블록 한 곳에만 둔다 (쿨다운 중처럼 플레이어를 못 보는
			// 틱에도 돌아와야 하므로, 여기서 같이 풀면 두 군데가 엇갈린다)
			if (HomeDist > FMath::Max(PatrolRadius, 50.f))
				bReturningHome = true;
		}
		else if (Dist <= ChaseRadius)
		{
			bReturningHome = false;                           // 영역 안으로 들어왔다 — 다시 쫓는다
			bMoved = MoveTowardXY(PlayerPos, DeltaSeconds);   // 발견 → 추격
		}
	}

	// 리쉬에 걸렸으면 배회보다 귀가가 먼저다
	if (!bMoved && bReturningHome)
	{
		if (FVector::Dist2D(GetActorLocation(), HomeLoc) <= FMath::Max(PatrolRadius, 50.f))
		{
			bReturningHome = false;
			PickPatrolTarget();
		}
		else
		{
			bMoved = MoveTowardXY(HomeLoc, DeltaSeconds);
		}
	}

	// 추격/귀가하지 않을 때만 배회
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

FVector AFMEncounterSymbol::ArenaCenter() const
{
	// 배치된 자리에 지형 높이만 다시 맞춘다 — 레벨에 대충 띄워 놓고 배치해도 전장이 공중에 뜨지 않게
	const AFMSimManager* M = Mgr.Get();
	const float Z = M ? M->GetGroundZAt(HomeLoc.X, HomeLoc.Y) : HomeLoc.Z;
	return FVector(HomeLoc.X, HomeLoc.Y, Z);
}

float AFMEncounterSymbol::ArenaDrawRadius() const
{
	// 양 진영은 중심에서 Separation/2씩 물러서고, 개전 직후 거기서 더 움직인다.
	// 이 원에 걸치게 엄폐물을 깔면 사선에 실제로 걸린다
	return FMath::Max(Encounter.Separation * 0.5f + 250.f, 350.f);
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
