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
	
	/** Sim이 매 틱 호출: 위치·방향 갱신 */
	void UpdateFromSim(const FVector& Loc, const FVector& FacingDir, EUnitAnim NewAnim);
};