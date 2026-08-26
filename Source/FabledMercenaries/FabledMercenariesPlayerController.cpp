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
#include "Sim/Item.h"
#include "Sim/Skill.h"
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

	//InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AFabledMercenariesPlayerController::OnRightClickCommand);


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
	// 좌클릭 실제 로직은 OnClickCommand/OnLeftReleased(BindKey)가 담당.
	// 여기(템플릿 Enhanced Input)는 커서 클릭 이펙트만.
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

	// ── 스킬 모드: 조준 방식에 따라 분기 ──
	if (bSkillMode)
	{
		if (PendingTargetMode == (int32)TargetMode::Point)
		{
			// 사용 위치 지정 — 클릭한 지면 좌표로 시전
			Mgr->CastSkillAtPoint(PendingSkillType, Hit.Location);
		}
		else
		{
			// 유닛 지정 — 진영 필터에 맞는 유닛만
			uint64 Target = Mgr->FindUnitNearFiltered(Hit.Location, 60.f, PendingTargetFilter);
			UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] SkillClick target=%llu skill=%d filter=%d"),
				Target, PendingSkillType, PendingTargetFilter);
			if (Target != 0)
				Mgr->CastSkill(PendingSkillType, Target);
			else
				Mgr->SetTargeting(false);   // 유효 대상 없음 = 취소, 링 복구
		}
		bSkillMode = false;
		return;
	}

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

void AFabledMercenariesPlayerController::ChooseSkill(int32 SkillType)
{
	AFMSimManager* Mgr = Cast<AFMSimManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
	if (!Mgr) return;
	
	const FSkillInfo Info = Mgr->FindSkillInfo(SkillType);
	UE_LOG(LogTemp, Warning, TEXT("[FM][DBG] ChooseSkill req=%d found=%d mode=%d filter=%d"),
		SkillType, Info.SkillType, Info.TargetMode, Info.TargetFilter);
	if (Info.SkillType == 0) return;		// 스킬 타입 에러
	if (!Info.bCanCast) return;				// MP 부족 / 쿨다운 중 (버튼도 비활성이지만 이중 방어)
	
	// -- 즉시 발동 : 조준 단계 없이 바로 시전 --
	if (Info.TargetMode == (int32)TargetMode::Instant)
	{
		Mgr->CancelMenu();          // 목록 창 닫기
		Mgr->CastSkill(SkillType, 0);
		return;
	}
	
	// 유닛 지점 지정 : 클릭 대기 모드.
	bSkillMode = true;
	PendingSkillType = SkillType;
	PendingTargetMode = Info.TargetMode;
	PendingTargetFilter = Info.TargetFilter;
	Mgr->SetTargeting(true);	// 링 숨김 (선택은 유지 → 어느 유닛이 시전할지 앎)
	Mgr->CancelMenu();			// 목록 창 닫기 (대상 클릭을 가리지 않게)
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

	// 최종 실행: 마지막 도착지에 방향 적용 (위에서 구한 Mgr 재사용)
	Mgr->MoveSelectedAlong(PendingWaypoints, Facing, bHasFacing);
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
	AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));

	// 목록이 떠 있으면 → 목록만 닫고 선택 유지 (= 링으로 복귀)
	if (Mgr && Mgr->IsMenuOpen())
	{
		Mgr->CancelMenu();
		bSkillMode = false;
		return;
	}

	// 평소 취소: 모드 해제 + 선택 해제
	bMoveMode  = false;
	bSkillMode = false;
	PendingWaypoints.Empty();
	if (Mgr) Mgr->ClearSelection();
}

void AFabledMercenariesPlayerController::EnterMoveMode()
{
	bMoveMode = true;
	PendingWaypoints.Empty();
	
	UE_LOG(LogTemp, Warning, TEXT("[FM] 이동 모드 ON"));
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		Mgr->SetTargeting(true);      // ← 버튼 누른 순간 링 사라짐 (선택은 유지)
	}

}

void AFabledMercenariesPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    // 링이 떠있는 동안(유닛 선택 중)엔 카메라를 그 유닛에 고정(따라감)
    if (AFMSimManager* SelMgr = Cast<AFMSimManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
    {
        FVector SelPos;
        if (SelMgr->HasSelectedUnit() && SelMgr->GetSelectedUnitWorldPos(SelPos))
        {
            if (AFM_CameraPawn* Cam = Cast<AFM_CameraPawn>(GetPawn()))
            {
                const FVector Cur = Cam->GetActorLocation();
                const FVector Tgt(SelPos.X, SelPos.Y, Cur.Z);   // XY만 따라감(높이·각도는 유지)
                Cam->SetActorLocation(FMath::VInterpTo(Cur, Tgt, DeltaTime, 10.f));
            }
        }
    }

    // ── 스킬 대상 지정 중: 커서 바닥에 조준 표시 ──
    if (bSkillMode)
    {
        FHitResult SkHit;
        AFMSimManager* SkMgr = Cast<AFMSimManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));

        if (SkMgr && GetHitResultUnderCursor(ECC_Visibility, false, SkHit))
        {
            const FVector Ground = SkHit.Location + FVector(0, 0, 5.f);
            const bool bPointMode = (PendingTargetMode == (int32)TargetMode::Point);

            // 커서 아래에 유효 대상이 있나 (OnClickCommand와 같은 반경/필터)
            const uint64 Target = bPointMode ? 0
                : SkMgr->FindUnitNearFiltered(SkHit.Location, 60.f, PendingTargetFilter);
            const bool bValid = bPointMode || (Target != 0);

            const FColor Col = bValid ? FColor(80, 255, 120) : FColor(255, 190, 60);

            // 조준 커서 (이중 원)
            DrawDebugCircle(GetWorld(), Ground, 70.f, 32, Col, false, -1.f, 0, 3.f,
                FVector(1, 0, 0), FVector(0, 1, 0), false);
            DrawDebugCircle(GetWorld(), Ground, 25.f, 24, Col, false, -1.f, 0, 2.f,
                FVector(1, 0, 0), FVector(0, 1, 0), false);

            // 시전자 → 커서 연결선
            FVector CasterPos;
            if (SkMgr->GetSelectedUnitWorldPos(CasterPos))
                DrawGroundLine(CasterPos + FVector(0, 0, 15.f), Ground + FVector(0, 0, 10.f), Col);

            // 유효 대상이면 그 유닛 강조
            FVector TargetPos;
            if (Target != 0 && SkMgr->GetUnitWorldPos(Target, TargetPos))
                DrawDebugSphere(GetWorld(), TargetPos, 55.f, 16, Col, false, -1.f, 0, 3.f);

            // 상태 문구 (DeltaTime 만큼만 유지 = 매 프레임 갱신)
            DrawDebugString(GetWorld(), Ground + FVector(0, 0, 70.f),
                bValid ? TEXT("스킬 시전 - 대상 선택") : TEXT("스킬 시전 - 대상 없음"),
                nullptr, Col, DeltaTime, true);
        }
    }

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

    // 유닛 → 각 예약점 (기존 색: 초록)
    for (const FVector& WP : PendingWaypoints)
    {
        const FVector P = WP + FVector(0, 0, 15);
        DrawGroundLine(Prev, P, FColor::Green);
        DrawDebugSphere(GetWorld(), P, 18.f, 8, FColor::Green, false, -1.f, 0, 2.f);
        Prev = P;
    }

    // 커서가 적 위인가? (적이면 선 끝을 적 위치에 고정)
    FVector EnemyPos;
    const uint64 Enemy = Mgr->FindEnemyNear(Hit.Location, 80.f);
    const bool bEnemy = (Enemy != 0) && Mgr->GetUnitWorldPos(Enemy, EnemyPos);
    if (bEnemy) EnemyPos += FVector(0, 0, 15);

    if (bAiming)
    {
        const FVector Aim = AimPoint + FVector(0, 0, 15);
        DrawGroundLine(Prev, Aim, FColor::Green);
        DrawDebugSphere(GetWorld(), Aim, 40.f, 12, FColor(200, 200, 255), false, -1.f, 0, 1.5f);

        if (bEnemy)   // 적 조준 = 빨강 공격선
        {
            DrawGroundLine(Aim, EnemyPos, FColor::Red);
            DrawDebugSphere(GetWorld(), EnemyPos, 45.f, 12, FColor::Red, false, -1.f, 0, 2.f);
        }
        else          // 도착 방향 화살표
        {
            FVector D = Cursor - Aim;  D.Z = 0.f;
            if (D.SizeSquared() > 1.f)
            {
                const FVector Tip = Aim + D.GetSafeNormal() * 100.f;
                DrawDebugDirectionalArrow(GetWorld(), Aim, Tip, 150.f, FColor::Yellow, false, -1.f, 0, 4.f);
            }
        }
    }
    else if (bEnemy)
    {
        // 커서가 적 → 선이 적에 고정 + 빨강(공격 예고)
        DrawGroundLine(Prev, EnemyPos, FColor::Red);
        DrawDebugSphere(GetWorld(), EnemyPos, 45.f, 12, FColor::Red, false, -1.f, 0, 2.f);
    }
    else
    {
        // 빈 땅 미리보기 (연한 파랑) + 고스트
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


void AFabledMercenariesPlayerController::CmdDefend()
{
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
	UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
		Mgr->IssueDefendSelected();
}

void AFabledMercenariesPlayerController::CmdStop()
{
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
		Mgr->IssueStopSelected();
}

void AFabledMercenariesPlayerController::CmdFocus()
{
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
		Mgr->IssueFocusSelected();
}

void AFabledMercenariesPlayerController::CmdUseHealPotion()
{
	// 대상 선택 없이 자신에게 즉시 사용 (P0: 아이템 1종)
	if (AFMSimManager* Mgr = Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
		Mgr->ActivateItem((int32)ItemType::HealPotion);
}