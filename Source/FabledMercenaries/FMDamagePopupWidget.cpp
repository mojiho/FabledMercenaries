#include "FMDamagePopupWidget.h"

#include "FMSimManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

AFMSimManager* UFMDamagePopupWidget::FindSimManager() const
{
	return Cast<AFMSimManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass()));
}

void UFMDamagePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 숫자가 마우스를 먹으면 안 된다 — 화면을 덮는 레이어다
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (AFMSimManager* Mgr = FindSimManager())
		Mgr->OnUnitHitDisplay.AddDynamic(this, &UFMDamagePopupWidget::HandleUnitHit);
}

void UFMDamagePopupWidget::NativeDestruct()
{
	if (AFMSimManager* Mgr = FindSimManager())
		Mgr->OnUnitHitDisplay.RemoveDynamic(this, &UFMDamagePopupWidget::HandleUnitHit);

	Popups.Reset();
	Super::NativeDestruct();
}

void UFMDamagePopupWidget::HandleUnitHit(FVector WorldPos, const FString& Text, bool bCrit, bool bAlly)
{
	AddPopup(WorldPos, Text, bCrit, bAlly);
}

void UFMDamagePopupWidget::AddPopup(const FVector& WorldPos, const FString& Text, bool bCrit, bool bAlly)
{
	if (Text.IsEmpty()) return;

	const UWorld* W = GetWorld();
	if (!W) return;

	FFMDamagePopup P;
	P.WorldPos = WorldPos;
	P.Text     = Text;
	P.Color    = bCrit ? CritColor : (bAlly ? AllyHitColor : EnemyHitColor);
	P.Scale    = bCrit ? CritScale : 1.f;
	P.BornTime = W->GetTimeSeconds();

	// 한꺼번에 쏟아지면 오래된 것부터 버린다 (메테오처럼 여럿이 동시에 맞을 때)
	if (MaxPopups > 0 && Popups.Num() >= MaxPopups)
		Popups.RemoveAt(0, Popups.Num() - MaxPopups + 1, EAllowShrinking::No);

	Popups.Add(MoveTemp(P));
}

int32 UFMDamagePopupWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	if (Popups.Num() == 0) return LayerId;

	const UWorld* W = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!W || !PC) return LayerId;

	const float Now  = W->GetTimeSeconds();
	const float Life = FMath::Max(0.05f, LifeTime);

	// 투영은 뷰포트 픽셀을 돌려준다 — 위젯 로컬 좌표로 내리려면 DPI 스케일로 나눈다
	const float ViewportScale = FMath::Max(KINDA_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));

	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		const FFMDamagePopup& P = Popups[i];

		const float Age = Now - P.BornTime;
		if (Age < 0.f || Age >= Life)
		{
			Popups.RemoveAt(i, 1, EAllowShrinking::No);   // 수명 끝 — 그리면서 정리한다
			continue;
		}

		const float T = Age / Life;   // 0 → 1

		FVector2D Screen;
		if (!UGameplayStatics::ProjectWorldToScreen(PC, P.WorldPos, Screen, false))
			continue;   // 카메라 뒤 — 이번 프레임은 건너뛴다 (돌아오면 다시 보인다)

		// 떠오르기 (뒤로 갈수록 천천히 — 감속)
		const float Rise = RisePixels * FMath::InterpEaseOut(0.f, 1.f, T, 2.f);
		FVector2D Local(Screen.X / ViewportScale, (Screen.Y - Rise) / ViewportScale);

		// 터질 때 살짝 부풀었다 제자리로 (앞 15%)
		float Pop = 1.f;
		if (PopScale > 1.f && T < 0.15f)
			Pop = FMath::Lerp(PopScale, 1.f, T / 0.15f);

		float Alpha = 1.f;
		if (T > FadeStart)
			Alpha = 1.f - (T - FadeStart) / FMath::Max(KINDA_SMALL_NUMBER, 1.f - FadeStart);

		FSlateFontInfo FontInfo(Font, FMath::Max(1, FMath::RoundToInt(BaseFontSize * P.Scale * Pop)));
		FontInfo.OutlineSettings.OutlineSize = 1;
		FontInfo.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, Alpha);

		// 글자 폭만큼 왼쪽으로 당겨 가운데 정렬 — 안 하면 숫자가 유닛 오른쪽으로 치우친다
		if (const TSharedPtr<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService())
		{
			const FVector2D Size = Measure->Measure(P.Text, FontInfo);
			Local.X -= Size.X * 0.5f;
			Local.Y -= Size.Y * 0.5f;
		}

		FLinearColor Col = P.Color;
		Col.A *= Alpha;

		// UE5의 FVector2D는 double, 슬레이트 지오메트리는 float — 여기서 한 번만 내린다
		const FVector2f LocalF((float)Local.X, (float)Local.Y);

		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(1.f, 1.f), FSlateLayoutTransform(LocalF)),
			P.Text, FontInfo, ESlateDrawEffect::None, Col);
	}

	return LayerId + 1;
}
