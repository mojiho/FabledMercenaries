#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMChatSubsystem.h"
#include "FMChatWidget.generated.h"

/**
 * 채팅 + 시스템·전투 로그 창 (WBP_Chat의 부모).
 *
 * 전투 중에도 보여야 하므로 WBP_WorldHUD 안에 넣지 않는다 — HUD는 전투에 들어가면 통째로 숨는다.
 * 컨트롤러가 직접 만들어 뷰포트에 붙인다 (AFabledMercenariesPlayerController::BeginPlay).
 *
 * 평소에는 입력창이 접혀 있고 Enter로 연다. 입력 중에는 서브시스템의 IsTyping()이 켜져
 * 컨트롤러가 키보드 조작(WASD·QE)을 흘린다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 입력창을 펴고 포커스를 준다 */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void BeginTyping();

	/** 입력창을 접는다. bSubmit이면 적혀 있던 줄을 보낸다 */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void EndTyping(bool bSubmit);

	UFUNCTION(BlueprintPure, Category = "Chat")
	bool IsTyping() const { return bTyping; }

	/** Enter 한 방으로 열고 닫기 — 컨트롤러가 이걸 부른다 */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void ToggleTyping();

	/** 탭 전환. 0=전체 1=일반 2=시스템 3=전투 */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetTab(int32 Tab);

	UFUNCTION(BlueprintPure, Category = "Chat")
	int32 GetTab() const { return ActiveTab; }

protected:
	/** 메시지 줄이 쌓이는 곳. 스크롤박스 자체가 패널이라 텍스트를 바로 넣는다 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UScrollBox> MessageScroll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UEditableTextBox> InputBox;

	/** 지금 보고 있는 탭 이름 (없어도 동작한다) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> TabText;

	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style") FLinearColor SayColor    = FLinearColor(0.92f, 0.92f, 0.92f);
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style") FLinearColor SystemColor = FLinearColor(1.00f, 0.84f, 0.35f);
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style") FLinearColor CombatColor = FLinearColor(1.00f, 0.52f, 0.45f);
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style") int32 FontSize = 13;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleMessageAdded(const FFMChatMessage& Msg);

	UFUNCTION()
	void HandleCleared();

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type Method);

private:
	class UFMChatSubsystem* Chat() const;

	/** 탭을 바꿨을 때 기록 전체를 다시 그린다 */
	void Rebuild();
	void AddLine(const FFMChatMessage& Msg);
	bool PassesFilter(const FFMChatMessage& Msg) const;
	FLinearColor ColorFor(EFMChatChannel Channel) const;

	int32 ActiveTab = 0;
	bool  bTyping   = false;
};
