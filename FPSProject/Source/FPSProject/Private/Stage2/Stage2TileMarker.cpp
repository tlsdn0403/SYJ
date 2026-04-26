#include "Stage2/Stage2TileMarker.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Truck/Truck.h"

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
	NextTileTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

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

	if (!Cast<ATruck>(OtherActor))
	{
		return;
	}

	if (bTriggerOnlyOnce && bHasTriggeredNextTile)
	{
		return;
	}

	bHasTriggeredNextTile = true;
	OnNextTileTriggerEntered.Broadcast(this, OtherActor);
}
