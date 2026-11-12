#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FMChatSubsystem.generated.h"

/**
 * 메시지가 속한 줄기. 탭 필터와 색이 이 값으로 갈린다.
 * 값이 저장·패킷 상수가 될 수 있으니 뒤에만 덧붙일 것.
 */
UENUM(BlueprintType)
enum class EFMChatChannel : uint8
{
	Say     UMETA(DisplayName = "일반"),   // 사람이 친 말
	System  UMETA(DisplayName = "시스템"), // 레벨 이동, 휴식, 장비 등 게임 알림
	Combat  UMETA(DisplayName = "전투"),   // Sim 이벤트 (피해·사망·시전)
};

USTRUCT(BlueprintType)
struct FFMChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	EFMChatChannel Channel = EFMChatChannel::System;

	/** 말한 사람. 비어 있으면 말머리 없이 본문만 찍는다 (시스템·전투 알림) */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Sender;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Text;

	/** 월드 시간(초). 지금은 안 쓰지만 로그를 파일로 뽑거나 묶을 때 필요하다 */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	float TimeSeconds = 0.f;

	/** 화면에 찍을 한 줄 */
	FString ToLine() const
	{
		return Sender.IsEmpty() ? Text : FString::Printf(TEXT("%s: %s"), *Sender, *Text);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFMChatMessageAdded, const FFMChatMessage&, Msg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFMChatCleared);

/**
 * 채팅·로그 메시지 버스.
 *
 * 게임 어디서든 Post()로 한 줄 적으면 구독 중인 창이 받아 그린다.
 * GameInstance 소속이라 레벨을 넘어가도 기록이 남는다 (월드맵 ↔ 전투맵 왕복).
 *
 * ── 서버가 붙을 때 ──
 * 사람이 친 말은 Submit()으로 들어온다. 지금은 그대로 되돌려 찍지만,
 * 서버가 생기면 Submit()에서 패킷을 보내고 서버가 돌려준 걸 Post()로 넣으면 된다.
 * 고칠 곳은 Submit() 하나이고 창·구독자는 손댈 필요가 없다.
 */
UCLASS()
class FABLEDMERCENARIES_API UFMChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 메시지가 하나 들어올 때마다 — 창이 이걸 구독해 한 줄씩 덧붙인다 */
	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FFMChatMessageAdded OnMessageAdded;

	/** 기록을 통째로 비웠을 때 — 창이 목록을 다시 그린다 */
	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FFMChatCleared OnCleared;

	/** 메시지 1건 기록 + 방송 */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void Post(EFMChatChannel Channel, const FString& Sender, const FString& Text);

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void PostSystem(const FString& Text) { Post(EFMChatChannel::System, FString(), Text); }

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void PostCombat(const FString& Text) { Post(EFMChatChannel::Combat, FString(), Text); }

	/**
	 * 플레이어가 입력창에서 Enter로 보낸 한 줄.
	 * '/'로 시작하면 명령으로 보고, 아니면 일반 대화로 찍는다.
	 * @return 창을 닫아도 되면 true (지금은 항상 true)
	 */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	bool Submit(const FString& Line);

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void Clear();

	const TArray<FFMChatMessage>& GetHistory() const { return History; }

	/** 지금 입력창에 타이핑 중인가 — 컨트롤러가 이걸 보고 키보드 조작을 막는다 */
	UFUNCTION(BlueprintPure, Category = "Chat")
	bool IsTyping() const { return bTyping; }

	void SetTyping(bool bInTyping) { bTyping = bInTyping; }

	/** 내가 친 말에 붙을 이름. 서버가 붙으면 계정 이름으로 바뀐다 */
	UPROPERTY(BlueprintReadWrite, Category = "Chat")
	FString PlayerName = TEXT("지휘관");

	/** 이 수를 넘으면 오래된 것부터 버린다 */
	UPROPERTY(EditAnywhere, Category = "Chat")
	int32 MaxHistory = 300;

private:
	/** '/'로 시작하는 줄 처리. @return 명령으로 소화했으면 true */
	bool HandleCommand(const FString& Line);

	UPROPERTY()
	TArray<FFMChatMessage> History;

	bool bTyping = false;
};
