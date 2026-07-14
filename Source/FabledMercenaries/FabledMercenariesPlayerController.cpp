// Copyright Epic Games, Inc. All Rights Reserved.

#include "FabledMercenariesPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "FabledMercenariesCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "FabledMercenaries.h"
#include "FMSimManager.h"
#include "Core/FM_CameraPawn.h"
#include "Kismet/GameplayStatics.h"

AFabledMercenariesPlayerController::AFabledMercenariesPlayerController()
{
	bIsTouch = false;
	bMoveToMouseCursor = false;

	// create the path following comp
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));

	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void AFabledMercenariesPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// 직접 좌클릭 바인딩 (Enhanced Input IA와 무관하게 항상 발동)
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AFabledMercenariesPlayerController::OnClickCommand);
	UE_LOG(LogTemp, Warning, TEXT("[FM] SetupInputComponent: LMB bound"));

	// Only set up input on local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}

		// Set up action bindings
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// Setup mouse input events
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &AFabledMercenariesPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &AFabledMercenariesPlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &AFabledMercenariesPlayerController::OnSetDestinationReleased);

			// Setup touch input events
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &AFabledMercenariesPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnTouchTriggered);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &AFabledMercenariesPlayerController::OnTouchReleased);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &AFabledMercenariesPlayerController::OnTouchReleased);

			// 마우스 휠 줌
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnZoom);
			
			// 키보드 마우스 컨트롤
			EnhancedInputComponent->BindAction(CameraMoveAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnCameraMove);
			EnhancedInputComponent->BindAction(CameraRotateAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnCameraRotate);


			// 우클릭 드래그 패닝
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Started,   this, &AFabledMercenariesPlayerController::OnDragStart);
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Completed, this, &AFabledMercenariesPlayerController::OnDragEnd);
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Canceled,  this, &AFabledMercenariesPlayerController::OnDragEnd);
			EnhancedInputComponent->BindAction(DragMoveAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnDragMove);
		}
		else
		{
			UE_LOG(LogFabledMercenaries, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
		}
	}
}

void AFabledMercenariesPlayerController::OnInputStarted()
{
	StopMovement();

	// Update the move destination to wherever the cursor is pointing at
	UpdateCachedDestination();
}

void AFabledMercenariesPlayerController::OnSetDestinationTriggered()
{
	// We flag that the input is being pressed
	FollowTime += GetWorld()->GetDeltaSeconds();
	
	// Update the move destination to wherever the cursor is pointing at
	UpdateCachedDestination();
	
	// (템플릿: 캐릭터를 커서 쪽으로 이동 — RTS라 Sim이 대신 하므로 끔)
	//APawn* ControlledPawn = GetPawn();
	//if (ControlledPawn != nullptr)
	//{
	//	FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
	//	ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	//}
}

void AFabledMercenariesPlayerController::OnSetDestinationReleased()
{
	UE_LOG(LogTemp, Warning, TEXT("[FM] OnSetDestinationReleased FIRED, dest=%s"), *CachedDestination.ToString());

	// [흡수] 클릭 릴리즈 → Sim 유닛(전사 100) 이동명령 (press 길이 무관, 항상)
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] Manager FOUND -> IssueMove 100"));
		Mgr->IssueMoveCommand(100, CachedDestination);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
			FString::Printf(TEXT("MOVE unit100 -> %s"), *CachedDestination.ToString()));
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("FMSimManager NOT FOUND"));
	}

	// 짧은 클릭이면 이펙트
	if (FollowTime <= ShortPressThreshold)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}

	FollowTime = 0.f;
}

void AFabledMercenariesPlayerController::OnClickCommand()
{
	FHitResult Hit;
	bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, Hit);
	UE_LOG(LogTemp, Warning, TEXT("[FM] OnClickCommand FIRED, hit=%d loc=%s"), bHit, *Hit.Location.ToString());

	if (!bHit) return;

	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] Manager FOUND -> IssueMove 100"));
		Mgr->HandleClick(Hit.Location);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
			FString::Printf(TEXT("MOVE 100 -> %s"), *Hit.Location.ToString()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] Manager NOT FOUND"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Manager NOT FOUND"));
	}
}

// Triggered every frame when the input is held down
void AFabledMercenariesPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void AFabledMercenariesPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void AFabledMercenariesPlayerController::OnZoom(const FInputActionValue& Value)
{
	// Value.Get<float>() : 휠 위 = +1.0, 휠 아래 = -1.0
	float ZoomValue = Value.Get<float>();

	// 현재 조종 중인 Pawn이 카메라 폰이면 줌 호출
	if (AFM_CameraPawn* Cam = Cast<AFM_CameraPawn>(GetPawn()))
	{
		Cam->ZoomCamera(ZoomValue);
	}
}

void AFabledMercenariesPlayerController::OnCameraMove(const FInputActionValue& Value)
{
	FVector2D Axis = Value.Get<FVector2D>();               // WASD → (X=좌우, Y=앞뒤)
	if (AFM_CameraPawn* Cam = Cast<AFM_CameraPawn>(GetPawn()))
		Cam->MoveCamera(Axis);
}

void AFabledMercenariesPlayerController::OnCameraRotate(const FInputActionValue& Value)
{
	float Axis = Value.Get<float>();                       // QE → (-1 / +1)
	if (AFM_CameraPawn* Cam = Cast<AFM_CameraPawn>(GetPawn()))
		Cam->RotateCamera(Axis);
}

void AFabledMercenariesPlayerController::OnDragStart()
{
	bIsDragging = true;
}

void AFabledMercenariesPlayerController::OnDragEnd()
{
	bIsDragging = false;
}

void AFabledMercenariesPlayerController::OnDragMove(const FInputActionValue& Value)
{
	// 우클릭이 안 눌려있으면 무시
	if (!bIsDragging) return;

	// Value.Get<FVector2D>() : 마우스 X/Y 이동량 (이번 프레임 델타)
	FVector2D Delta = Value.Get<FVector2D>();

	// 카메라 폰을 중심점 축으로 궤도 회전 (스케일은 폰의 OrbitSpeed가 담당)
	if (AFM_CameraPawn* Cam = Cast<AFM_CameraPawn>(GetPawn()))
	{
		Cam->OrbitCamera(Delta);
	}
}

void AFabledMercenariesPlayerController::UpdateCachedDestination()
{
	// We look for the location in the world where the player has pressed the input
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	// If we hit a surface, cache the location
	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}
}
