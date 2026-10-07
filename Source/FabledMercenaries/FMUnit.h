#pragma once
#include "GameFramework/Actor.h"
#include "FMUnit.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
UENUM(BlueprintType)
enum class EUnitAnim : uint8
{
	Idle, Move, Attack, Cast, Defend, Focus, Stun, Dead
};

UCLASS()
class FABLEDMERCENARIES_API AFMUnit : public AActor
{
	GENERATED_BODY()
public:
	AFMUnit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TObjectPtr<class USkeletalMeshComponent> Mesh;

	/** AnimBP가 읽는 현재 애니 상태 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit")
	EUnitAnim Anim = EUnitAnim::Idle;

	/** 마네킹 메시 정면 보정(도). 이동 방향을 안 보면 이 값 조절 (-90 / 90 / 180) */
	UPROPERTY(EditAnywhere, Category = "Unit")
	float MeshYawOffset = -90.f;
	
	/** Sim이 매 틱 호출: 위치·방향 갱신 */
	void UpdateFromSim(const FVector& Loc, const FVector& FacingDir, EUnitAnim NewAnim, float DeltaSeconds);
	
	UPROPERTY(BlueprintReadOnly, Category = "Unit")
	float Speed = 0.f;              // AnimBP 블렌드스페이스 입력 (bAlwaysRunAnim이면 이동 중 최소 RunAnimSpeed)

	/** 실제 이동 속도(cm/s) — 위치 변화량으로 잰 값 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit")
	float ActualSpeed = 0.f;

	/**
	 * 이동 중이면 실제 속도와 상관없이 달리기 모션으로 보여준다.
	 * 이속(250~360)은 그대로 두고 연출만 바꾸는 것 — 발이 살짝 미끄러져 보일 수 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "Anim")
	bool bAlwaysRunAnim = true;

	/** 달리기로 보일 블렌드스페이스 속도 (BS_Idle_Walk_Run: 300=걷기, 600=조깅) */
	UPROPERTY(EditAnywhere, Category = "Anim")
	float RunAnimSpeed = 600.f;
	
	UPROPERTY(EditAnywhere, Category="Anim") TObjectPtr<UAnimMontage> AttackMontage;
	UPROPERTY(EditAnywhere, Category="Anim") TObjectPtr<UAnimMontage> HitMontage;
	UPROPERTY(EditAnywhere, Category="Anim") TObjectPtr<UAnimMontage> DeathMontage;
	UPROPERTY(EditAnywhere, Category="Anim") TMap<int32, TObjectPtr<UAnimMontage>> SkillMontages; // 
	
	UPROPERTY(BlueprintReadOnly, Category="Unit") bool bDead = false;

	// ── 점프 — Sim엔 높이 개념이 없으므로 화면에서만 띄우는 연출 (위치 판정은 그대로 지면) ──
	/** 점프 동작 — AnimBP의 DefaultSlot으로 재생. 비우면 몸만 떠오른다 */
	UPROPERTY(EditAnywhere, Category="Anim") TObjectPtr<UAnimSequenceBase> JumpAnim;

	/** 최고점 높이(cm) */
	UPROPERTY(EditAnywhere, Category="Jump") float JumpHeight = 120.f;

	/** 이륙부터 착지까지(초) — 점프 애님도 이 시간에 맞춰 빨리/느리게 재생된다 */
	UPROPERTY(EditAnywhere, Category="Jump") float JumpDuration = 0.75f;

	/** 점프 시작. 이미 공중이거나 죽었으면 false */
	UFUNCTION(BlueprintCallable, Category="Jump") bool StartJump();

	UFUNCTION(BlueprintPure, Category="Jump") bool IsJumping() const { return JumpElapsed >= 0.f; }

	void NotifyAttackFired();
	void NotifyDamaged(bool bFromBehind, bool bCrit);
	void NotifyDeath();
	void NotifySkillCast(int32 SkillType);
	
	// 이펙트/사운드는 BP에서 붙이는 게 편함
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnAttackFired();
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnDamaged(bool bFromBehind, bool bCrit);
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnDeath();
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnSkillCast(int32 SkillType);
	
	
private:
	void PlayOneShot(UAnimMontage* M, float Rate = 1.f);
	
	FVector PrevLoc = FVector::ZeroVector;   // 속도 계산용 이전 위치
	bool bHasPrev = false;

	float JumpElapsed = -1.f;                // 점프 경과 시간. 음수 = 땅에 있음
};