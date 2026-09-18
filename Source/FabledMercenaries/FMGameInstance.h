#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Meta/Player.h"
#include "FMEncounter.h"
#include "FMGameInstance.generated.h"

/**
 * 레벨을 넘나들며 살아남아야 하는 것만 담는다.
 *
 * 월드맵 → 컴뱃맵으로 OpenLevel 하면 레벨 안의 액터는 전부 파괴된다.
 * 인벤토리·로스터 같은 메타 데이터가 SimManager에 붙어 있으면 그때 같이 날아가므로
 * 여기로 올린다. (전투 Sim은 여전히 이 데이터를 모른다 — itemId만 주고받는다)
 */

USTRUCT(BlueprintType)
struct FFMMercInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32    MercId    = 0;
	UPROPERTY(BlueprintReadOnly) EFMClass UnitClass = EFMClass::None;
	UPROPERTY(BlueprintReadOnly) FText    ClassName;
	UPROPERTY(BlueprintReadOnly) float    Hp        = 0.f;
	UPROPERTY(BlueprintReadOnly) float    MaxHp     = 0.f;
	UPROPERTY(BlueprintReadOnly) bool     bAlive    = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFMRosterChanged);

UCLASS()
class FABLEDMERCENARIES_API UFMGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	/** 전투 밖 데이터 (인벤토리 등) */
	MetaPlayer& GetMeta() { return Meta; }
	const MetaPlayer& GetMeta() const { return Meta; }

	/**
	 * 월드맵 휴식 — 사망한 용병 부활 + 전원 체력 회복.
	 * 월드맵에서만 불려야 한다. 호출부(AFMRestNode)가 FMSimManager.bIsWorldMap으로 막는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Roster")
	void RestAllMercenaries();

	UFUNCTION(BlueprintPure, Category = "Roster")
	int32 GetMercenaryCount() const { return (int32)Meta.roster.size(); }

	UFUNCTION(BlueprintPure, Category = "Roster")
	int32 GetAliveMercenaryCount() const { return Meta.AliveMercCount(); }

	/** 로스터 상태를 한 줄로 로그 — 전투 기록/휴식 검증용 */
	void LogRoster(const TCHAR* Context) const;

	/** 컴뱃맵에 들어가기 직전 밟고 있던 월드맵 노드 — 복귀 시 그 자리에 세우기 위함 */
	UPROPERTY(BlueprintReadWrite, Category = "World")
	FName LastWorldNodeId;

	/** 돌아갈 월드맵 레벨 이름 — 컴뱃맵에서 나올 때 쓴다 */
	UPROPERTY(BlueprintReadWrite, Category = "World")
	FName ReturnWorldMap;

	UFUNCTION(BlueprintCallable, Category = "Roster")
	TArray<FFMMercInfo> GetRoster() const;

	/** 명단이 바뀌면 알림 — 카드 UI가 구독해서 다시 그린다 */
	UPROPERTY(BlueprintAssignable, Category = "Roster")
	FFMRosterChanged OnRosterChanged;
	
private:
	MetaPlayer Meta;
};
