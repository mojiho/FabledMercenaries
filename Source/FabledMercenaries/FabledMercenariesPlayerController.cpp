// Copyright Epic Games, Inc. All Rights Reserved.

#include "DrawDebugHelpers.h" 
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
#include "Engine/Engine.h"
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
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AFabledMercenariesPlayerController::OnLeftReleased);
	UE_LOG(LogTemp, Warning, TEXT("[FM] SetupInputComponent: LMB bound"));

	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AFabledMercenariesPlayerController::OnRightClickPressed);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AFabledMercenariesPlayerController::OnRightClickReleased);

	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AFabledMercenariesPlayerController::OnRightClickCommand);


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
			// Set up mouse input events
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &AFabledMercenariesPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &AFabledMercenariesPlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &AFabledMercenariesPlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &AFabledMercenariesPlayerController::OnSetDestinationReleased);

			// Set up touch input events
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
	if (!bHit) return;

	AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
	if (!Mgr) return;

	// ── 이동 모드: 누름 = 도착지 지정 + 방향 조준 시작 (확정은 뗄 때) ──
	if (bMoveMode)
	{
		AimPoint = Hit.Location;
		bAiming = true;
		return;
	}

	// ── 평소: 유닛 선택 ──
	Mgr->HandleClick(Hit.Location);
}

void AFabledMercenariesPlayerController::OnLeftReleased()
{
	if (!bMoveMode || !bAiming) return;
	bAiming = false;

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;
	AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
	if (!Mgr) return;
	
	// 뗀 지점에 적이 있으면 → 이동 대신 공격
	uint64 Enemy = Mgr->FindEnemyNear(Hit.Location, 60.f);
	if (Enemy != 0)
	{
		Mgr->AttackTarget(Enemy);
		PendingWaypoints.Empty();
		bMoveMode = false;
		UE_LOG(LogTemp, Warning, TEXT("[FM] 공격 명령 -> %llu"), Enemy);
		return;
	}
	
	// 드래그 방향 = 도착지 → 뗀 지점 (조금이라도 끌었을 때만)
	FVector Dir = Hit.Location - AimPoint;  Dir.Z = 0.f;
	const bool bHasFacing = Dir.SizeSquared() > (30.f * 30.f);
	const FVector Facing = bHasFacing ? Dir.GetSafeNormal() : FVector::ZeroVector;

	PendingWaypoints.Add(AimPoint);

	// Ctrl이면 예약만 (마지막 아님 → 방향 무시하고 계속)
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bCtrl)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FM] 경유지 예약 %d개"), PendingWaypoints.Num());
		return;
	}

	// 최종 실행: 마지막 도착지에 방향 적용
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		Mgr->MoveSelectedAlong(PendingWaypoints, Facing, bHasFacing);
	}
	PendingWaypoints.Empty();
	bMoveMode = false;
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

void AFabledMercenariesPlayerController::OnRightClickPressed()
{
	float X, Y;
	if (GetMousePosition(X, Y))
	{
		RightPressPos = FVector2D(X, Y);
	}
}

void AFabledMercenariesPlayerController::OnRightClickReleased()
{
	float X, Y;
	if (!GetMousePosition(X, Y)) return;

	if (FVector2D::Distance(RightPressPos, FVector2D(X, Y)) > RightClickDragThreshold) return;

	OnRightClickCommand();
}

void AFabledMercenariesPlayerController::OnRightClickCommand()
{
	bMoveMode = false;          // ← 이동 모드 취소
	PendingWaypoints.Empty();   // ← 예약 점들 비우기
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		Mgr->ClearSelection();
	}
}

void AFabledMercenariesPlayerController::EnterMoveMode()
{
	bMoveMode = true;
	PendingWaypoints.Empty();
	
	UE_LOG(LogTemp, Warning, TEXT("[FM] 이동 모드 ON"));
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		Mgr->SetRingHidden(true);      // ← 버튼 누른 순간 링 사라짐 (선택은 유지)
	}

}

void AFabledMercenariesPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!bMoveMode) return;

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;
	const FVector Cursor = Hit.Location + FVector(0, 0, 15);

	AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
	if (!Mgr) return;

	FVector Prev;
	if (!Mgr->GetSelectedUnitWorldPos(Prev)) return;
	Prev += FVector(0, 0, 15);

	// 유닛 → 각 예약점 (전부 지형 따라감)
	for (const FVector& WP : PendingWaypoints)
	{
		const FVector P = WP + FVector(0, 0, 15);
		DrawGroundLine(Prev, P, FColor::Green);
		DrawDebugSphere(GetWorld(), P, 18.f, 8, FColor::Green, false, -1.f, 0, 2.f);
		Prev = P;
	}

	if (bAiming)
	{
		// 확정 대기 도착지 + 방향(도착지 → 커서) 화살표
		const FVector Aim = AimPoint + FVector(0, 0, 15);
		DrawGroundLine(Prev, Aim, FColor::Green);
		DrawDebugSphere(GetWorld(), Aim, 40.f, 12, FColor(200, 200, 255), false, -1.f, 0, 1.5f);

		FVector D = Cursor - Aim;  D.Z = 0.f;
		if (D.SizeSquared() > 1.f)
		{
			const FVector Tip = Aim + D.GetSafeNormal() * 100.f;
			DrawDebugDirectionalArrow(GetWorld(), Aim, Tip, 150.f, FColor::Yellow, false, -1.f, 0, 4.f);
		}
	}
	else
	{
		// 아직 안 눌렀으면 커서까지 미리보기 + 고스트
		DrawGroundLine(Prev, Cursor, FColor(200, 200, 255));
		DrawDebugSphere(GetWorld(), Cursor, 40.f, 12, FColor(200, 200, 255), false, -1.f, 0, 1.5f);
	}
}

void AFabledMercenariesPlayerController::DrawGroundLine(const FVector& A, const FVector& B, FColor Color)
	{
		const int32 Steps = 20;               // 조각 수 (많을수록 매끈)
		FVector Prev;
		bool bHavePrev = false;

		for (int32 i = 0; i <= Steps; ++i)
		{
			const float T = (float)i / Steps;
			const FVector P = FMath::Lerp(A, B, T);      // XY 경로상의 한 점

			// 그 지점에서 아래로 쏴서 바닥 높이 찾기
			FHitResult GHit;
			const FVector From(P.X, P.Y, P.Z + 500.f);
			const FVector To(P.X, P.Y, P.Z - 1000.f);
			FVector Ground = P;
			if (GetWorld()->LineTraceSingleByChannel(GHit, From, To, ECC_Visibility))
				Ground = GHit.Location;
			Ground.Z += 15.f;                            // 바닥에서 살짝 띄움

			if (bHavePrev)
				DrawDebugLine(GetWorld(), Prev, Ground, Color, false, -1.f, 0, 3.f);
			Prev = Ground;
			bHavePrev = true;
		}
	}

