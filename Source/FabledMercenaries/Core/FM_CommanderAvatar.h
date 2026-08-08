#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Sim/CombatSim.h"
#include "FM_CommanderAvatar.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

/**
 * 지휘관 아바타 — P0 와이어링.
 * 엔진 비의존 CombatSim을 소유하고, 매 틱 Sim을 진행시킨 뒤
 * Sim 내 자기 Unit의 pos를 읽어 액터 위치를 동기화한다.
 */
UCLASS()
class FABLEDMERCENARIES_API AFM_CommanderAvatar : public APawn
{
	GENERATED_BODY()

public:
	AFM_CommanderAvatar();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** PlayerController가 호출 — 클릭한 월드 좌표로 이동 명령 발행 */
	void IssueMove(const FVector& WorldPos, bool bReserve = false);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 디버그: BeginPlay에서 자동으로 이동 명령을 한 번 발행 (검증용) */
	UPROPERTY(EditAnywhere, Category="Sim|Debug")
	bool bAutoIssueOnBegin = true;

	/** 디버그: 자동 이동 목표 오프셋 (현재 위치 기준 상대) */
	UPROPERTY(EditAnywhere, Category="Sim|Debug")
	FVector AutoMoveOffset = FVector(1000.f, 0.f, 0.f);

private:
	/** 순수 C++ Sim 인스턴스 (P0: Pawn이 소유. 나중에 매니저로 분리 예정) */
	CombatSim Sim;

	/** 이 아바타의 Sim 내 Unit ID */
	uint64_t MyUnitId = 1;

	/** 지휘관(플레이어) ID — P0에선 고정 */
	static constexpr uint64_t COMMANDER_ID = 1;
};
