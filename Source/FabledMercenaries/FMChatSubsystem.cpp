#include "FMChatSubsystem.h"
#include "Engine/World.h"

void UFMChatSubsystem::Post(EFMChatChannel Channel, const FString& Sender, const FString& Text)
{
	if (Text.IsEmpty()) return;

	FFMChatMessage Msg;
	Msg.Channel = Channel;
	Msg.Sender  = Sender;
	Msg.Text    = Text;
	if (const UWorld* W = GetWorld()) Msg.TimeSeconds = W->GetTimeSeconds();

	History.Add(Msg);

	// 오래된 것부터 버린다. 한 번에 여러 개가 넘칠 일은 없지만 루프로 둬야 안전하다
	const int32 Limit = FMath::Max(1, MaxHistory);
	if (History.Num() > Limit)
		History.RemoveAt(0, History.Num() - Limit, EAllowShrinking::No);

	OnMessageAdded.Broadcast(Msg);
}

bool UFMChatSubsystem::Submit(const FString& Line)
{
	const FString Trimmed = Line.TrimStartAndEnd();
	if (Trimmed.IsEmpty()) return true;           // 빈 줄 — 그냥 창만 닫는다

	if (Trimmed.StartsWith(TEXT("/")))
		return HandleCommand(Trimmed);

	// ── 서버가 붙으면 이 줄이 패킷 전송으로 바뀐다 ──
	// 지금은 로컬이라 바로 되돌려 찍는다. 서버 방식이 되면 여기서 보내기만 하고,
	// 서버가 돌려준 메시지를 Post()로 받게 된다 (내가 친 말도 포함).
	Post(EFMChatChannel::Say, PlayerName, Trimmed);
	return true;
}

bool UFMChatSubsystem::HandleCommand(const FString& Line)
{
	FString Cmd = Line;
	FString Args;
	Line.Split(TEXT(" "), &Cmd, &Args);          // 공백이 없으면 Cmd는 Line 그대로 남는다
	Cmd = Cmd.ToLower();
	Args = Args.TrimStartAndEnd();

	if (Cmd == TEXT("/clear") || Cmd == TEXT("/c"))
	{
		Clear();
		return true;
	}

	if (Cmd == TEXT("/name") || Cmd == TEXT("/nick"))
	{
		if (Args.IsEmpty())
		{
			PostSystem(TEXT("사용법: /name <이름>"));
			return true;
		}
		const FString Old = PlayerName;
		PlayerName = Args;
		PostSystem(FString::Printf(TEXT("이름을 '%s' → '%s' 로 바꿨습니다"), *Old, *PlayerName));
		return true;
	}

	if (Cmd == TEXT("/help") || Cmd == TEXT("/?"))
	{
		PostSystem(TEXT("/help  이 도움말"));
		PostSystem(TEXT("/clear  기록 비우기"));
		PostSystem(TEXT("/name <이름>  내 이름 바꾸기"));
		return true;
	}

	PostSystem(FString::Printf(TEXT("모르는 명령입니다: %s  (/help)"), *Cmd));
	return true;
}

void UFMChatSubsystem::Clear()
{
	History.Reset();
	OnCleared.Broadcast();
}
