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
	float OrbitSpeed = 3.3f;

	/** 궤도 회전 시 피치(위아래 각도) 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PitchMin = -80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PitchMax = -10.f;

	/** 키보드 회전 속도 (초당 도수) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float KeyRotateSpeed = 90.f;

	/**
	 * 명령 수령 가능 유닛 외곽선 포스트 프로세스 (M_PP_CommandReadyOutline).
	 * 레벨마다 PostProcessVolume을 놓는 대신 카메라에 직접 건다 — 어느 맵에서든 자동으로 먹고,
	 * 볼륨 경계를 넘나들며 꺼지는 일이 없다. 비우면 외곽선이 안 나온다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|FX")
	TObjectPtr<class UMaterialInterface> OutlineMaterial;

	/** 카메라 연출 튜닝값*/
	UPROPERTY(EditAnywhere, Category = "Camera|TurnFocus") float ExpandOutTime  = 0.40f;
	UPROPERTY(EditAnywhere, Category = "Camera|TurnFocus") float ExpandHoldTime = 0.60f;
	UPROPERTY(EditAnywhere, Category = "Camera|TurnFocus") float ExpandBackTime = 0.50f;
	UPROPERTY(EditAnywhere, Category = "Camera|TurnFocus") float ExpandCooldown = 2.00f;

	/** 화면 안쪽 안전 비율. 1.0이면 정가장자리에 1픽셀 걸친 것도 '보인다'로 친다 */
	UPROPERTY(EditAnywhere, Category = "Camera|TurnFocus") float SafeFrac = 0.75f;

	/**
	 * 턴 연출이 벌릴 수 있는 최대 붐 길이. ZoomMax(휠 한계)와 일부러 따로 둔다 —
	 * 연출은 '멀리 있는 유닛을 담는다'는 요구가 있고 휠은 조작감이 걸려 있어 기준이 다르다.
	 * 연출이 끝나면 원래 길이로 돌아오므로 이 값이 휠 범위를 넓히지는 않는다.
	 * ZoomMax보다 작게 두면 연출이 휠보다 좁아진다 — 그럴 이유는 보통 없다.
	 */
	UPROPERTY(EditAnywhere, Category = "Camera|TurnFocus") float ExpandArmMax = 2400.f;
	
private:
	enum class EExpandState : uint8 { Idle, Out, Hold, Back };

	/** 점들이 다 화면에 들어오는 최소 붐 길이. 음수면 지금도 충분히 보인다는 뜻 */
	float ComputeRequiredArm(const TArray<FVector>& Points) const;

	EExpandState ExpandState   = EExpandState::Idle;
	float ExpandFrom    = 0.f;   // 연출 전 붐 길이 — 복귀 목표
	float ExpandTo      = 0.f;   // 확장 목표
	float ExpandElapsed = 0.f;
	float CooldownLeft  = 0.f;
	
public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 카메라 패닝 (X/Y 이동) - WASD 축 입력용 (방향 -1~1) */
	void MoveCamera(FVector2D Direction);

	/** 줌 인/아웃 (양수 = 줌 인, 음수 = 줌 아웃) */
	void ZoomCamera(float Value);

	/** 마우스 드래그 궤도 회전 - 중심점(폰 위치)을 축으로 카메라를 돌림 */
	void OrbitCamera(FVector2D ScreenDelta);

	void RotateCamera(float AxisValue);   // QE 회전
	
	/** 월드에 점들이 화면에 들어오도록 줌아웃을 시작한다.
	 *  이미 다 보이거나, ZoomMax로 해도 안들어오면 아무것도 하지 않고 False
	 */
	bool TryExpandToInclude(const TArray<FVector>& Points);
	
	/** 확장 연출 중 목표 갱신 - 대상이 움직여도 따라간다. 줄어들지 않는다. */
	void RaiseExpandTarget(const TArray<FVector>& Points);
	
	bool IsExpanding() const {return ExpandState  != EExpandState::Idle;}
	
	/** 플레이어가 조작할땐 연출을 포기한다*/
	void AbortExpand();
};
