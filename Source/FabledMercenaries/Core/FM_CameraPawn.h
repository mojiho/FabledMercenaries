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

	/** 마우스 드래그 궤도 회전 감도 (마우스 1픽셀당 회전 도수) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float OrbitSpeed = 0.3f;

	/** 궤도 회전 시 피치(위아래 각도) 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PitchMin = -80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PitchMax = -10.f;

	/** 키보드 회전 속도 (초당 도수) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float KeyRotateSpeed = 90.f;
public:
	virtual void Tick(float DeltaTime) override;

	/** 카메라 패닝 (X/Y 이동) - WASD 축 입력용 (방향 -1~1) */
	void MoveCamera(FVector2D Direction);

	/** 줌 인/아웃 (양수 = 줌 인, 음수 = 줌 아웃) */
	void ZoomCamera(float Value);

	/** 마우스 드래그 궤도 회전 - 중심점(폰 위치)을 축으로 카메라를 돌림 */
	void OrbitCamera(FVector2D ScreenDelta);

	void RotateCamera(float AxisValue);   // QE 회전
};
