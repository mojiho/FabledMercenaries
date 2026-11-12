#include "FMChatWidget.h"

#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

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
}

void UFMChatWidget::NativeDestruct()
{
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

	// 게임 조작을 살려 둔 채로 텍스트 상자에만 키보드를 보낸다 (MMO식 — 입력 중에도 클릭은 먹는다)
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
}

void UFMChatWidget::EndTyping(bool bSubmit)
{
	if (!bTyping) return;
	bTyping = false;

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

void UFMChatWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type Method)
{
	// Enter = 보내기, 그 밖(포커스 잃음·ESC) = 버리기
	EndTyping(Method == ETextCommit::OnEnter);
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
