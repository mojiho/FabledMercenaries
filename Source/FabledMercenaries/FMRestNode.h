#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FMRestNode.generated.h"

/**
 * 월드맵 휴식 지점(야영지) — 아바타가 들어오면 용병 전원 부활 + 체력 회복.
 *
 * 컴뱃맵에 놓으면 동작하지 않는다 (FMSimManager.bIsWorldMap으로 막는다).
 * 들어오는 순간 한 번만 회복하고, 반경 밖으로 나갔다가 다시 들어와야 또 동작한다.
 */
UCLASS()
class FABLEDMERCENARIES_API AFMRestNode : public AActor
{
	GENERATED_BODY()

public:
	AFMRestNode();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** 아바타가 이 거리 안에 들어오면 휴식 (cm) */
	UPROPERTY(EditAnywhere, Category = "Rest")
	float RestRadius = 200.f;

	/** 아트가 붙기 전까지 휴식 반경을 화면에 그린다 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugRadius = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> Mesh;

private:
	TWeakObjectPtr<class AFMSimManager> Mgr;

	/** 지난 프레임에 아바타가 반경 안에 있었나 — 들어오는 순간만 잡기 위한 에지 검출 */
	bool bAvatarInside = false;
};
