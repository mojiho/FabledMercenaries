#include "FMSpeechBubbleWidget.h"

#include "FMSimManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
	/**
	 * 글자 폭을 재는 서비스. GetFontMeasureService()는 TSharedRef라 nullptr과 섞을 수 없어서
	 * (삼항 연산자가 컴파일되지 않는다) 여기서 한 번만 TSharedPtr로 받아 둔다.
	 */
	TSharedPtr<FSlateFontMeasure> GetFontMeasure()
	{
		if (!FSlateApplication::IsInitialized()) return nullptr;
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	}
}

AFMSimManager* UFMSpeechBubbleWidget::FindSimManager() const
{
	return Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
}

UFMChatSubsystem* UFMSpeechBubbleWidget::Chat() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFMChatSubsystem>() : nullptr;
}

void UFMSpeechBubbleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면을 덮는 레이어다 — 마우스를 먹으면 클릭 명령이 막힌다
	SetVisibility(ESlateVisibility::HitTestInvisible);

	BubbleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	BubbleBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	BubbleBrush.OutlineSettings.CornerRadii  = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
	BubbleBrush.OutlineSettings.Width        = 1.f;

	if (UFMChatSubsystem* C = Chat())
		C->OnMessageAdded.AddDynamic(this, &UFMSpeechBubbleWidget::HandleMessageAdded);
}

void UFMSpeechBubbleWidget::NativeDestruct()
{
	// 채팅 버스는 GameInstance 소속이라 레벨을 넘어가도 살아 있다 — 안 끊으면 죽은 위젯이 남는다
	if (UFMChatSubsystem* C = Chat())
		C->OnMessageAdded.RemoveDynamic(this, &UFMSpeechBubbleWidget::HandleMessageAdded);

	Bubbles.Reset();
	Super::NativeDestruct();
}

void UFMSpeechBubbleWidget::HandleMessageAdded(const FFMChatMessage& Msg)
{
	// 사람이 친 말만 풍선이 된다. 시스템·전투 로그는 채팅창에만 남는다
	if (Msg.Channel != EFMChatChannel::Say) return;

	AddBubble(Msg.Sender, Msg.Text);
}

FSlateFontInfo UFMSpeechBubbleWidget::MakeFont() const
{
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	FSlateFontInfo Info(Font, FMath::Max(1, FontSize));
	Info.OutlineSettings.OutlineSize = 1;
	return Info;
}

TArray<FString> UFMSpeechBubbleWidget::WrapText(const FString& Text, const FSlateFontInfo& FontInfo, float& OutWidthPx) const
{
	TArray<FString> Out;
	OutWidthPx = 0.f;
	if (Text.IsEmpty()) return Out;

	const TSharedPtr<FSlateFontMeasure> Measure = GetFontMeasure();
	if (!Measure.IsValid())
	{
		Out.Add(Text);   // 잴 수 없으면 한 줄로 — 그려지기는 해야 한다
		return Out;
	}

	const float Limit = FMath::Max(40.f, MaxWidthPixels);
	const int32 LineCap = FMath::Max(1, MaxLines);

	FString Line;
	int32   LastSpace = INDEX_NONE;   // 지금 줄에서 마지막으로 본 공백의 '다음' 위치
	bool    bTruncated = false;

	auto PushLine = [&Out, &OutWidthPx, &Measure, &FontInfo](const FString& S)
	{
		Out.Add(S);
		OutWidthPx = FMath::Max(OutWidthPx, (float)Measure->Measure(S, FontInfo).X);
	};

	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR Ch = Text[i];

		if (Ch == TEXT('\n'))
		{
			PushLine(Line);
			Line.Empty();
			LastSpace = INDEX_NONE;
			if (Out.Num() >= LineCap) { bTruncated = (i + 1 < Text.Len()); break; }
			continue;
		}

		Line.AppendChar(Ch);
		if (Ch == TEXT(' ')) LastSpace = Line.Len();

		if ((float)Measure->Measure(Line, FontInfo).X <= Limit) continue;

		// 폭을 넘었다 — 공백이 있으면 단어 단위로, 없으면(한글처럼) 글자 단위로 끊는다
		FString Head;
		FString Rest;
		if (LastSpace > 0 && LastSpace < Line.Len())
		{
			Head = Line.Left(LastSpace).TrimEnd();
			Rest = Line.Mid(LastSpace);
		}
		else if (Line.Len() > 1)
		{
			Head = Line.LeftChop(1);
			Rest = Line.Right(1);
		}
		else
		{
			Head = Line;   // 글자 하나가 폭보다 넓다 — 그냥 내보낸다
			Rest.Empty();
		}

		PushLine(Head);
		Line = Rest;
		LastSpace = INDEX_NONE;

		if (Out.Num() >= LineCap) { bTruncated = true; break; }
	}

	// 남은 꼬리를 넣을 자리가 있으면 넣고, 없으면 잘렸다는 표시를 남긴다.
	// 표시 없이 그냥 버리면 "말이 중간에 끊겼는데 왜 그런지 모르는" 상태가 된다.
	const bool bLeftover = !Line.IsEmpty();
	if (bLeftover && !bTruncated && Out.Num() < LineCap)
	{
		PushLine(Line);
	}
	else if ((bTruncated || bLeftover) && Out.Num() > 0)
	{
		Out[Out.Num() - 1] += TEXT("…");
		OutWidthPx = FMath::Max(OutWidthPx, (float)Measure->Measure(Out[Out.Num() - 1], FontInfo).X);
	}

	return Out;
}

void UFMSpeechBubbleWidget::AddBubble(const FString& Sender, const FString& Text)
{
	const FString Body = Text.TrimStartAndEnd();
	if (Body.IsEmpty()) return;

	const UWorld* W = GetWorld();
	if (!W) return;

	FFMSpeechBubble B;
	B.Sender   = Sender;
	B.Lines    = WrapText(Body, MakeFont(), B.WidthPx);
	B.BornTime = W->GetTimeSeconds();
	B.Life     = FMath::Clamp(MinLife + Body.Len() * LifePerChar,
	                          FMath::Max(0.1f, MinLife),
	                          FMath::Max(MinLife, MaxLife));

	if (B.Lines.Num() == 0) return;

	// 같은 사람이 연달아 말하면 겹쳐 쌓이지 않고 갈아끼운다
	ClearBubble(Sender);

	if (MaxBubbles > 0 && Bubbles.Num() >= MaxBubbles)
		Bubbles.RemoveAt(0, Bubbles.Num() - MaxBubbles + 1, EAllowShrinking::No);

	Bubbles.Add(MoveTemp(B));
}

void UFMSpeechBubbleWidget::ClearBubble(const FString& Sender)
{
	for (int32 i = Bubbles.Num() - 1; i >= 0; --i)
		if (Bubbles[i].Sender == Sender)
			Bubbles.RemoveAt(i, 1, EAllowShrinking::No);
}

bool UFMSpeechBubbleWidget::ResolveSpeaker(const FString& Sender, FVector& OutHeadPos) const
{
	AFMSimManager* Mgr = FindSimManager();
	if (!Mgr) return false;

	if (!bShowInCombat && Mgr->IsInCombat()) return false;

	// ── 지금은 사람이 하나뿐이라 '내 아바타'만 찾는다 ──
	// 서버가 붙으면 여기서 이름으로 액터를 찾아 돌려주면 되고, 그리는 쪽은 그대로다.
	const UFMChatSubsystem* C = Chat();
	if (C && !Sender.IsEmpty() && Sender != C->PlayerName) return false;

	FVector Feet;
	if (!Mgr->GetAvatarWorldPos(Feet)) return false;   // 아바타가 없는 맵·상태

	OutHeadPos = Feet + FVector(0.f, 0.f, HeadOffset);
	return true;
}

int32 UFMSpeechBubbleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	if (Bubbles.Num() == 0) return LayerId;

	const UWorld* W = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!W || !PC) return LayerId;

	const float Now = W->GetTimeSeconds();

	// 투영은 뷰포트 픽셀을 돌려준다 — 위젯 로컬 좌표로 내리려면 DPI 스케일로 나눈다
	const float ViewportScale = FMath::Max(KINDA_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));

	const FSlateFontInfo BaseFont = MakeFont();
	const TSharedPtr<FSlateFontMeasure> Measure = GetFontMeasure();

	const float LineH = Measure.IsValid() ? (float)Measure->GetMaxCharacterHeight(BaseFont) : (float)FontSize * 1.3f;

	const int32 BoxLayer  = LayerId;
	const int32 TextLayer = LayerId + 1;

	for (int32 i = Bubbles.Num() - 1; i >= 0; --i)
	{
		const FFMSpeechBubble& B = Bubbles[i];

		const float Age = Now - B.BornTime;
		if (Age < 0.f || Age >= B.Life)
		{
			Bubbles.RemoveAt(i, 1, EAllowShrinking::No);   // 수명 끝 — 그리면서 정리한다
			continue;
		}

		FVector Head;
		if (!ResolveSpeaker(B.Sender, Head)) continue;   // 이번 프레임만 건너뛴다

		FVector2D Screen;
		if (!UGameplayStatics::ProjectWorldToScreen(PC, Head, Screen, false))
			continue;                                     // 카메라 뒤 — 돌아오면 다시 보인다

		const float T = Age / FMath::Max(KINDA_SMALL_NUMBER, B.Life);
		float Alpha = 1.f;
		if (T > FadeStart)
			Alpha = 1.f - (T - FadeStart) / FMath::Max(KINDA_SMALL_NUMBER, 1.f - FadeStart);
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

		const float BoxW = B.WidthPx + PaddingX * 2.f;
		const float BoxH = LineH * B.Lines.Num() + PaddingY * 2.f;

		// 머리 위에 바닥을 맞추고 가로는 가운데 정렬
		const float CenterX = (float)Screen.X / ViewportScale;
		const float HeadY   = (float)Screen.Y / ViewportScale;
		const float BoxX    = CenterX - BoxW * 0.5f;
		const float BoxY    = HeadY - GapPixels - BoxH;

		// 외곽선은 브러시가 들고 있어서 투명도를 따라가지 않는다 — 그릴 때마다 손본다
		FLinearColor Outline = OutlineColor;
		Outline.A *= Alpha;
		BubbleBrush.OutlineSettings.Color = FSlateColor(Outline);
		BubbleBrush.OutlineSettings.CornerRadii =
			FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);

		FLinearColor Fill = BubbleColor;
		Fill.A *= Alpha;

		FSlateDrawElement::MakeBox(OutDrawElements, BoxLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(BoxW, BoxH),
				FSlateLayoutTransform(FVector2f(BoxX, BoxY))),
			&BubbleBrush, ESlateDrawEffect::None, Fill);

		// 꼬리 — 머리를 가리키는 작은 네모. 풍선 아래 모서리에 걸쳐 둔다
		const float NubSize = 9.f;
		FSlateDrawElement::MakeBox(OutDrawElements, BoxLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(NubSize, NubSize),
				FSlateLayoutTransform(FVector2f(CenterX - NubSize * 0.5f, BoxY + BoxH - NubSize * 0.5f))),
			&BubbleBrush, ESlateDrawEffect::None, Fill);

		FLinearColor Col = TextColor;
		Col.A *= Alpha;

		FSlateFontInfo FontInfo = BaseFont;
		FontInfo.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, Alpha);

		for (int32 L = 0; L < B.Lines.Num(); ++L)
		{
			// 줄마다 가운데 정렬 — 안 하면 짧은 줄이 왼쪽에 붙어 보인다
			float LineW = B.WidthPx;
			if (Measure.IsValid())
				LineW = (float)Measure->Measure(B.Lines[L], FontInfo).X;

			const float TextX = CenterX - LineW * 0.5f;
			const float TextY = BoxY + PaddingY + LineH * L;

			FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
				AllottedGeometry.ToPaintGeometry(FVector2f(1.f, 1.f),
					FSlateLayoutTransform(FVector2f(TextX, TextY))),
				B.Lines[L], FontInfo, ESlateDrawEffect::None, Col);
		}
	}

	return TextLayer + 1;
}
