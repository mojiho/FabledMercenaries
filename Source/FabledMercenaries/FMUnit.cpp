#include "FMUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"   // UAnimInstance::Montage_Play
#include "Animation/AnimMontage.h"    // UAnimMontage 정의


AFMUnit::AFMUnit()
{
	PrimaryActorTick.bCanEverTick = false;   // 매니저가 갱신하니 자체 Tick 불필요
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void AFMUnit::UpdateFromSim(const FVector& Loc, const FVector& FacingDir, EUnitAnim NewAnim, float DeltaSeconds)
{
	// 속도 = 위치 변화량 / dt (이전 위치가 있을 때만)
	if (bHasPrev && DeltaSeconds > 0.f)
		Speed = FVector::Dist2D(Loc, PrevLoc) / DeltaSeconds;
	PrevLoc = Loc;
	bHasPrev = true;

	SetActorLocation(Loc);
	if (!FacingDir.IsNearlyZero())
	{
		FRotator R = FacingDir.Rotation();
		R.Yaw += MeshYawOffset;
		SetActorRotation(R);
	}
	Anim = NewAnim;
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

