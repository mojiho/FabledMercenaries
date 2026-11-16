#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FMEncounter.h"
#include "FMEncounterSymbol.generated.h"

/**
 * 컴뱃맵을 배회하는 적 심볼 (유니콘 오버로드식 조우).
 *
 * 탐험 중 아바타가 TriggerRadius 안에 들어오면 자기가 들고 있는 인카운터로
 * 그 자리에서 전투를 연다. 전투는 레벨 전환 없이 같은 맵에서 벌어지고,
 * 승리하면 심볼이 사라진다.
 */
UCLASS()
class FABLEDMERCENARIES_API AFMEncounterSymbol : public AActor
{
	GENERATED_BODY()

public:
	AFMEncounterSymbol();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** 이 심볼이 열 전투의 편성 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	FFMEncounterDef Encounter;

	/** 아바타가 이 거리 안에 들어오면 전투 시작 (cm) */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float TriggerRadius = 150.f;

	/** 패배 후 이 시간 동안은 다시 걸리지 않는다 — 재접촉 무한루프 방지 (초) */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	float RetriggerCooldown = 3.f;

	/**
	 * 전장 중심을 심볼이 '배치된 자리'(HomeLoc)로 고정할지.
	 *
	 * 끄면 예전 동작 — 심볼과 플레이어의 중간 지점에서 싸운다. 그러면 전장이 매번
	 * 다른 곳에 생기므로 엄폐물을 깔아 둬도 거기서 싸울지는 운이다.
	 * 켜면 이 심볼의 전투는 언제나 같은 지형에서 벌어진다 — 레벨에 전장을 설계할 수 있다.
	 * 붙잡힌 자리에서 전장까지 끌려오는 건 암전 중에 일어나 화면에는 안 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bArenaAtHome = true;

	/**
	 * 진형 축도 심볼을 배치할 때의 방향(액터 Yaw)으로 고정할지. bArenaAtHome이 켜져 있어야 쓴다.
	 *
	 * 중심만 고정하면 플레이어가 어느 쪽에서 붙잡히느냐에 따라 양 진영이 통째로 회전한다.
	 * 그러면 전장 가운데 깔아 둔 엄폐물이 사선에 걸릴지, 아니면 유닛 스폰 자리에 걸릴지가
	 * 매번 달라진다 — 축까지 고정해야 "여기 벽을 세우면 이 전투는 엄폐전이 된다"가 성립한다.
	 * 플레이어가 반대편에서 붙잡히면 축을 뒤집어 아군이 늘 플레이어 쪽에 서게 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bArenaAxisFromSymbol = true;

	/** 아바타를 발견하고 쫓기 시작하는 거리. TriggerRadius보다 커야 의미가 있다 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float ChaseRadius = 700.f;

	/** 시작 지점 주변 이 반경을 배회. 0이면 제자리에 선다 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolRadius = 400.f;

	/**
	 * 집에서 이 거리를 넘어가면 추격을 끊고 돌아간다 (cm).
	 *
	 * 없으면 ChaseRadius는 '추격 시작' 조건일 뿐이라, 한 번 걸린 심볼이 맵 끝까지 따라온다.
	 * 그러면 전투가 맵 어디서든 열려서 지형 설계가 성립하지 않는다.
	 * PatrolRadius보다 커야 한다 — 작으면 배회만 해도 리쉬에 걸려 제자리를 맴돈다.
	 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float LeashRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float MoveSpeed = 220.f;

	/** 아트가 붙기 전까지 조우 반경을 화면에 그린다 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugRadius = true;

	/**
	 * 배회할 때 보여줄 걸어다니는 아바타. 비우면 SimManager의 UnitClass(BP_Unit)를 쓴다.
	 * 아바타가 뜨면 아래 Mesh(구체)는 자동으로 숨는다 — 클래스를 못 찾으면 구체로 남는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Avatar")
	TSubclassOf<class AFMUnit> AvatarClass;

	/** 아바타를 쓰지 않고 예전처럼 메시만 보여주고 싶을 때 */
	UPROPERTY(EditAnywhere, Category = "Avatar")
	bool bUseAvatar = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> Mesh;

	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void SpawnAvatar();

	/** 아바타를 지금 심볼 위치·방향으로 갱신. 지형 높이는 SimManager에서 받아 쓴다 */
	void UpdateAvatar(float DeltaSeconds, bool bMoving);

	/** 배회/추격으로 실제 보여줄 아바타 액터 (심볼에 붙이지 않고 따로 띄운다 — 위치를 직접 넣어주므로) */
	UPROPERTY(Transient)
	TObjectPtr<class AFMUnit> Avatar;

	/** 아바타가 바라볼 방향 (마지막으로 움직인 쪽) */
	FVector AvatarFacing = FVector::ForwardVector;

	/** SimManager의 OnCombatEnded 구독 — 내가 연 전투일 때만 반응한다 */
	UFUNCTION()
	void HandleCombatEnded(bool bVictory);

	void PickPatrolTarget();

	/** 전장 중심 — 배치된 자리에 지형 높이만 맞춘 것. bArenaAtHome이 꺼져 있어도 디버그 표시에 쓴다 */
	FVector ArenaCenter() const;

	/** 전장 표시 원의 반경 — 양 진영이 벌어져 서는 폭을 덮는다 */
	float ArenaDrawRadius() const;

	/** @return 실제로 움직였으면 true (걷기 애니를 켤지 판단) */
	bool MoveTowardXY(const FVector& Target, float DeltaSeconds);

	TWeakObjectPtr<class AFMSimManager> Mgr;

	FVector  HomeLoc      = FVector::ZeroVector;  // 배회 중심 (배치된 자리) = 전장 자리
	FRotator HomeRot      = FRotator::ZeroRotator; // 배치된 방향 = 진형 축 (배회로 돌아간 현재 회전이 아니다)
	FVector  PatrolTarget = FVector::ZeroVector;

	/**
	 * 리쉬에 걸려 귀가하는 중. 집 근처(PatrolRadius 안)로 돌아와야 풀린다.
	 * 경계에서 바로 풀면 추격↔귀가를 한 프레임씩 번갈아 하며 제자리에서 떤다.
	 */
	bool    bReturningHome = false;
	bool    bEngaged     = false;                 // 이 심볼이 연 전투가 진행 중인가
	float   CooldownLeft = 0.f;
};
