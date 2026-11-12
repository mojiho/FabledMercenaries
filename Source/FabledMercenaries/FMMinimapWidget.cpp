#include "FMMinimapWidget.h"
#include "FMSimManager.h"
#include "FMEncounterSymbol.h"
#include "FMWorldMapNode.h"
#include "FMRestNode.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElementTypes.h"

namespace
{
	/** 중심에서 Size만큼 벌어진 마름모 — 작게 그리면 점으로 보인다 */
	TArray<FVector2D> Diamond(const FVector2D& C, float Size)
	{
		const float R = Size * 0.5f;
		return { FVector2D(C.X, C.Y - R), FVector2D(C.X + R, C.Y), FVector2D(C.X, C.Y + R),
		         FVector2D(C.X - R, C.Y), FVector2D(C.X, C.Y - R) };
	}

	/** 위를 향한 삼각형 — 아바타 표시 */
	TArray<FVector2D> Triangle(const FVector2D& C, float Size)
	{
		const float R = Size * 0.5f;
		return { FVector2D(C.X, C.Y - R), FVector2D(C.X + R * 0.8f, C.Y + R),
		         FVector2D(C.X - R * 0.8f, C.Y + R), FVector2D(C.X, C.Y - R) };
	}

	TArray<FVector2D> Circle(const FVector2D& C, float Radius, int32 Segments = 32)
	{
		TArray<FVector2D> Pts;
		Pts.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = (2.f * PI * i) / Segments;
			Pts.Add(FVector2D(C.X + FMath::Cos(A) * Radius, C.Y + FMath::Sin(A) * Radius));
		}
		return Pts;
	}
}

void UFMMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Rescan();
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(RescanTimer, FTimerDelegate::CreateUObject(this, &UFMMinimapWidget::Rescan),
			FMath::Max(0.1f, RescanInterval), true);
	}
}

void UFMMinimapWidget::NativeDestruct()
{
	if (UWorld* W = GetWorld())
		W->GetTimerManager().ClearTimer(RescanTimer);

	Super::NativeDestruct();
}

void UFMMinimapWidget::Rescan()
{
	Blips.Reset();

	UWorld* W = GetWorld();
	if (!W) return;

	for (TActorIterator<AFMEncounterSymbol> It(W); It; ++It)
		Blips.Add({ *It, EFMBlipKind::Enemy });

	for (TActorIterator<AFMWorldMapNode> It(W); It; ++It)
		Blips.Add({ *It, EFMBlipKind::Gate });

	for (TActorIterator<AFMRestNode> It(W); It; ++It)
		Blips.Add({ *It, EFMBlipKind::Rest });
}

bool UFMMinimapWidget::GetViewCenter(FVector& OutPos) const
{
	// 탐험 아바타가 기준. 월드맵·탐험맵 둘 다 아바타가 상시 존재한다
	if (const AFMSimManager* Mgr = Cast<AFMSimManager>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AFMSimManager::StaticClass())))
	{
		if (Mgr->GetAvatarWorldPos(OutPos)) return true;
	}

	if (const APawn* P = GetOwningPlayerPawn())
	{
		OutPos = P->GetActorLocation();
		return true;
	}
	return false;
}

FLinearColor UFMMinimapWidget::ColorFor(EFMBlipKind Kind) const
{
	switch (Kind)
	{
	case EFMBlipKind::Gate: return GateColor;
	case EFMBlipKind::Rest: return RestColor;
	default:                return EnemyColor;
	}
}

int32 UFMMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size   = AllottedGeometry.GetLocalSize();
	const FVector2D Mid    = Size * 0.5f;
	const float     Radius = (float)(FMath::Min(Size.X, Size.Y) * 0.5) - 2.f;
	if (Radius <= 0.f || ViewRadius <= 0.f) return LayerId;

	const FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry();
	const float Scale = Radius / ViewRadius;

	// 테두리 — 여기까지가 ViewRadius
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Circle(Mid, Radius),
		ESlateDrawEffect::None, FrameColor, true, LineThickness);

	FVector Center;
	if (!GetViewCenter(Center)) return LayerId;

	// 화면 위쪽을 카메라 앞쪽으로 — 탑다운 카메라는 고정이라 사실상 상수다
	float Yaw = YawOffset;
	if (bAlignToCamera)
	{
		if (const APlayerController* PC = GetOwningPlayer())
			if (const APlayerCameraManager* Cam = PC->PlayerCameraManager)
				Yaw += Cam->GetCameraRotation().Yaw;
	}
	const float YawRad = FMath::DegreesToRadians(-Yaw);
	const float CosY   = FMath::Cos(YawRad);
	const float SinY   = FMath::Sin(YawRad);

	for (const FBlip& B : Blips)
	{
		const AActor* A = B.Actor.Get();
		if (!A) continue;   // 전투로 사라진 심볼 — 다음 Rescan에서 정리된다

		const FVector D = A->GetActorLocation() - Center;

		// 월드 +X = 화면 위, +Y = 화면 오른쪽. 화면 Y는 아래로 증가하므로 뒤집는다
		// (UE FVector는 double, Slate는 float — 섞이지 않게 여기서 한 번만 내린다)
		const double RX = D.X * CosY - D.Y * SinY;
		const double RY = D.X * SinY + D.Y * CosY;
		FVector2D Offset(RY * Scale, -RX * Scale);

		// 반경 밖이면 테두리에 붙여 방향만 알린다
		float Alpha = 1.f;
		const float Len = (float)Offset.Size();
		if (Len > Radius)
		{
			Offset = (Len > KINDA_SMALL_NUMBER) ? Offset / Len * Radius : FVector2D::ZeroVector;
			Alpha  = ClampedOpacity;
		}

		FLinearColor C = ColorFor(B.Kind);
		C.A *= Alpha;

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Diamond(Mid + Offset, BlipSize),
			ESlateDrawEffect::None, C, true, LineThickness);
	}

	// 아바타는 언제나 한가운데
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Triangle(Mid, AvatarSize),
		ESlateDrawEffect::None, AvatarColor, true, LineThickness);

	return LayerId + 1;
}
