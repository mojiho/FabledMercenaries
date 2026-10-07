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
}

void AFMUnit::UpdateFromSim(const FVector& Loc, const FVector& FacingDir, EUnitAnim NewAnim, float DeltaSeconds)
{
	// 속도 = 위치 변화량 / dt (이전 위치가 있을 때만)
	if (bHasPrev && DeltaSeconds > 0.f)
		ActualSpeed = FVector::Dist2D(Loc, PrevLoc) / DeltaSeconds;

	// 움직이는 중이면 블렌드스페이스를 달리기 지점으로 — 멈추면 0이라 대기 모션 그대로
	const bool bMoving = ActualSpeed > 5.f;
	Speed = (bAlwaysRunAnim && bMoving) ? FMath::Max(ActualSpeed, RunAnimSpeed) : ActualSpeed;
	PrevLoc = Loc;
	bHasPrev = true;

	// 점프 중이면 포물선만큼 띄운다. Speed/PrevLoc은 지면 기준 그대로 — 달리기 블렌드가 흔들리지 않게
	FVector DrawLoc = Loc;
	if (JumpElapsed >= 0.f)
	{
		JumpElapsed += DeltaSeconds;
		const float T = JumpElapsed / FMath::Max(JumpDuration, 0.05f);
		if (T >= 1.f) JumpElapsed = -1.f;                          // 착지
		else          DrawLoc.Z += 4.f * JumpHeight * T * (1.f - T);  // 0 → 최고점(T=0.5) → 0
	}

	SetActorLocation(DrawLoc);
	if (!FacingDir.IsNearlyZero())
	{
		FRotator R = FacingDir.Rotation();
		R.Yaw += MeshYawOffset;
		SetActorRotation(R);
	}
	Anim = NewAnim;
}

bool AFMUnit::StartJump()
{
	if (bDead || IsJumping()) return false;
	JumpElapsed = 0.f;

	if (JumpAnim && Mesh)
		if (UAnimInstance* Inst = Mesh->GetAnimInstance())
		{
			// 애님 길이를 체공 시간에 맞춘다 — 착지 순간 동작도 같이 끝나게
			const float Rate = JumpAnim->GetPlayLength() / FMath::Max(JumpDuration, 0.05f);
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

