// Copyright Epic Games, Inc. All Rights Reserved.

#include "FabledMercenariesCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"

AFabledMercenariesCharacter::AFabledMercenariesCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create the camera boom component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	// Create the camera component
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));

	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AFabledMercenariesCharacter::BeginPlay()
{
	Super::BeginPlay();

	// stub
}

void AFabledMercenariesCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	// stub
}

void AFabledMercenariesCharacter::ZoomCamera(float Value)
{
	// 현재 암 길이에서 줌 값만큼 빼거나 더함
	// Value > 0 : 휠 위 → 줌 인 (카메라가 가까워짐)
	// Value < 0 : 휠 아래 → 줌 아웃 (카메라가 멀어짐)
	float NewLength = CameraBoom->TargetArmLength - (Value * ZoomSpeed);
	CameraBoom->TargetArmLength = FMath::Clamp(NewLength, ZoomMin, ZoomMax);
}

void AFabledMercenariesCharacter::PanCamera(FVector2D Delta)
{
	// SpringArm의 회전만 변경 → 카메라가 캐릭터 중심으로 공전
	// Delta.X : 좌우 (Yaw)
	// Delta.Y : 위아래 (Pitch)
	FRotator CurrentRot = CameraBoom->GetRelativeRotation();
	CurrentRot.Yaw   += Delta.X;
	CurrentRot.Pitch += Delta.Y;

	// 너무 수직/수평까지 가지 않도록 Pitch 제한
	CurrentRot.Pitch = FMath::Clamp(CurrentRot.Pitch, -80.f, -10.f);

	CameraBoom->SetRelativeRotation(CurrentRot);
}
