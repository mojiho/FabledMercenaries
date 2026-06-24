#include "Core/FM_CameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

AFM_CameraPawn::AFM_CameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 루트 씬 컴포넌트 (카메라 위치 앵커)
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// SpringArm - 쿼터뷰 각도 설정
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-50.f, 0.f, 0.f)); // 쿼터뷰 각도
	CameraBoom->bDoCollisionTest = false;         // 카메라가 지형에 막히지 않게
	CameraBoom->bInheritPitch = false;            // 패닝해도 각도 고정
	CameraBoom->bInheritYaw   = false;
	CameraBoom->bInheritRoll  = false;

	// Camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;
}

void AFM_CameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFM_CameraPawn::MoveCamera(FVector2D Direction)
{
	// XY 평면 이동 (쿼터뷰이므로 World 기준으로 이동)
	FVector Delta = FVector(Direction.X, Direction.Y, 0.f) * PanSpeed * GetWorld()->GetDeltaSeconds();
	AddActorWorldOffset(Delta);
}

void AFM_CameraPawn::ZoomCamera(float Value)
{
	float NewLength = CameraBoom->TargetArmLength - (Value * ZoomSpeed);
	CameraBoom->TargetArmLength = FMath::Clamp(NewLength, ZoomMin, ZoomMax);
}
