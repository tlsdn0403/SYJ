#include "Stage/StageTransitionZone.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Truck/Truck.h"

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

	if (!TriggerTruck || TargetLevelName.IsNone())
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
	UGameplayStatics::OpenLevel(this, TargetLevelName);
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
