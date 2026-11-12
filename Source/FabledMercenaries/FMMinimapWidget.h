#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMMinimapWidget.generated.h"

/** 미니맵에 찍히는 점의 종류 — 색만 다르다 */
UENUM()
enum class EFMBlipKind : uint8
{
	Enemy,   // 적 심볼 (배회 중인 인카운터)
	Gate,    // 월드맵 노드 (레벨 진입)
	Rest,    // 휴식 지점
};

/**
 * 미니맵 (WBP_Minimap의 부모) — 아바타를 한가운데 두고 주변 지형지물을 점으로 찍는다.
 *
 * 아트가 아직 없으므로 지형을 그리지 않고 선으로만 그린다(프로젝트의 DrawDebug 관례와 같은 결).
 * 나중에 SceneCapture 텍스처를 깔고 싶으면 이 위젯 뒤에 Image를 하나 두면 된다.
 *
 * - 가장자리 = ViewRadius 거리. 그보다 먼 대상은 테두리에 붙여 흐리게 찍어 방향만 알린다.
 * - 대상 '목록'은 RescanInterval마다 다시 훑고, 위치는 매 프레임 읽는다 (심볼이 배회하므로).
 * - NativeTick은 UUserWidget이 기본으로 꺼 두므로 타이머로 훑는다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 미니맵 가장자리에 해당하는 실제 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Minimap") float ViewRadius = 6000.f;

	/** 대상 목록을 다시 훑는 간격 (초) */
	UPROPERTY(EditAnywhere, Category = "Minimap") float RescanInterval = 0.5f;

	/** 화면 위쪽을 카메라 앞쪽에 맞춘다. 끄면 월드 +X가 위 */
	UPROPERTY(EditAnywhere, Category = "Minimap") bool bAlignToCamera = true;

	/** 방향이 틀어져 보이면 이걸로 돌린다 (도) */
	UPROPERTY(EditAnywhere, Category = "Minimap") float YawOffset = 0.f;

	UPROPERTY(EditAnywhere, Category = "Minimap|Style") float BlipSize   = 7.f;
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") float AvatarSize = 10.f;
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") float LineThickness = 2.f;

	UPROPERTY(EditAnywhere, Category = "Minimap|Style") FLinearColor FrameColor  = FLinearColor(1.f, 1.f, 1.f, 0.35f);
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") FLinearColor AvatarColor = FLinearColor(0.35f, 1.f, 0.5f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") FLinearColor EnemyColor  = FLinearColor(1.f, 0.3f, 0.25f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") FLinearColor GateColor   = FLinearColor(0.45f, 0.7f, 1.f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") FLinearColor RestColor   = FLinearColor(1.f, 0.85f, 0.4f, 1.f);

	/** 가장자리 밖으로 밀려난 점의 투명도 */
	UPROPERTY(EditAnywhere, Category = "Minimap|Style") float ClampedOpacity = 0.45f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FBlip
	{
		TWeakObjectPtr<AActor> Actor;
		EFMBlipKind            Kind = EFMBlipKind::Enemy;
	};

	/** 레벨에서 찍을 대상을 다시 모은다 — 전투로 사라진 심볼을 걷어내는 역할도 한다 */
	void Rescan();

	/** 미니맵 한가운데가 될 위치 — 탐험 아바타, 없으면 플레이어 폰 */
	bool GetViewCenter(FVector& OutPos) const;

	FLinearColor ColorFor(EFMBlipKind Kind) const;

	TArray<FBlip>   Blips;
	FTimerHandle    RescanTimer;
};
