#include "Stage2/Stage2TileMarker.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Truck/Truck.h"

namespace
{
ATruck* ResolveTruckActor(AActor* CandidateActor)
{
	for (AActor* CurrentActor = CandidateActor; CurrentActor; CurrentActor = CurrentActor->GetOwner())
	{
		if (ATruck* TruckActor = Cast<ATruck>(CurrentActor))
		{
			return TruckActor;
		}
	}

	for (AActor* CurrentActor = CandidateActor ? CandidateActor->GetAttachParentActor() : nullptr;
		CurrentActor;
		CurrentActor = CurrentActor->GetAttachParentActor())
	{
		if (ATruck* TruckActor = Cast<ATruck>(CurrentActor))
		{
			return TruckActor;
		}
	}

	return nullptr;
}
}

AStage2TileMarker::AStage2TileMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EntryArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("EntryArrow"));
	EntryArrow->SetupAttachment(SceneRoot);
	EntryArrow->ArrowColor = FColor::Green;

	ExitArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ExitArrow"));
	ExitArrow->SetupAttachment(SceneRoot);
	ExitArrow->SetRelativeLocation(FVector(5000.0f, 0.0f, 0.0f));
	ExitArrow->ArrowColor = FColor::Yellow;

	NextTileTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("NextTileTrigger"));
	NextTileTrigger->SetupAttachment(SceneRoot);
	NextTileTrigger->SetRelativeLocation(FVector(4500.0f, 0.0f, 0.0f));
	NextTileTrigger->SetBoxExtent(FVector(300.0f, 1000.0f, 400.0f));
	NextTileTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NextTileTrigger->SetGenerateOverlapEvents(true);
	NextTileTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

	ZombieSpawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZombieSpawnRoot"));
	ZombieSpawnRoot->SetupAttachment(SceneRoot);
}

void AStage2TileMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EntryArrow)
	{
		EntryArrow->SetRelativeLocation(FVector::ZeroVector);
		EntryArrow->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void AStage2TileMarker::BeginPlay()
{
	Super::BeginPlay();

	if (NextTileTrigger)
	{
		NextTileTrigger->SetGenerateOverlapEvents(true);
		NextTileTrigger->OnComponentBeginOverlap.AddDynamic(this, &AStage2TileMarker::HandleNextTileTriggerBeginOverlap);
	}
}

FTransform AStage2TileMarker::GetEntryTransform() const
{
	return EntryArrow ? EntryArrow->GetComponentTransform() : GetActorTransform();
}

FTransform AStage2TileMarker::GetExitTransform() const
{
	return ExitArrow ? ExitArrow->GetComponentTransform() : GetActorTransform();
}

FTransform AStage2TileMarker::GetNextTileSpawnTransform() const
{
	const FTransform EntryTransform = GetEntryTransform();
	const FTransform ExitTransform = GetExitTransform();

	// When the exit arrow is left at the entry point, fall back to the trigger plane.
	if (FVector::DistSquared(EntryTransform.GetLocation(), ExitTransform.GetLocation()) > FMath::Square(100.0f))
	{
		return ExitTransform;
	}

	if (NextTileTrigger)
	{
		const FTransform TriggerTransform = NextTileTrigger->GetComponentTransform();
		const FVector ForwardVector = TriggerTransform.GetUnitAxis(EAxis::X);
		const FVector SpawnLocation = TriggerTransform.GetLocation() + (ForwardVector * NextTileTrigger->GetScaledBoxExtent().X);
		return FTransform(TriggerTransform.GetRotation(), SpawnLocation, FVector::OneVector);
	}

	return ExitTransform;
}

void AStage2TileMarker::ResetNextTileTrigger()
{
	bHasTriggeredNextTile = false;
}

void AStage2TileMarker::SetNextTileTriggerEnabled(bool bEnabled)
{
	if (!NextTileTrigger)
	{
		return;
	}

	NextTileTrigger->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AStage2TileMarker::HandleNextTileTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	ATruck* TriggerTruck = ResolveTruckActor(OtherActor);
	if (!TriggerTruck)
	{
		return;
	}

	if (bTriggerOnlyOnce && bHasTriggeredNextTile)
	{
		return;
	}

	bHasTriggeredNextTile = true;
	OnNextTileTriggerEntered.Broadcast(this, TriggerTruck);
}
