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
	float Yaw = CameraBoom->GetRelativeRotation().Yaw;         // 카메라가 돌아간 각도
	FRotator YawRot(0.f, Yaw, 0.f);
	FVector Forward = YawRot.RotateVector(FVector::ForwardVector); // 화면에서 "위쪽"
	FVector Right = YawRot.RotateVector(FVector::RightVector);   // 화면에서 "오른쪽"
	FVector Delta = (Forward * Direction.Y + Right * Direction.X) // W/S=앞뒤, A/D=좌우
		* PanSpeed * GetWorld()->GetDeltaSeconds();
	AddActorWorldOffset(Delta);                                 // 폰(중심점) 이동
}

void AFM_CameraPawn::RotateCamera(float AxisValue)
{
	FRotator Rot = CameraBoom->GetRelativeRotation();
	Rot.Yaw += AxisValue * KeyRotateSpeed * GetWorld()->GetDeltaSeconds();
	CameraBoom->SetRelativeRotation(Rot);
}


void AFM_CameraPawn::ZoomCamera(float Value)
{
	float NewLength = CameraBoom->TargetArmLength - (Value * ZoomSpeed);
	CameraBoom->TargetArmLength = FMath::Clamp(NewLength, ZoomMin, ZoomMax);
}

void AFM_CameraPawn::OrbitCamera(FVector2D ScreenDelta)
{
	// CameraBoom(스프링암)은 폰 위치(루트)에 붙어있어, 암의 회전을 바꾸면
	// 카메라가 폰 위치(=중심점)를 축으로 궤도 회전한다.
	FRotator Rot = CameraBoom->GetRelativeRotation();
	Rot.Yaw   += ScreenDelta.X * OrbitSpeed;                                    // 좌우 드래그 → 궤도 회전
	Rot.Pitch  = FMath::Clamp(Rot.Pitch + ScreenDelta.Y * OrbitSpeed, PitchMin, PitchMax); // 상하 드래그 → 올려/내려보기
	CameraBoom->SetRelativeRotation(Rot);
}
