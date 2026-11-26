#include "Core/FM_CameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

	// 외곽선 포스트 프로세스 기본값 — 디테일 패널에서 비우면 외곽선이 꺼진다
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OutlineMat(
		TEXT("/Game/FX/M_PP_CommandReadyOutline.M_PP_CommandReadyOutline"));
	if (OutlineMat.Succeeded()) OutlineMaterial = OutlineMat.Object;
}

void AFM_CameraPawn::BeginPlay()
{
	Super::BeginPlay();

	if (CameraComponent && OutlineMaterial)
		CameraComponent->PostProcessSettings.AddBlendable(OutlineMaterial, 1.f);
}

void AFM_CameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaTime);
	if (ExpandState == EExpandState::Idle) return;

	ExpandElapsed += DeltaTime;

	switch (ExpandState)
	{
	case EExpandState::Out:
		{
			const float T = FMath::Clamp(ExpandElapsed / FMath::Max(ExpandOutTime, 0.01f), 0.f, 1.f);
			CameraBoom->TargetArmLength = FMath::InterpEaseInOut(ExpandFrom, ExpandTo, T, 2.f);
			if (T >= 1.f) { ExpandState = EExpandState::Hold; ExpandElapsed = 0.f; }
			break;
		}
	case EExpandState::Hold:
		// 머무름이 없으면 '스쳤다'로 보여서 뭘 보여 줬는지 모른다.
		// 머무는 동안에도 RaiseExpandTarget이 목표를 올릴 수 있다 — 그때 그냥 대입하면
		// 한 프레임에 튄다. 쫓아가게 둔다 (이미 같으면 아무 일도 안 일어난다)
		CameraBoom->TargetArmLength =
			FMath::FInterpTo(CameraBoom->TargetArmLength, ExpandTo, DeltaTime, 6.f);

		if (ExpandElapsed >= ExpandHoldTime)
		{
			ExpandTo      = CameraBoom->TargetArmLength;   // 실제 도달한 값에서 되돌아간다
			ExpandState   = EExpandState::Back;
			ExpandElapsed = 0.f;
		}
		break;

	case EExpandState::Back:
		{
			const float T = FMath::Clamp(ExpandElapsed / FMath::Max(ExpandBackTime, 0.01f), 0.f, 1.f);
			CameraBoom->TargetArmLength = FMath::InterpEaseInOut(ExpandTo, ExpandFrom, T, 2.f);
			if (T >= 1.f) { ExpandState = EExpandState::Idle; CooldownLeft = ExpandCooldown; }
			break;
		}
	default: break;
	}
}

void AFM_CameraPawn::MoveCamera(FVector2D Direction)
{
	AbortExpand();
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
	AbortExpand();
	FRotator Rot = CameraBoom->GetRelativeRotation();
	Rot.Yaw += AxisValue * KeyRotateSpeed * GetWorld()->GetDeltaSeconds();
	CameraBoom->SetRelativeRotation(Rot);
}


void AFM_CameraPawn::ZoomCamera(float Value)
{
	AbortExpand();

	// 연출이 ZoomMax보다 멀리 벌린 상태를 넘겨받을 수 있다(ExpandArmMax가 더 크다).
	// 그때 곧바로 ZoomMax로 자르면 휠 한 번에 화면이 튄다 — 지금 길이까지는 허용하고
	// 당기는 건 자유롭게 둔다. 한 번 ZoomMax 안으로 들어오면 다시 넘지 못한다
	const float Cur        = CameraBoom->TargetArmLength;
	const float UpperBound = FMath::Max(ZoomMax, Cur);

	CameraBoom->TargetArmLength = FMath::Clamp(Cur - (Value * ZoomSpeed), ZoomMin, UpperBound);
}

void AFM_CameraPawn::OrbitCamera(FVector2D ScreenDelta)
{
	AbortExpand();
	// CameraBoom(스프링암)은 폰 위치(루트)에 붙어있어, 암의 회전을 바꾸면
	// 카메라가 폰 위치(=중심점)를 축으로 궤도 회전한다.
	FRotator Rot = CameraBoom->GetRelativeRotation();
	Rot.Yaw   += ScreenDelta.X * OrbitSpeed;                                    // 좌우 드래그 → 궤도 회전
	Rot.Pitch  = FMath::Clamp(Rot.Pitch + ScreenDelta.Y * OrbitSpeed, PitchMin, PitchMax); // 상하 드래그 → 올려/내려보기
	CameraBoom->SetRelativeRotation(Rot);
}

float AFM_CameraPawn::ComputeRequiredArm(const TArray<FVector>& Points) const
{
	if (Points.Num() == 0 || !CameraComponent) return -1.f;

	// 화면비 — bConstrainAspectRatio가 꺼져 있어 뷰포트가 그대로 화면비다
	float Aspect = 16.f / 9.f;
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		int32 VX = 0, VY = 0;
		PC->GetViewportSize(VX, VY);
		if (VY > 0) Aspect = (float)VX / (float)VY;
	}

	// UCameraComponent::FieldOfView는 '수평' FOV다
	const float TanH = FMath::Tan(FMath::DegreesToRadians(CameraComponent->FieldOfView * 0.5f))
					 * FMath::Max(SafeFrac, 0.1f);
	const float TanV = TanH / FMath::Max(Aspect, 0.1f);

	// 피봇 = 붐이 붙어 있는 루트. 카메라는 여기서 붐 방향으로 L만큼 뒤에 있다
	const FVector  Pivot  = GetActorLocation();
	const FRotator CamRot = CameraComponent->GetComponentRotation();

	// 붐을 L만큼 뒤로 미는 것은 카메라 좌표계에서 X(앞쪽 거리)만 +L 하는 것이고
	// Y(좌우)·Z(위아래)는 L에 무관하다. 그래서 필요한 L이 닫힌 식으로 바로 나온다
	float Need = -1.f;
	for (const FVector& P : Points)
	{
		const FVector V = CamRot.UnrotateVector(P - Pivot);   // X=앞, Y=우, Z=상
		const float NeedX = FMath::Max(FMath::Abs(V.Y) / TanH, FMath::Abs(V.Z) / TanV);
		Need = FMath::Max(Need, NeedX - V.X);
	}
	return Need;
}

bool AFM_CameraPawn::TryExpandToInclude(const TArray<FVector>& Points)
{
	if (ExpandState != EExpandState::Idle || CooldownLeft > 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] Expand 거절: 쿨다운 %.2f초 남음"), CooldownLeft);
		return false;
	}

	const float Need = ComputeRequiredArm(Points);
	const float Cur  = CameraBoom->TargetArmLength;

	UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] Expand 요청: 점 %d개, 필요 붐 %.0f, 현재 %.0f, 상한 %.0f"),
		Points.Num(), Need, Cur, ExpandArmMax);

	if (Need <= Cur) return false;   // 이미 화면 안 — 패스

	// ExpandArmMax로도 다 담지 못하면 갈 수 있는 데까지만 간다. 그냥 버리면 '멀리 있는
	// 유닛은 아무 반응이 없다'가 되어 고장처럼 보인다 — 끝까지 벌리면 적어도 가까워지고,
	// 누구 턴인지는 SetCommandReady 외곽선이 알려 준다
	const float Target = FMath::Min(Need, ExpandArmMax);
	if (Target <= Cur) return false;   // 이미 ExpandArmMax에 붙어 있다 — 벌릴 여지가 없다

	ExpandFrom    = Cur;
	ExpandTo      = Target;   // Cur보다 크고 ExpandArmMax 이하 — ZoomMin 클램프는 불필요
	ExpandElapsed = 0.f;
	ExpandState   = EExpandState::Out;
	return true;
}

void AFM_CameraPawn::RaiseExpandTarget(const TArray<FVector>& Points)
{
	// 확장·머무름 동안에만. 복귀 중에 올리면 되돌아가다 말고 다시 벌어진다
	if (ExpandState != EExpandState::Out && ExpandState != EExpandState::Hold) return;

	const float Need = ComputeRequiredArm(Points);
	if (Need > ExpandTo)
		ExpandTo = FMath::Min(Need, ExpandArmMax);   // 올리기만 — 내리면 줌이 떤다
}

void AFM_CameraPawn::AbortExpand()
{
	if (ExpandState == EExpandState::Idle) return;

	// 지금 줌을 그대로 넘긴다. 원래대로 되돌리면 손을 댔는데 카메라가
	// 제멋대로 움직이는 셈이 된다
	ExpandState  = EExpandState::Idle;
	CooldownLeft = ExpandCooldown;
}