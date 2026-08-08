#include "Core/FM_CommanderAvatar.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Sim/Unit.h"
#include "Sim/Class.h"

AFM_CommanderAvatar::AFM_CommanderAvatar()
{
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(42.f, 96.f);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(CapsuleComponent);
	// 캡슐 바닥에 메시 발이 닿도록 살짝 내림
	MeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
}

void AFM_CommanderAvatar::BeginPlay()
{
	Super::BeginPlay();

	// 지휘관 등록 → 이 아바타를 그 지휘관의 Unit으로 등록
	Sim.AddCommander(COMMANDER_ID);

	const FVector StartLoc = GetActorLocation();
	const Vec3 StartPos{ StartLoc.X, StartLoc.Y, StartLoc.Z };

	// brain=nullptr → 사람(외부 명령)이 조종. 클래스=전사 임시값.
	Sim.AddUnit(MyUnitId, COMMANDER_ID, Faction::Player, Class::Warrior, StartPos);

	// 검증용: BeginPlay 직후 한 번 자동 이동 명령
	if (bAutoIssueOnBegin)
	{
		IssueMove(StartLoc + AutoMoveOffset, /*bReserve=*/false);
	}
}

void AFM_CommanderAvatar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ① Sim 한 틱 진행
	Sim.Tick(DeltaTime);

	// ② Sim의 좌표를 액터에 반영
	if (Unit* U = Sim.GetUnit(MyUnitId))
	{
		SetActorLocation(FVector(U->pos.x, U->pos.y, U->pos.z));
	}
}

void AFM_CommanderAvatar::IssueMove(const FVector& WorldPos, bool bReserve)
{
	Command cmd;
	cmd.type = CommandType::Move;
	cmd.waypoints.push_back(Vec3{ WorldPos.X, WorldPos.Y, WorldPos.Z });
	Sim.IssueCommand(MyUnitId, cmd, bReserve);
}
