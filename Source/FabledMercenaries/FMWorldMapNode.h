#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FMWorldMapNode.generated.h"

/**
 * 월드맵 위의 진입 지점 — 아바타가 다가오면 그 지역의 컴뱃맵을 연다.
 *
 * 심볼(AFMEncounterSymbol)과 역할이 다르다:
 *   월드맵 노드 = 레벨 전환(OpenLevel), 컴뱃맵 심볼 = 같은 레벨 안에서 전투 시작.
 */
UCLASS()
class FABLEDMERCENARIES_API AFMWorldMapNode : public AActor
{
	GENERATED_BODY()

public:
	AFMWorldMapNode();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** 이 노드가 여는 컴뱃맵 레벨 */
	UPROPERTY(EditAnywhere, Category = "World")
	TSoftObjectPtr<UWorld> CombatMap;

	/** 복귀 지점 식별자 — 컴뱃맵에서 월드맵으로 돌아올 때 이 노드 앞에 세운다 */
	UPROPERTY(EditAnywhere, Category = "World")
	FName NodeId;

	/** 아바타가 이 거리 안에 들어오면 진입 (cm) */
	UPROPERTY(EditAnywhere, Category = "World")
	float EnterRadius = 200.f;

	/**
	 * 이 노드로 복귀했을 때 아바타를 세울 위치 (노드 로컬 기준).
	 * 노드 위에 그대로 세우면 진입 반경 안이라 즉시 다시 들어가버린다.
	 */
	UPROPERTY(EditAnywhere, Category = "World")
	FVector ReturnOffset = FVector(-300.f, 0.f, 0.f);

	/** 복귀 직후 이 시간 동안은 진입 판정을 하지 않는다 (초) — 되돌아가기 루프 방지 */
	UPROPERTY(EditAnywhere, Category = "World")
	float ReturnGrace = 2.f;

	/** 아트가 붙기 전까지 진입 반경을 화면에 그린다 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugRadius = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> Mesh;

private:
	TWeakObjectPtr<class AFMSimManager> Mgr;

	/** OpenLevel은 즉시 끝나지 않는다 — 남은 프레임에 중복 호출되는 걸 막는 빗장 */
	bool bEntering = false;

	/** 복귀 배치를 첫 Tick에 한 번만 시도 — 매니저 BeginPlay 순서를 보장할 수 없어서 */
	bool  bReturnHandled = false;
	float GraceLeft      = 0.f;
};
