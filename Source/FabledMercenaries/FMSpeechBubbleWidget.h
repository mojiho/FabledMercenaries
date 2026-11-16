#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "FMChatSubsystem.h"
#include "FMSpeechBubbleWidget.generated.h"

/** 머리 위에 떠 있는 말풍선 하나 */
USTRUCT()
struct FFMSpeechBubble
{
	GENERATED_BODY()

	/** 말한 사람. 같은 사람이 다시 말하면 이전 풍선을 갈아끼운다 */
	FString Sender;

	/** 미리 줄바꿈해 둔 본문. 폭은 말할 때 한 번만 재면 되고, 매 프레임 다시 잴 이유가 없다 */
	TArray<FString> Lines;

	/** Lines 중 가장 긴 줄의 폭(픽셀) — 풍선 상자 크기 */
	float WidthPx = 0.f;

	float BornTime = 0.f;
	float Life     = 2.5f;
};

/**
 * 말풍선 레이어 — 화면 전체를 덮는 투명 위젯 하나가 모든 풍선을 그린다.
 *
 * 채팅창(UFMChatWidget)이 '기록'이라면 이쪽은 '지금 누가 뭐라고 했는지'다.
 * 채팅 버스의 Say 메시지만 받아 말한 사람 머리 위에 띄우고, 잠시 뒤 사라진다.
 *
 * 구조는 UFMDamagePopupWidget과 같은 이유로 같은 모양이다:
 *  - 캐릭터마다 WidgetComponent를 달지 않는다. 위젯은 하나고, 말한 사람이 사라져도
 *    떠 있던 풍선이 같이 날아가지 않는다.
 *  - 이 프로젝트의 UUserWidget은 NativeTick이 돌지 않으므로(DisableNativeTick)
 *    NativePaint에서 월드 시간과 태어난 시각의 차이로 위치·투명도를 계산한다.
 *
 * 데미지 숫자와 다른 점은 풍선이 '따라다닌다'는 것이다 — 월드 좌표를 박아 두지 않고
 * 매 프레임 말한 사람을 다시 찾는다. 걸어가면서 말하면 풍선도 같이 간다.
 *
 * ── 서버가 붙을 때 ──
 * 고칠 곳은 ResolveSpeaker() 하나다. 지금은 사람이 하나뿐이라 '내 아바타'만 찾지만,
 * 이름 → 액터 조회로 바꾸면 남의 말풍선도 그대로 뜬다. 그리는 쪽은 손댈 필요가 없다.
 */
UCLASS(Abstract)
class FABLEDMERCENARIES_API UFMSpeechBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 특정 월드 좌표가 아니라 '사람'에게 띄운다. BP/연출에서 직접 불러도 된다 */
	UFUNCTION(BlueprintCallable, Category = "Speech")
	void AddBubble(const FString& Sender, const FString& Text);

	/** 그 사람의 풍선을 지금 지운다 (대화 연출이 끊길 때 등) */
	UFUNCTION(BlueprintCallable, Category = "Speech")
	void ClearBubble(const FString& Sender);

protected:
	/** 짧은 말도 이만큼은 떠 있는다(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float MinLife = 2.0f;

	/** 글자 하나당 더해지는 시간(초) — 긴 말을 읽을 틈을 준다 */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float LifePerChar = 0.09f;

	/** 아무리 길어도 이 시간을 넘기지 않는다(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float MaxLife = 7.0f;

	/** 이 비율을 지난 뒤부터 흐려진다 (0~1) */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float FadeStart = 0.8f;

	/** 머리 위 높이(cm). 발밑 기준이라 유닛 키보다 커야 한다 */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float HeadOffset = 195.f;

	/** 머리와 풍선 바닥 사이 간격(픽셀) */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float GapPixels = 10.f;

	/** 풍선 한 줄의 최대 폭(픽셀). 넘으면 줄을 바꾼다 */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float MaxWidthPixels = 260.f;

	/** 이 줄 수를 넘으면 잘라내고 말줄임표를 붙인다 */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") int32 MaxLines = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Speech") int32 FontSize = 14;
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float PaddingX = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float PaddingY = 6.f;
	UPROPERTY(EditDefaultsOnly, Category = "Speech") float CornerRadius = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Speech|Color") FLinearColor BubbleColor  = FLinearColor(0.04f, 0.05f, 0.07f, 0.86f);
	UPROPERTY(EditDefaultsOnly, Category = "Speech|Color") FLinearColor OutlineColor = FLinearColor(0.55f, 0.62f, 0.75f, 0.55f);
	UPROPERTY(EditDefaultsOnly, Category = "Speech|Color") FLinearColor TextColor    = FLinearColor(0.96f, 0.96f, 0.96f, 1.f);

	/** 한 번에 떠 있을 수 있는 최대 개수 */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") int32 MaxBubbles = 16;

	/**
	 * 전투 중에도 띄울지. 월드맵·탐험맵이 주 무대라 기본은 켜 둔다 —
	 * 전투 화면이 복잡해서 거슬리면 끄면 된다 (채팅창에는 그대로 남는다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Speech") bool bShowInCombat = true;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION()
	void HandleMessageAdded(const FFMChatMessage& Msg);

private:
	class AFMSimManager* FindSimManager() const;
	class UFMChatSubsystem* Chat() const;

	/**
	 * 말한 사람의 머리 위 월드 좌표. 못 찾으면 false — 그 풍선은 이번 프레임에 안 그린다
	 * (사라지지는 않는다. 카메라 밖에 있다가 돌아오면 다시 보여야 하므로).
	 */
	bool ResolveSpeaker(const FString& Sender, FVector& OutHeadPos) const;

	FSlateFontInfo MakeFont() const;

	/** 폭에 맞춰 줄을 나눈다. 공백이 없으면(한글) 글자 단위로 끊는다 */
	TArray<FString> WrapText(const FString& Text, const FSlateFontInfo& FontInfo, float& OutWidthPx) const;

	/** const NativePaint에서 수명 지난 항목을 지우기 위해 mutable */
	mutable TArray<FFMSpeechBubble> Bubbles;

	/** 외곽선 색이 투명도를 따라가야 해서 그릴 때마다 손본다 */
	mutable FSlateBrush BubbleBrush;
};
