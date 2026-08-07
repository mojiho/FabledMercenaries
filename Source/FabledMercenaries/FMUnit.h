#pragma once
#include "GameFramework/Actor.h"
#include "FMUnit.generated.h"

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
	float Speed = 0.f;
	
private:
	FVector PrevLoc = FVector::ZeroVector;   // 속도 계산용 이전 위치
	bool bHasPrev = false;
};