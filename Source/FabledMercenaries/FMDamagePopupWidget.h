#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FMDamagePopupWidget.generated.h"

/** 떠 있는 숫자 하나 */
USTRUCT()
struct FFMDamagePopup
{
	GENERATED_BODY()

	FVector      WorldPos = FVector::ZeroVector;   // 출발 지점 (유닛 머리 위)
	FString      Text;
	FLinearColor Color    = FLinearColor::White;
	float        Scale    = 1.f;
	float        BornTime = 0.f;                   // 월드 시간(초)
};

/**
 * 데미지 팝업 레이어 — 화면 전체를 덮는 투명 위젯 하나가 모든 숫자를 그린다.
 *
 * 유닛마다 WidgetComponent를 달지 않는 이유: 유닛이 수십 기라도 위젯은 하나이고,
 * 유닛이 죽어 액터가 사라져도 떠 있던 숫자가 같이 날아가지 않는다.
 *
 * 그리기는 NativePaint에서 직접 한다. 이 프로젝트의 UUserWidget은 NativeTick이 돌지 않아
 * (DisableNativeTick) 애니메이션을 틱으로 굴릴 수 없다 — 대신 월드 시간과 태어난 시각의
 * 차이로 매 프레임 위치·투명도를 계산한다. 미니맵(FMMinimapWidget)과 같은 방식.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMDamagePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 월드 한 지점에 숫자를 띄운다. BP에서 직접 불러도 된다(연출·디버그용) */
	UFUNCTION(BlueprintCallable, Category = "Popup")
	void AddPopup(const FVector& WorldPos, const FString& Text, bool bCrit, bool bAlly);

protected:
	/** 떠오르는 데 걸리는 시간(초). 이 시간이 지나면 사라진다 */
	UPROPERTY(EditDefaultsOnly, Category = "Popup") float LifeTime = 1.1f;

	/** 수명 동안 화면에서 올라가는 높이(픽셀) */
	UPROPERTY(EditDefaultsOnly, Category = "Popup") float RisePixels = 70.f;

	/** 이 비율을 지난 뒤부터 흐려지기 시작 (0~1) */
	UPROPERTY(EditDefaultsOnly, Category = "Popup") float FadeStart = 0.45f;

	/** 터질 때 살짝 커졌다 돌아오는 정도 (1 = 효과 없음) */
	UPROPERTY(EditDefaultsOnly, Category = "Popup") float PopScale = 1.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Popup") int32 BaseFontSize = 20;
	UPROPERTY(EditDefaultsOnly, Category = "Popup") float CritScale    = 1.5f;

	/** 적이 맞았을 때 / 아군이 맞았을 때 / 후면 크리 */
	UPROPERTY(EditDefaultsOnly, Category = "Popup|Color") FLinearColor EnemyHitColor = FLinearColor(1.00f, 0.95f, 0.80f);
	UPROPERTY(EditDefaultsOnly, Category = "Popup|Color") FLinearColor AllyHitColor  = FLinearColor(1.00f, 0.35f, 0.30f);
	UPROPERTY(EditDefaultsOnly, Category = "Popup|Color") FLinearColor CritColor     = FLinearColor(1.00f, 0.75f, 0.15f);

	/** 한 번에 떠 있을 수 있는 최대 개수 — 범위 마법처럼 한꺼번에 터질 때 방어선 */
	UPROPERTY(EditDefaultsOnly, Category = "Popup") int32 MaxPopups = 64;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION()
	void HandleUnitHit(FVector WorldPos, const FString& Text, bool bCrit, bool bAlly);

private:
	class AFMSimManager* FindSimManager() const;

	/** const NativePaint에서 수명 지난 항목을 지우기 위해 mutable */
	mutable TArray<FFMDamagePopup> Popups;
};
