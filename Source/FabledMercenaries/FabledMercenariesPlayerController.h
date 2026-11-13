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

	/** WASD 카메라 이동 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraMoveAction;

	/** QE 카메라 회전 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraRotateAction;

	/** 우클릭 누름 상태 (드래그 시작/종료) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> DragHoldAction;

	/** 마우스 X/Y 이동량 (드래그 중일 때만 적용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> DragMoveAction;

	/** 드래그 회전 감도 (마우스 1픽셀당 회전 도수) */
	UPROPERTY(EditAnywhere, Category="Camera|Pan")
	float DragPanSpeed = 5.f;

	/** 현재 우클릭 드래그 중인지 */
	bool bIsDragging = false;

	/**
	 * 우클릭을 누른 뒤 쌓인 마우스 이동량 (픽셀).
	 * 누른 자리와 뗀 자리를 비교하지 않는 이유: 카메라를 돌렸다가 제자리로 돌아오면 '안 움직인 것'이 되고,
	 * 반대로 손떨림 몇 픽셀에 명령이 통째로 씹힌다. 실제로 궤도 회전을 굴린 양을 그대로 쌓는 게 정확하다.
	 */
	float RightDragDistance = 0.f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float RightClickDragThreshold = 14.f; // 이만큼 넘게 끌면 '카메라 회전'으로 보고 명령을 내지 않는다 (픽셀)

	/** 점프 입력을 기억해 두는 시간 (초) — 공중일 때 눌러도 착지하면 바로 뛰게 */
	UPROPERTY(EditAnywhere, Category = "Input")
	float JumpBufferTime = 0.25f;

	/** 남은 점프 예약 시간. 0보다 크면 매 틱 다시 시도한다 */
	float JumpBufferLeft = 0.f;

	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	/** Set to true if we're using touch input */
	uint32 bIsTouch : 1;

	/** Saved location of the character movement destination */
	FVector CachedDestination;

	/** Time that the click input has been pressed */
	float FollowTime = 0.0f;

	/** 링의 이동 버튼이 호출 (UMG에서 부를 수 있게 BlueprintCallable) */
	UFUNCTION(BlueprintCallable, Category = "Command")
	void EnterMoveMode();

	UFUNCTION(BlueprintCallable, Category = "Command")
	void CmdDefend();

	UFUNCTION(BlueprintCallable, Category = "Command")
	void CmdStop();
	
	UFUNCTION(BlueprintCallable, Category = "Command")
	void CmdFocus();

	/** 스킬 선택 → 대상 클릭 모드 진입 (WBP_SkillRow가 호출) */
	UFUNCTION(BlueprintCallable, Category = "Command")
	void ChooseSkill(int32 SkillType);

	/** 회복 포션 사용 (링의 아이템 버튼이 호출). 선택 유닛 자신에게 즉시 사용 */
	UFUNCTION(BlueprintCallable, Category = "Command")
	void CmdUseHealPotion();
	
	/** 채팅·로그 창 (WBP_Chat) — BeginPlay에서 만들어 뷰포트에 붙인다. 비우면 채팅창이 안 뜬다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UFMChatWidget> ChatWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<class UFMChatWidget> ChatWidget;

	/** 데미지 팝업 레이어 (WBP_DamagePopup) — 화면 전체를 덮는 투명 레이어. 비우면 숫자가 안 뜬다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UFMDamagePopupWidget> DamagePopupClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<class UFMDamagePopupWidget> DamagePopupWidget;

public:

	/** Constructor */
	AFabledMercenariesPlayerController();

	/** 채팅 입력창에 타이핑 중인가 — 키보드 조작을 흘릴지 판단 */
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsChatTyping() const;

protected:

	virtual void BeginPlay() override;

	/** Enter — 채팅 입력창 열기/보내기 */
	void OnChatToggle();

	/** Esc — 입력 중이면 버리고 닫기 */
	void OnChatCancel();

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	
	/** Input handlers */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnClickCommand();   // 직접 좌클릭 → Sim 명령 (Enhanced Input 우회)
	void OnTouchTriggered();
	void OnTouchReleased();

	/** 줌 처리 */
	void OnZoom(const FInputActionValue& Value);

	/** 키보드 카메라 무브*/
	void OnCameraMove(const FInputActionValue& Value);
	void OnCameraRotate(const FInputActionValue& Value);

	/** 우클릭 드래그 시작/종료/이동 */
	void OnDragStart();
	void OnDragEnd();
	void OnDragMove(const FInputActionValue& Value);

	/** Helper function to get the move destination */
	void UpdateCachedDestination();

	void OnRightClickCommand();
	void OnRightClickPressed();
	void OnRightClickReleased();

	virtual void PlayerTick(float DeltaTime) override;

	/** 두 점 사이를 지형 높이를 따라 조각내어 그림 */
	void DrawGroundLine(const FVector& A, const FVector& B, FColor Color);

	void OnLeftReleased();                      // 좌클릭 뗌 (도착 방향 확정)

	bool bMoveMode = false;

	TArray<FVector> PendingWaypoints;

	bool bAiming = false;                       // 도착지 방향 조준 중
	FVector AimPoint = FVector::ZeroVector;     // 확정 대기 중인 도착지

	bool bSkillMode = false;                    // 스킬 대상 클릭 대기 중
	int32 PendingSkillType = 0;                 // 시전 대기 중인 스킬(Sim SkillType 값)
	int32 PendingTargetMode = 0;                // 0=즉시 1=유닛 2=지점
	int32 PendingTargetFilter = 0;              // 0=Any 1=Ally 2=Enemy
	float PendingSkillRadius = 0.f;             // 지점 지정 스킬의 피해 반경 (0=범위 없음). 조준 원 크기
	
	// ── 월드맵 드래그 이동 ──
	bool    bWorldDragMove     = false;                 // 월드맵에서 땅을 누른 채 끌고 있는 중
	float   WorldDragTimer     = 0.f;                   // 마지막 목적지 갱신 이후 경과 시간
	FVector WorldDragLastPoint = FVector::ZeroVector;   // 마지막으로 보낸 목적지
};


