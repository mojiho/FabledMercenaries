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
	
	// Move towards mouse pointer or touch
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn != nullptr)
	{
		FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	}
}

void AFabledMercenariesPlayerController::OnSetDestinationReleased()
{
	// If it was a short press
	if (FollowTime <= ShortPressThreshold)
	{
		// We move there and spawn some particles
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, CachedDestination);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}

	FollowTime = 0.f;
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

	// 현재 조종 중인 Pawn이 FabledMercenariesCharacter인지 확인 후 줌 호출
	if (AFabledMercenariesCharacter* Char = Cast<AFabledMercenariesCharacter>(GetPawn()))
	{
		Char->ZoomCamera(ZoomValue);
	}
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

	if (AFabledMercenariesCharacter* Char = Cast<AFabledMercenariesCharacter>(GetPawn()))
	{
		// 마우스 오른쪽으로(Delta.X+) → 카메라 Yaw 회전
		// 마우스 위로(Delta.Y+) → 카메라 Pitch 회전
		Char->PanCamera(FVector2D(Delta.X * DragPanSpeed, Delta.Y * DragPanSpeed));
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
