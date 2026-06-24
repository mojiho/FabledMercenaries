// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "FabledMercenariesPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UPathFollowingComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Player controller for a top-down perspective game.
 *  Implements point and click based controls
 */
UCLASS(abstract)
class AFabledMercenariesPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Component used for moving along a NavMesh path. */
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, Category="Input")
	float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	/** 줌 인/아웃 Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> ZoomAction;

	/** 우클릭 누름 상태 (드래그 시작/종료) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> DragHoldAction;

	/** 마우스 X/Y 이동량 (드래그 중일 때만 적용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> DragMoveAction;

	/** 드래그 회전 감도 (마우스 1픽셀당 회전 도수) */
	UPROPERTY(EditAnywhere, Category="Camera|Pan")
	float DragPanSpeed = 0.3f;

	/** 현재 우클릭 드래그 중인지 */
	bool bIsDragging = false;

	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	/** Set to true if we're using touch input */
	uint32 bIsTouch : 1;

	/** Saved location of the character movement destination */
	FVector CachedDestination;

	/** Time that the click input has been pressed */
	float FollowTime = 0.0f;

public:

	/** Constructor */
	AFabledMercenariesPlayerController();

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	
	/** Input handlers */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();

	/** 줌 처리 */
	void OnZoom(const FInputActionValue& Value);

	/** 우클릭 드래그 시작/종료/이동 */
	void OnDragStart();
	void OnDragEnd();
	void OnDragMove(const FInputActionValue& Value);

	/** Helper function to get the move destination */
	void UpdateCachedDestination();
};


