#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sim/Class.h"
#include "FMEncounter.generated.h"

/**
 * Sim의 `Class`(Sim/Class.h) 미러. Sim은 엔진 비의존이라 UENUM을 못 달기 때문에
 * BP/데이터테이블에서 고를 수 있는 사본을 여기에 둔다.
 * 값이 어긋나면 컴파일 타임에 잡히도록 아래 static_assert로 묶어둔다.
 */
UENUM(BlueprintType)
enum class EFMClass : uint8
{
	None     = 0,
	Warrior  = 1  UMETA(DisplayName = "전사"),
	Tanker   = 2  UMETA(DisplayName = "탱커"),
	Mage     = 3  UMETA(DisplayName = "마법사"),
	Archer   = 4  UMETA(DisplayName = "궁수"),
	Assassin = 5  UMETA(DisplayName = "암살자"),
	Healer   = 6  UMETA(DisplayName = "힐러")
};

static_assert((uint8)EFMClass::Warrior  == (uint8)Class::Warrior,  "EFMClass가 Sim의 Class와 어긋남");
static_assert((uint8)EFMClass::Tanker   == (uint8)Class::Tanker,   "EFMClass가 Sim의 Class와 어긋남");
static_assert((uint8)EFMClass::Mage     == (uint8)Class::Mage,     "EFMClass가 Sim의 Class와 어긋남");
static_assert((uint8)EFMClass::Archer   == (uint8)Class::Archer,   "EFMClass가 Sim의 Class와 어긋남");
static_assert((uint8)EFMClass::Assassin == (uint8)Class::Assassin, "EFMClass가 Sim의 Class와 어긋남");
static_assert((uint8)EFMClass::Healer   == (uint8)Class::Healer,   "EFMClass가 Sim의 Class와 어긋남");

/** EFMClass → Sim Class */
FORCEINLINE Class ToSimClass(EFMClass C) { return (Class)(uint8)C; }

/** 유닛 1기의 편성 정보 */
USTRUCT(BlueprintType)
struct FFMUnitSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	EFMClass UnitClass = EFMClass::Warrior;

	/**
	 * 자기 진영 기준선에서의 상대 배치(cm).
	 * X = 상대 진영 쪽으로 얼마나 앞에 설지, Y = 좌우.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FVector2D Offset = FVector2D::ZeroVector;
};

/**
 * 인카운터 1건의 정의 — 심볼이 들고 있다가 접촉 시 SimManager에 넘긴다.
 * DataTable 행으로도 쓸 수 있게 FTableRowBase 상속.
 */
USTRUCT(BlueprintType)
struct FFMEncounterDef : public FTableRowBase
{
	GENERATED_BODY()

	/** 로그/디버그용 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FName EncounterId;

	/** 적 편성 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FFMUnitSpawn> Enemies;

	/**
	 * 아군 편성. 지금은 여기 직접 적지만, 메타 로스터가 생기면
	 * 비워두고 MetaPlayer의 용병 목록에서 채우게 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FFMUnitSpawn> Allies;

	/** 양 진영 기준선 사이 거리(cm). 조우 지점을 가운데 두고 절반씩 물러선다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	float Separation = 600.f;
};
