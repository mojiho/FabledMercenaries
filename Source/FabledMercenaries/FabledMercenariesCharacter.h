// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FabledMercenariesCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 *  A controllable top-down perspective character
 */
UCLASS(abstract)
class AFabledMercenariesCharacter : public ACharacter
{
	GENERATED_BODY()

private:

	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

public:

	/** Constructor */
	AFabledMercenariesCharacter();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns the camera component **/
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** Returns the Camera Boom component **/
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

	/** 마우스 휠로 줌 인/아웃 (양수 = 줌 인, 음수 = 줌 아웃) */
	void ZoomCamera(float Value);

private:

	/** 줌 이동 속도 */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float ZoomSpeed = 100.f;

	/** 최대로 당길 수 있는 거리 (줌 아웃 한계) */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float ZoomMax = 1600.f;

	/** 최소로 당길 수 있는 거리 (줌 인 한계) */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float ZoomMin = 400.f;

};

