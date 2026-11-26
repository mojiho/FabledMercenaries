#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
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
 *
 * ── Enter를 어떻게 잡는가 ──
 * 컨트롤러의 BindKey만으로는 부족하다. HUD 버튼을 한 번 누르면 그 버튼이 키보드 포커스를 쥐고
 * Enter를 '클릭'으로 먹어버리고, 입력 모드가 UI Only인 동안이나 게임이 멈춘 동안에는
 * 플레이어 입력이 아예 돌지 않는다. 그래서 어떤 위젯보다 먼저 키를 보는
 * 슬레이트 입력 전처리기(IInputProcessor)를 직접 달아 둔다 — 포커스가 어디 있든 Enter가 산다.
 * 컨트롤러의 BindKey는 이 창이 없을 때를 위한 예비로 남겨 둔다.
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

	/**
	 * 입력 전처리기가 부른다 — 위젯들이 키를 보기 전에 호출된다.
	 * @return 여기서 처리했으면 true (그 키는 더 이상 어디로도 가지 않는다)
	 */
	bool HandleGlobalKeyDown(const FKeyEvent& Ev);

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

	/** 입력창에 키보드를 물리고, 몇 프레임 동안 지키는 감시를 켠다 */
	void FocusInputBox();

	/**
	 * 포커스 감시 — 열린 직후 몇 프레임 동안 매 프레임 한 번씩 돈다.
	 *
	 * 입력창을 펴면서 입력 모드를 바꾸면 게임 뷰포트도 같은 프레임에 포커스를 가져가려 든다.
	 * 그 줄다리기에서 몇 프레임만 우리가 이겨 주면 그 뒤로는 안정된다.
	 *
	 * 월드 타이머가 아니라 코어 티커로 돈다. 월드 타이머는 게임이 멈추면 같이 멈추고,
	 * 반복 타이머는 프레임이 떨어지면 한 프레임에 여러 번 몰려 터져서
	 * '몇 프레임 동안 지킨다'가 성립하지 않는다. UUserWidget의 NativeTick도 쓸 수 없다 —
	 * 엔진이 DisableNativeTick을 달아 놔서 이 프로젝트의 위젯은 틱이 돌지 않는다.
	 *
	 * @return 다음 프레임에도 계속 돌 것인가
	 */
	bool TickFocusWatch();

	void StopFocusRetry();

	bool IsInputBoxFocused() const;

	/**
	 * 지금 이 Enter를 우리가 가져가도 되는가.
	 *
	 * 전처리기는 에디터 키까지 전부 보기 때문에 울타리가 필요하다. 다만 '게임 화면 안에
	 * 포커스가 있는가'로 재면 PIE 모드나 창 구성에 따라 답이 흔들려서 채팅이 안 열린다.
	 * 그래서 반대로 묻는다 — 글자를 받는 위젯이 포커스를 쥐고 있나? 그렇다면 비켜준다.
	 * 에디터의 이름 바꾸기 칸·검색창은 이걸로 걸러지고, 나머지는 전부 채팅 몫이다.
	 */
	bool ShouldTakeKey() const;

	int32 ActiveTab = 0;
	bool  bTyping   = false;

	/** 슬레이트에 달아 둔 전처리기. 이 창이 살아 있는 동안만 등록된다 */
	TSharedPtr<class FFMChatKeyProcessor> KeyProcessor;

	/** 포커스를 지켜 줄 프레임 수. 60fps에서 0.2초 — 줄다리기는 그보다 훨씬 빨리 끝난다 */
	static constexpr int32 FocusWatchFrames = 12;

	/** 0보다 크면 포커스 감시가 돌고 있다. 매 프레임 하나씩 줄어든다 */
	int32 FocusRetriesLeft = 0;
	FTSTicker::FDelegateHandle FocusWatchHandle;
	/** 감시 안에서 StopFocusRetry가 불릴 수 있다 — 그때 RemoveTicker를 부르면 자기를 기다리다 멈춘다 */
	bool  bInFocusWatch    = false;
};
