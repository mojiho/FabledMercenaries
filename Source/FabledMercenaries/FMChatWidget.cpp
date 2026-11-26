#include "FMChatWidget.h"

#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const TCHAR* TabLabel(int32 Tab)
	{
		switch (Tab)
		{
		case 1:  return TEXT("일반");
		case 2:  return TEXT("시스템");
		case 3:  return TEXT("전투");
		default: return TEXT("전체");
		}
	}
}

/**
 * 모든 키를 위젯보다 먼저 보는 전처리기.
 * 판단은 전부 창(UFMChatWidget)이 하고, 이쪽은 전달만 한다 —
 * 창이 먼저 파괴돼도 약한 참조라 안전하다.
 */
class FFMChatKeyProcessor : public IInputProcessor
{
public:
	explicit FFMChatKeyProcessor(UFMChatWidget* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float /*DeltaTime*/, FSlateApplication& /*App*/, TSharedRef<ICursor> /*Cursor*/) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& /*App*/, const FKeyEvent& Ev) override
	{
		UFMChatWidget* W = Owner.Get();
		return W && W->HandleGlobalKeyDown(Ev);
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("FMChatKey"); }

private:
	TWeakObjectPtr<UFMChatWidget> Owner;
};

UFMChatSubsystem* UFMChatWidget::Chat() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFMChatSubsystem>() : nullptr;
}

void UFMChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (InputBox)
	{
		InputBox->OnTextCommitted.AddDynamic(this, &UFMChatWidget::HandleTextCommitted);
		InputBox->SetVisibility(ESlateVisibility::Collapsed);   // Enter로 열기 전까지 접어 둔다
	}

	if (UFMChatSubsystem* C = Chat())
	{
		C->OnMessageAdded.AddDynamic(this, &UFMChatWidget::HandleMessageAdded);
		C->OnCleared.AddDynamic(this, &UFMChatWidget::HandleCleared);
		C->SetTyping(false);   // 레벨을 넘어오며 창이 다시 만들어졌을 수 있다
	}

	SetTab(ActiveTab);   // 라벨 갱신 + 지난 기록 복원 (레벨 이동 후에도 로그가 이어진다)

	// Enter를 잡아 둔다. 창이 살아 있는 동안만 — 레벨을 넘어가며 창이 다시 만들어지면 여기서 다시 단다
	if (!KeyProcessor.IsValid() && !IsDesignTime() && FSlateApplication::IsInitialized())
	{
		KeyProcessor = MakeShared<FFMChatKeyProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(KeyProcessor);
	}
}

void UFMChatWidget::NativeDestruct()
{
	StopFocusRetry();

	if (KeyProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().UnregisterInputPreProcessor(KeyProcessor);
		KeyProcessor.Reset();
	}

	if (UFMChatSubsystem* C = Chat())
	{
		C->OnMessageAdded.RemoveDynamic(this, &UFMChatWidget::HandleMessageAdded);
		C->OnCleared.RemoveDynamic(this, &UFMChatWidget::HandleCleared);
		C->SetTyping(false);   // 입력 중에 레벨이 바뀌면 조작이 막힌 채로 남는다
	}

	if (InputBox)
		InputBox->OnTextCommitted.RemoveDynamic(this, &UFMChatWidget::HandleTextCommitted);

	Super::NativeDestruct();
}

// ──────────────────────────────── 입력 ────────────────────────────────

void UFMChatWidget::ToggleTyping()
{
	if (bTyping) EndTyping(true);
	else         BeginTyping();
}

void UFMChatWidget::BeginTyping()
{
	if (bTyping || !InputBox) return;

	bTyping = true;
	if (UFMChatSubsystem* C = Chat()) C->SetTyping(true);

	InputBox->SetVisibility(ESlateVisibility::Visible);
	InputBox->SetText(FText::GetEmpty());

	// 접혀 있던 칸은 크기가 없어서 슬레이트가 '포커스를 줄 길'을 못 찾는다.
	// 먼저 한 번 재어 주지 않으면 SetKeyboardFocus가 조용히 실패한다 —
	// 이게 Enter를 여러 번 눌러야 열리던 이유다
	ForceLayoutPrepass();

	FocusInputBox();
}

void UFMChatWidget::FocusInputBox()
{
	if (!InputBox) return;

	// 게임 조작을 살려 둔 채로 텍스트 상자에만 키보드를 보낸다 (MMO식 — 입력 중에도 클릭은 먹는다).
	// 대화창 같은 게 입력 모드를 UI Only로 바꿔 놨더라도 여기서 되돌려 놓는다
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(InputBox->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}

	InputBox->SetKeyboardFocus();

	// 열린 직후 몇 프레임은 포커스를 지켜본다
	FocusRetriesLeft = FocusWatchFrames;

	if (!FocusWatchHandle.IsValid())
	{
		FocusWatchHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) { return TickFocusWatch(); }));
	}
}

bool UFMChatWidget::TickFocusWatch()
{
	if (FocusRetriesLeft <= 0 || !bTyping || !InputBox)
	{
		FocusRetriesLeft = 0;
		FocusWatchHandle.Reset();   // 안에서 RemoveTicker를 부르면 자기를 기다리다 멈춘다
		return false;
	}

	TGuardValue<bool> Reentry(bInFocusWatch, true);

	if (!IsInputBoxFocused())
		InputBox->SetKeyboardFocus();   // 아직 못 붙었거나 뷰포트에 빼앗겼다 — 되찾는다

	if (--FocusRetriesLeft > 0) return true;

	FocusWatchHandle.Reset();
	return false;
}

void UFMChatWidget::StopFocusRetry()
{
	FocusRetriesLeft = 0;

	// 감시 안에서 불렸으면 손대지 않는다 — 위에서 0으로 만들었으니 다음 프레임에 스스로 빠진다
	if (!bInFocusWatch && FocusWatchHandle.IsValid())
	{
		FTSTicker::RemoveTicker(FocusWatchHandle);
		FocusWatchHandle.Reset();
	}
}

bool UFMChatWidget::IsInputBoxFocused() const
{
	return InputBox && (InputBox->HasKeyboardFocus() || InputBox->HasFocusedDescendants());
}

bool UFMChatWidget::ShouldTakeKey() const
{
	if (!FSlateApplication::IsInitialized()) return false;

	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	const TSharedPtr<SWidget> Mine    = InputBox ? InputBox->GetCachedWidget() : nullptr;

	// 글자를 받는 위젯을 봤더라도 바로 물러서지 않는다 — 우리 입력창의 '속'도
	// SEditableText라서, 끝까지 올라가 그게 우리 것인지 확인하고 판단한다
	bool bTextField = false;

	for (TSharedPtr<SWidget> It = Focused; It.IsValid(); It = It->GetParentWidget())
	{
		if (Mine.IsValid() && It == Mine) return true;   // 우리 입력창 — 부르는 쪽이 따로 판단한다

		const FString Type = It->GetTypeAsString();
		if (Type.Contains(TEXT("EditableText")) || Type.Contains(TEXT("SearchBox")))
			bTextField = true;
	}

	return !bTextField;   // 남의 칸에 글을 쓰는 중이면 비켜준다
}

bool UFMChatWidget::HandleGlobalKeyDown(const FKeyEvent& Ev)
{
	const FKey  Key    = Ev.GetKey();
	const bool  bEnter = (Key == EKeys::Enter);
	const bool  bEsc   = (Key == EKeys::Escape);

	if ((!bEnter && !bEsc) || !InputBox || IsDesignTime()) return false;
	if (!ShouldTakeKey()) return false;

	// 꾹 눌러도 한 번만. 안 막으면 Enter 연타가 열기/보내기를 번갈아 때린다
	if (Ev.IsRepeat()) return bEnter;

	if (bTyping)
	{
		// Esc는 포커스가 어디 있든 여기서 접는다. 텍스트 상자의 Revert Text On Escape 설정에
		// 맡기면 그 체크박스 하나로 취소가 되거나 안 되거나 하므로 손에 쥐고 있는다
		if (bEsc) { EndTyping(false); return true; }

		// Enter: 상자가 포커스를 쥐고 있으면 그대로 넘긴다 — 상자가 '보내기'로 처리한다
		if (IsInputBoxFocused()) return false;

		// 포커스가 빠져나간 상태(버튼을 눌렀다든가) — 보내지 말고 다시 물려준다
		FocusInputBox();
		return true;
	}

	if (bEsc) return false;   // 평소의 Esc는 다른 기능(메뉴 등) 몫이다

	BeginTyping();
	return true;
}

void UFMChatWidget::EndTyping(bool bSubmit)
{
	if (!bTyping) return;
	bTyping = false;
	StopFocusRetry();

	FString Line;
	if (InputBox)
	{
		if (bSubmit) Line = InputBox->GetText().ToString();
		InputBox->SetText(FText::GetEmpty());
		InputBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UFMChatSubsystem* C = Chat())
	{
		C->SetTyping(false);
		if (bSubmit && !Line.IsEmpty()) C->Submit(Line);
	}

	// 포커스를 게임으로 돌려준다 — 안 돌리면 WASD가 계속 먹히지 않는다
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void UFMChatWidget::HandleTextCommitted(const FText& /*Text*/, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter)   { EndTyping(true);  return; }   // 보내기
	if (Method == ETextCommit::OnCleared) { EndTyping(false); return; }   // ESC — 버리기

	// 여기 남은 건 '포커스를 잃었다'는 통보뿐이다.
	// 여는 중(감시가 돌고 있는 동안)이면 뷰포트와의 줄다리기 때문에 날아온 것이므로
	// 믿지 않는다 — 이걸 믿고 접으면 열자마자 닫혀서 Enter가 안 먹는 것처럼 보인다.
	// 감시가 끝난 뒤의 통보는 진짜로 딴 데를 클릭한 것이니 그때는 접는다
	if (FocusRetriesLeft > 0) return;

	EndTyping(false);
}

// ──────────────────────────────── 표시 ────────────────────────────────

void UFMChatWidget::SetTab(int32 Tab)
{
	ActiveTab = FMath::Clamp(Tab, 0, 3);
	if (TabText) TabText->SetText(FText::FromString(TabLabel(ActiveTab)));
	Rebuild();
}

bool UFMChatWidget::PassesFilter(const FFMChatMessage& Msg) const
{
	switch (ActiveTab)
	{
	case 1:  return Msg.Channel == EFMChatChannel::Say;
	case 2:  return Msg.Channel == EFMChatChannel::System;
	case 3:  return Msg.Channel == EFMChatChannel::Combat;
	default: return true;   // 전체
	}
}

FLinearColor UFMChatWidget::ColorFor(EFMChatChannel Channel) const
{
	switch (Channel)
	{
	case EFMChatChannel::System: return SystemColor;
	case EFMChatChannel::Combat: return CombatColor;
	default:                     return SayColor;
	}
}

void UFMChatWidget::Rebuild()
{
	if (!MessageScroll) return;

	MessageScroll->ClearChildren();

	const UFMChatSubsystem* C = Chat();
	if (!C) return;

	for (const FFMChatMessage& Msg : C->GetHistory())
		AddLine(Msg);

	MessageScroll->ScrollToEnd();
}

void UFMChatWidget::AddLine(const FFMChatMessage& Msg)
{
	if (!MessageScroll || !PassesFilter(Msg)) return;

	UTextBlock* Line = NewObject<UTextBlock>(MessageScroll);
	if (!Line) return;

	Line->SetText(FText::FromString(Msg.ToLine()));
	Line->SetColorAndOpacity(FSlateColor(ColorFor(Msg.Channel)));
	Line->SetAutoWrapText(true);

	FSlateFontInfo Font = Line->GetFont();
	Font.Size = FMath::Max(1, FontSize);
	Line->SetFont(Font);

	MessageScroll->AddChild(Line);
}

void UFMChatWidget::HandleMessageAdded(const FFMChatMessage& Msg)
{
	if (!MessageScroll || !PassesFilter(Msg)) return;

	AddLine(Msg);

	// 기록 상한(MaxHistory)을 넘어 버려진 줄이 화면에만 남지 않게 맞춰 준다.
	// 탭 필터가 걸려 있으면 화면 줄 수가 더 적으므로 '넘칠 때만' 잘라낸다.
	if (const UFMChatSubsystem* C = Chat())
	{
		while (MessageScroll->GetChildrenCount() > C->MaxHistory)
			MessageScroll->RemoveChildAt(0);
	}

	MessageScroll->ScrollToEnd();
}

void UFMChatWidget::HandleCleared()
{
	if (MessageScroll) MessageScroll->ClearChildren();
}
