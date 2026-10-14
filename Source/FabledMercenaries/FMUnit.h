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

class UStaticMeshComponent;
struct FFMWeaponVisual;

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

	// ── 수직 이동 (점프·낙하) ──
	// Sim은 높이를 모르고 매 프레임 지면 위치만 준다. 화면 높이(발 높이)는 여기서 중력으로 따로 굴린다.
	// 점프 = 위로 초속을 주고 공중 상태로, 턱에서 걸어 나감 = 초속 0으로 공중 상태로 — 둘 다 같은 낙하 처리.
	// 발 높이(GetFootZ)는 SimManager가 '턱을 오를 수 있나' 판정에 쓴다 → 높은 턱은 점프해야 오른다.

	/** 점프 동작 — AnimBP의 DefaultSlot으로 재생. 비우면 몸만 떠오른다 */
	UPROPERTY(EditAnywhere, Category="Anim") TObjectPtr<UAnimSequenceBase> JumpAnim;

	/** 점프 최고 높이(cm) — 이 높이 + 턱 오르기 허용치(SimManager MaxStepUpHeight)까지의 턱을 넘을 수 있다 */
	UPROPERTY(EditAnywhere, Category="Jump") float JumpHeight = 120.f;

	/** 중력(cm/s²) — 점프·낙하 공통. 탑뷰에서 둥둥 떠 보이지 않게 실제(980)보다 세게 */
	UPROPERTY(EditAnywhere, Category="Jump") float Gravity = 2400.f;

	/** 점프 시작. 이미 공중이거나 죽었으면 false */
	UFUNCTION(BlueprintCallable, Category="Jump") bool StartJump();

	UFUNCTION(BlueprintPure, Category="Jump") bool IsJumping() const { return bAirborne && bJumped; }
	UFUNCTION(BlueprintPure, Category="Jump") bool IsInAir() const { return bAirborne; }

	/** 이 각도까지의 경사는 바닥에 붙어 걷는다. 그보다 가파르게 꺼지면 떨어진다 */
	UPROPERTY(EditAnywhere, Category="Jump") float WalkableSlopeDeg = 50.f;

	/** 낮은 턱(오르기 허용치 이하)을 오를 때 따라붙는 속도 (FInterpTo 속도) */
	UPROPERTY(EditAnywhere, Category="Jump") float StepUpInterpSpeed = 14.f;

	/** 낙하 동작 (루프) — 걸어서 떨어질 때, 이만큼 떨어진 뒤에야 재생 (작은 턱에서 깜빡이지 않게) */
	UPROPERTY(EditAnywhere, Category="Anim") TObjectPtr<UAnimSequenceBase> FallAnim;
	UPROPERTY(EditAnywhere, Category="Jump") float FallAnimDelay = 0.12f;

	// ── 전투 등장 연출 ──
	/**
	 * FromOffset만큼 떨어진 곳(진영 뒤쪽 맵 끝)에서 Delay초 뒤 출발해 RunTime초 동안 달려 들어오며 서서히 나타난다.
	 * Sim 위치는 이미 진형 자리 — 액터만 뒤에서 따라붙는 화면 연출. 시체(사망 상태로 스폰)는 제자리에서 나타나기만 한다.
	 */
	void PlayEntrance(const FVector& FromOffset, float RunTime, float Delay);

	/** 지금 Sim 위치 대비 화면 위치 차이 (등장 연출 중에만 0이 아님) — SimManager가 그 자리 바닥 높이를 잴 때 쓴다 */
	FVector GetEntranceOffset() const { return CurrentEntranceOffset; }

	/** 서서히 나타나는 시간(초) — 달려오기 시작할 때부터 */
	UPROPERTY(EditAnywhere, Category="Anim") float EntranceFadeTime = 0.7f;

	/**
	 * 투명도 (0=안 보임 1=보임). 머티리얼의 'FadeAlpha' 스칼라 파라미터를 바꾼다 —
	 * 파라미터가 없는 머티리얼이면 효과 없음 (M_Mannequin에는 디더 마스크로 들어가 있다)
	 */
	UFUNCTION(BlueprintCallable, Category="Anim") void SetFadeAlpha(float Alpha);

	/** 지금 화면상 발 높이. 아직 한 번도 갱신 안 됐으면 false */
	bool GetFootZ(float& OutZ) const { OutZ = VisualZ; return bHasVisualZ; }

	void NotifyAttackFired();
	void NotifyDamaged(bool bFromBehind, bool bCrit);
	void NotifyDeath();
	void NotifySkillCast(int32 SkillType);
	
	// 이펙트/사운드는 BP에서 붙이는 게 편함
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnAttackFired();
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnDamaged(bool bFromBehind, bool bCrit);
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnDeath();
	UFUNCTION(BlueprintImplementableEvent, Category="Unit") void BP_OnSkillCast(int32 SkillType);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Unit")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;
	
	/** 무기 외형 적용. null 이면 맨손 **/
	void SetWeaponVisual(const FFMWeaponVisual* Visual);

	/** 왼손 — 방패(shield_l 소켓) 또는 쌍수 무기(weapon_l 소켓). 세트 무기 아이템이 채운다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Unit")
	TObjectPtr<UStaticMeshComponent> OffhandMesh;

private:
	void PlayOneShot(UAnimMontage* M, float Rate = 1.f);
	
	FVector PrevLoc = FVector::ZeroVector;   // 속도 계산용 이전 위치
	bool bHasPrev = false;

	// 등장 연출
	FVector EntranceFrom = FVector::ZeroVector;      // 출발 오프셋 (Sim 위치 기준)
	FVector CurrentEntranceOffset = FVector::ZeroVector;
	float   EntranceTime = -1.f;                     // 음수 = 연출 없음
	float   EntranceRunTime = 1.f;
	float   EntranceDelay = 0.f;

	// 수직 상태
	void  UpdateVertical(float GroundZ, float HorizMove, float DeltaSeconds);
	void  Land();
	float VisualZ = 0.f;                     // 발 높이 (화면)
	bool  bHasVisualZ = false;
	bool  bAirborne = false;                 // 점프 중이거나 떨어지는 중
	bool  bJumped = false;                   // 공중에 뜬 이유가 점프인가 (낙하 모션 대신 점프 모션 유지)
	float VertVel = 0.f;                     // 수직 속도(cm/s, 위가 +)
	float AirTime = 0.f;
	TWeakObjectPtr<UAnimMontage> FallMontage; // 재생 중인 낙하 루프 — 착지 때 멈춘다
	
	virtual void BeginPlay() override;
	
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> UnarmedAttackMontage;

	/** 장착 무기의 공격 동작들 (콤보 순서) — 비어 있으면 AttackMontage */
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequenceBase>> WeaponAttackAnims;
	float WeaponAttackRate = 1.f;
	int32 AttackComboIndex = 0;		// 무기를 뺄 때 돌아갈 모션
};