#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FM_CameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * 전략 게임의 카메라 Pawn.
 * 캐릭터와 분리된 독립적인 카메라로 맵을 자유롭게 패닝/줌할 수 있다.
 */
UCLASS()
class FABLEDMERCENARIES_API AFM_CameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AFM_CameraPawn();

protected:
	/** 카메라 붐 - 쿼터뷰 각도와 줌 거리 제어 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 카메라 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	/** 카메라 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PanSpeed = 800.f;

	/** 줌 최솟값 (SpringArm 길이) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomMin = 400.f;

	/** 줌 최댓값 (SpringArm 길이) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomMax = 1600.f;

	/** 줌 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomSpeed = 100.f;

public:
	virtual void Tick(float DeltaTime) override;

	/** 카메라 패닝 (X/Y 이동) */
	void MoveCamera(FVector2D Direction);

	/** 줌 인/아웃 (양수 = 줌 인, 음수 = 줌 아웃) */
	void ZoomCamera(float Value);
};
