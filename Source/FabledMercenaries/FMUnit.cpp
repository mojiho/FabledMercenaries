#include "FMUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FMItemTypes.h"
#include "Animation/AnimInstance.h"   // UAnimInstance::Montage_Play
#include "Animation/AnimMontage.h"    // UAnimMontage 정의
#include "Animation/AnimSequenceBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AFMUnit::AFMUnit()
{
	PrimaryActorTick.bCanEverTick = false;   // 매니저가 갱신하니 자체 Tick 불필요
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Mesh,TEXT("weapon_r"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);		// 바닥 트레이스가 무기를 바닥으로 잡지 않게
	WeaponMesh->SetCastShadow(true);

	OffhandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OffhandMesh"));
	OffhandMesh->SetupAttachment(Mesh, TEXT("weapon_l"));
	OffhandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 무기와 같은 이유 — 바닥 트레이스에 안 걸리게
	OffhandMesh->SetCastShadow(true);
	
	// 기본 점프 동작 — BP_Unit 디테일에서 바꿔 끼울 수 있다
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> JumpFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump"));
	if (JumpFinder.Succeeded()) JumpAnim = JumpFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> FallFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	if (FallFinder.Succeeded()) FallAnim = FallFinder.Object;
	
}

void AFMUnit::UpdateFromSim(const FVector& Loc, const FVector& FacingDir, EUnitAnim NewAnim, float DeltaSeconds)
{
	// 등장 연출 — 뒤쪽 출발점에서 진형 자리(Sim 위치)로 달려 들어오며 나타난다
	bool bEntering = false;
	if (EntranceTime >= 0.f)
	{
		EntranceTime += DeltaSeconds;
		const float Active = EntranceTime - EntranceDelay;                 // 음수 = 아직 출발 전 (안 보이는 채 대기)
		const float T      = FMath::Clamp(Active / FMath::Max(EntranceRunTime, 0.05f), 0.f, 1.f);
		CurrentEntranceOffset = EntranceFrom * (1.f - FMath::InterpEaseOut(0.f, 1.f, T, 2.f));   // 도착할수록 감속
		SetFadeAlpha(FMath::Clamp(Active / FMath::Max(EntranceFadeTime, 0.05f), 0.f, 1.f));
		bEntering = T < 1.f;

		if (T >= 1.f && Active >= EntranceFadeTime)
		{
			EntranceTime = -1.f;                  // 끝 — 완전히 보이고 제자리
			CurrentEntranceOffset = FVector::ZeroVector;
			SetFadeAlpha(1.f);
		}
	}
	const FVector VisLoc(Loc.X + CurrentEntranceOffset.X, Loc.Y + CurrentEntranceOffset.Y, Loc.Z);

	// 속도 = (화면) 위치 변화량 / dt (이전 위치가 있을 때만)
	const float HorizMove = bHasPrev ? FVector::Dist2D(VisLoc, PrevLoc) : 0.f;
	if (bHasPrev && DeltaSeconds > 0.f)
		ActualSpeed = HorizMove / DeltaSeconds;

	// 움직이는 중이면 블렌드스페이스를 달리기 지점으로 — 멈추면 0이라 대기 모션 그대로
	const bool bMoving = ActualSpeed > 5.f;
	Speed = (bAlwaysRunAnim && bMoving) ? FMath::Max(ActualSpeed, RunAnimSpeed) : ActualSpeed;
	PrevLoc = VisLoc;
	bHasPrev = true;

	// 지면 높이(Loc.Z)를 그대로 쓰지 않고 발 높이를 중력으로 굴린다 — 경사는 따라가고, 턱에선 떨어지고, 점프는 포물선
	UpdateVertical(Loc.Z, HorizMove, DeltaSeconds);
	const FVector DrawLoc(VisLoc.X, VisLoc.Y, VisualZ);

	SetActorLocation(DrawLoc);

	// 달려 들어오는 동안은 진행 방향을 보고 달리기 상태로 (Sim 유닛은 이미 제자리에 서 있어 Idle이다)
	if (bEntering && !bDead && NewAnim != EUnitAnim::Dead)
	{
		const FVector RunDir = -EntranceFrom.GetSafeNormal2D();
		if (!RunDir.IsNearlyZero())
		{
			FRotator R = RunDir.Rotation();
			R.Yaw += MeshYawOffset;
			SetActorRotation(R);
		}
		Anim = (ActualSpeed > 5.f) ? EUnitAnim::Move : EUnitAnim::Idle;
		return;
	}

	if (!FacingDir.IsNearlyZero())
	{
		FRotator R = FacingDir.Rotation();
		R.Yaw += MeshYawOffset;
		SetActorRotation(R);
	}
	// 무기 공격 동작이 있으면 선딜 동안 AnimBP의 맨손 공격(펀치) 상태로 가지 않게 — 대기 자세로 있다가 타격 순간 휘두른다
	Anim = (NewAnim == EUnitAnim::Attack && WeaponAttackAnims.Num() > 0) ? EUnitAnim::Idle : NewAnim;
}

void AFMUnit::UpdateVertical(float GroundZ, float HorizMove, float DeltaSeconds)
{
	// 첫 프레임 / 시체 — 바로 바닥에
	if (!bHasVisualZ || bDead)
	{
		VisualZ = GroundZ;
		bHasVisualZ = true;
		if (bAirborne) Land();
		return;
	}

	if (bAirborne)
	{
		VertVel -= Gravity * DeltaSeconds;
		VisualZ += VertVel * DeltaSeconds;
		AirTime += DeltaSeconds;

		// 걸어서 떨어질 때만 낙하 루프 (점프는 점프 모션 유지). 작은 턱에서 깜빡이지 않게 잠깐 뒤에
		if (!bJumped && !FallMontage.IsValid() && FallAnim && AirTime >= FallAnimDelay && Mesh)
			if (UAnimInstance* Inst = Mesh->GetAnimInstance())
				FallMontage = Inst->PlaySlotAnimationAsDynamicMontage(FallAnim, TEXT("DefaultSlot"), 0.1f, 0.15f, 1.f, 1000);

		if (VisualZ <= GroundZ)
		{
			VisualZ = GroundZ;
			if (VertVel <= 0.f) Land();          // 내려오다 바닥에 닿음 = 착지 (올라가는 중이면 바닥을 따라 밀려 올라감)
		}
		return;
	}

	// 이번 프레임 수평 이동량으로 '걸어서 내려갈 수 있는' 최대 낙차 — 넘으면 턱/낭떠러지
	const float WalkableDrop = HorizMove * FMath::Tan(FMath::DegreesToRadians(WalkableSlopeDeg)) + 2.f;
	const float Diff = GroundZ - VisualZ;

	if (Diff < -WalkableDrop)
	{
		bAirborne = true;                        // 걸어 나감 — 초속 0으로 낙하 시작
		bJumped   = false;
		VertVel   = 0.f;
		AirTime   = 0.f;
	}
	else if (Diff > WalkableDrop)
	{
		// 높은 턱은 SimManager가 막으므로 여기 오는 건 허용치 이하의 낮은 턱 — 툭 튀지 않게 빠르게 따라붙는다
		VisualZ = FMath::FInterpTo(VisualZ, GroundZ, DeltaSeconds, StepUpInterpSpeed);
		if (GroundZ - VisualZ < 1.f) VisualZ = GroundZ;
	}
	else
	{
		VisualZ = GroundZ;                       // 걸을 수 있는 경사 — 바닥에 붙는다
	}
}

void AFMUnit::Land()
{
	bAirborne = false;
	bJumped   = false;
	VertVel   = 0.f;
	AirTime   = 0.f;
	if (UAnimMontage* M = FallMontage.Get())
		if (Mesh)
			if (UAnimInstance* Inst = Mesh->GetAnimInstance())
				Inst->Montage_Stop(0.15f, M);
	FallMontage.Reset();
}

void AFMUnit::PlayEntrance(const FVector& FromOffset, float RunTime, float Delay)
{
	// 시체로 스폰된 용병은 달려오지 않고 제자리에서 나타나기만
	const bool bCorpse = bDead || Anim == EUnitAnim::Dead;
	EntranceFrom          = bCorpse ? FVector::ZeroVector : FVector(FromOffset.X, FromOffset.Y, 0.f);
	CurrentEntranceOffset = EntranceFrom;
	EntranceRunTime       = RunTime;
	EntranceDelay         = Delay;
	EntranceTime          = 0.f;
	bHasPrev              = false;   // 출발점으로 순간이동하는 첫 프레임을 '달리기'로 재지 않게
	bHasVisualZ           = false;
	SetFadeAlpha(0.f);
}

void AFMUnit::SetFadeAlpha(float Alpha)
{
	if (!Mesh) return;
	Mesh->SetScalarParameterValueOnMaterials(TEXT("FadeAlpha"), Alpha);
	Mesh->SetVisibility(Alpha > 0.01f);   // 디더는 0에서도 점이 남을 수 있다 — 완전히 숨긴다
	if (WeaponMesh)  WeaponMesh->SetVisibility(Alpha > 0.99f);  // 몸이 다 나타난 뒤에 무기 표시
	if (OffhandMesh) OffhandMesh->SetVisibility(Alpha > 0.99f);
}

bool AFMUnit::StartJump()
{
	if (bDead || bAirborne || !bHasVisualZ) return false;

	// 최고점 = JumpHeight가 되는 초속: v0 = √(2gh), 체공 = 2·v0/g (같은 높이에 착지할 때)
	const float G = FMath::Max(Gravity, 1.f);
	VertVel   = FMath::Sqrt(2.f * G * JumpHeight);
	bAirborne = true;
	bJumped   = true;
	AirTime   = 0.f;

	if (JumpAnim && Mesh)
		if (UAnimInstance* Inst = Mesh->GetAnimInstance())
		{
			// 애님 길이를 체공 시간에 맞춘다 — 같은 높이에 내릴 때 착지 순간 동작도 같이 끝나게
			const float AirDuration = 2.f * VertVel / G;
			const float Rate = JumpAnim->GetPlayLength() / FMath::Max(AirDuration, 0.05f);
			Inst->PlaySlotAnimationAsDynamicMontage(JumpAnim, TEXT("DefaultSlot"), 0.05f, 0.15f, Rate);
		}
	return true;
}

void AFMUnit::PlayOneShot(UAnimMontage* M, float Rate)
{
	if (!M || !Mesh) return;
	if (UAnimInstance* Inst = Mesh->GetAnimInstance())
		Inst->Montage_Play(M, Rate);
}

void AFMUnit::NotifyAttackFired()
{
	// 무기 공격 동작이 있으면 콤보 순서대로 (A → B → C → A …). 타격 순간에 시작한다
	if (WeaponAttackAnims.Num() > 0 && Mesh)
	{
		UAnimSequenceBase* SwingAnim = WeaponAttackAnims[AttackComboIndex % WeaponAttackAnims.Num()];
		AttackComboIndex = (AttackComboIndex + 1) % WeaponAttackAnims.Num();
		if (SwingAnim)
			if (UAnimInstance* Inst = Mesh->GetAnimInstance())
				Inst->PlaySlotAnimationAsDynamicMontage(SwingAnim, TEXT("DefaultSlot"), 0.08f, 0.25f, WeaponAttackRate);
	}
	else
	{
		PlayOneShot(AttackMontage);
	}
	BP_OnAttackFired();
}

void AFMUnit::NotifyDamaged(bool bFromBehind, bool bCrit)
{
	if (bCrit) PlayOneShot(HitMontage);   // 매 타격마다 끊으면 지저분함 → 크리만 권장
	BP_OnDamaged(bFromBehind, bCrit);
}

void AFMUnit::NotifyDeath()
{
	bDead = true;
	PlayOneShot(DeathMontage);
	BP_OnDeath();
}

void AFMUnit::NotifySkillCast(int32 SkillType)
{
	if (TObjectPtr<UAnimMontage>* M = SkillMontages.Find(SkillType))
		PlayOneShot(M->Get());
	BP_OnSkillCast(SkillType);
}

void AFMUnit::BeginPlay()
{
	Super::BeginPlay();
	UnarmedAttackMontage = AttackMontage;   // BP_Unit에 지정된 기본(맨손) 공격 모션 기억
}

void AFMUnit::SetWeaponVisual(const FFMWeaponVisual* V)
{
	WeaponMesh->SetStaticMesh(V ? V->Mesh.Get() : nullptr);
	if (V)
	{
		WeaponMesh->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, V->Socket);
		WeaponMesh->SetRelativeScale3D(FVector(V->Scale));
	}
	AttackMontage = (V && V->AttackMontage) ? V->AttackMontage.Get() : UnarmedAttackMontage.Get();
	// 로드 안 된 동작(예: Paragon 팩이 없는 PC)은 빼고 담는다 — 하나도 없으면 맨손 공격 모션으로 돌아간다
	WeaponAttackAnims.Reset();
	if (V)
		for (UAnimSequenceBase* A : V->AttackAnims)
			if (A) WeaponAttackAnims.Add(A);
	WeaponAttackRate = V ? FMath::Max(V->AttackAnimRate, 0.05f) : 1.f;
	AttackComboIndex = 0;

	// 왼손 — 세트 무기면 방패/두 번째 무기, 아니면 비운다
	UStaticMesh* Off = V ? V->OffhandMesh.Get() : nullptr;
	OffhandMesh->SetStaticMesh(Off);
	if (Off)
	{
		OffhandMesh->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, V->OffhandSocket);
		OffhandMesh->SetRelativeScale3D(FVector(V->OffhandScale));
	}
}