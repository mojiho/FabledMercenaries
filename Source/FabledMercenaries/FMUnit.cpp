#include "FMUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"   // UAnimInstance::Montage_Play
#include "Animation/AnimMontage.h"    // UAnimMontage 정의
#include "Animation/AnimSequenceBase.h"
#include "UObject/ConstructorHelpers.h"


AFMUnit::AFMUnit()
{
	PrimaryActorTick.bCanEverTick = false;   // 매니저가 갱신하니 자체 Tick 불필요
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

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
	// 속도 = 위치 변화량 / dt (이전 위치가 있을 때만)
	const float HorizMove = bHasPrev ? FVector::Dist2D(Loc, PrevLoc) : 0.f;
	if (bHasPrev && DeltaSeconds > 0.f)
		ActualSpeed = FVector::Dist2D(Loc, PrevLoc) / DeltaSeconds;

	// 움직이는 중이면 블렌드스페이스를 달리기 지점으로 — 멈추면 0이라 대기 모션 그대로
	const bool bMoving = ActualSpeed > 5.f;
	Speed = (bAlwaysRunAnim && bMoving) ? FMath::Max(ActualSpeed, RunAnimSpeed) : ActualSpeed;
	PrevLoc = Loc;
	bHasPrev = true;

	// 지면 높이(Loc.Z)를 그대로 쓰지 않고 발 높이를 중력으로 굴린다 — 경사는 따라가고, 턱에선 떨어지고, 점프는 포물선
	UpdateVertical(Loc.Z, HorizMove, DeltaSeconds);
	const FVector DrawLoc(Loc.X, Loc.Y, VisualZ);

	SetActorLocation(DrawLoc);
	if (!FacingDir.IsNearlyZero())
	{
		FRotator R = FacingDir.Rotation();
		R.Yaw += MeshYawOffset;
		SetActorRotation(R);
	}
	Anim = NewAnim;
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
	PlayOneShot(AttackMontage);
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

