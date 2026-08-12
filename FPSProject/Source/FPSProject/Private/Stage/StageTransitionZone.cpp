#include "Stage/StageTransitionZone.h"

#include "ClientPacketHandler.h"
#include "Components/BoxComponent.h"
#include "FPSProjectGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Protocol.pb.h"
#include "Truck/Truck.h"

namespace
{
	const FName StaticStage2LevelName(TEXT("/Game/Maps/map_level2/0812_NEWMAP_Ba"));

	FName ResolveStageTransitionTargetLevelName(const FName& TargetLevelName)
	{
		if (TargetLevelName.IsNone())
		{
			return TargetLevelName;
		}

		const FString TargetLevelString = TargetLevelName.ToString();
		if (TargetLevelString.Contains(TEXT("map_level2_test"), ESearchCase::IgnoreCase))
		{
			return StaticStage2LevelName;
		}

		return TargetLevelName;
	}
}

AStageTransitionZone::AStageTransitionZone()
{
	// 매 프레임 tick 함수 호출 X
	PrimaryActorTick.bCanEverTick = false;

	TransitionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TransitionBox"));
	SetRootComponent(TransitionBox);

	TransitionBox->SetBoxExtent(FVector(300.0f, 600.0f, 250.0f));
	TransitionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TransitionBox->SetGenerateOverlapEvents(true);
	TransitionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	// vehicle 채널과 겹칠 때만 오버랩 이벤트 발생하도록.
	TransitionBox->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
}

void AStageTransitionZone::BeginPlay()
{
	Super::BeginPlay();

	if (TransitionBox)
	{
		TransitionBox->SetGenerateOverlapEvents(true);
		TransitionBox->OnComponentBeginOverlap.AddDynamic(this, &AStageTransitionZone::HandleTransitionBoxBeginOverlap);
	}
}

void AStageTransitionZone::TravelToTargetLevel(ATruck* TriggerTruck)
{
	if (bTriggerOnce && bHasTriggered)
	{
		return;
	}

	const FName ResolvedTargetLevelName = ResolveStageTransitionTargetLevelName(TargetLevelName);
	if (!TriggerTruck || ResolvedTargetLevelName.IsNone())
	{
		return;
	}

	if (bRequireLoadingPhaseFinished && TriggerTruck->IsLoadingPhase())
	{
		UE_LOG(LogTemp, Log, TEXT("[StageTransition] Truck reached transition zone before loading phase finished. Truck=%s"),
			*GetNameSafe(TriggerTruck));
		return;
	}

	bHasTriggered = true;

	if (UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
	{
		if (GameInstance->IsConnectedToGameServer())
		{
			Protocol::C_STAGE_TRANSITION_REQUEST RequestPkt;
			RequestPkt.set_truck_id(TriggerTruck->NetworkTruckId);
			RequestPkt.set_target_level(TCHAR_TO_UTF8(*ResolvedTargetLevelName.ToString()));
			GameInstance->SendPacket(ClientPacketHandler::MakeSendBuffer(RequestPkt));
			return;
		}
	}

	UGameplayStatics::OpenLevel(this, ResolvedTargetLevelName);
}

void AStageTransitionZone::HandleTransitionBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 넘어온 엑터가 트럭이라면
	ATruck* TriggerTruck = Cast<ATruck>(OtherActor);
	if (!TriggerTruck)
	{
		return;
	}

	TravelToTargetLevel(TriggerTruck);
}