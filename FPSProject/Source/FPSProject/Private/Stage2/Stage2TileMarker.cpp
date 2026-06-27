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
	NextTileTrigger->SetGenerateOverlapEvents(true);
	NextTileTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	NextTileTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

	ZombieSpawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZombieSpawnRoot"));
	ZombieSpawnRoot->SetupAttachment(SceneRoot);
}

// 게임중에 호출되지 않음, 에디터에서 배치하거나 변경이 될 때 호출이 됨.
void AStage2TileMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 시작 화살표 설정
	/*if (EntryArrow)
	{
		EntryArrow->SetRelativeLocation(FVector::ZeroVector);
		EntryArrow->SetRelativeRotation(FRotator::ZeroRotator);
	}*/
}

void AStage2TileMarker::BeginPlay()
{
	Super::BeginPlay();

	if (NextTileTrigger)
	{
		NextTileTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		NextTileTrigger->SetGenerateOverlapEvents(true);
		NextTileTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		NextTileTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
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
// 다음 타일이 스폰될 위치를 구함
FTransform AStage2TileMarker::GetNextTileSpawnTransform() const
{
	const FTransform ExitTransform = GetExitTransform();
	return ExitTransform;
}

TArray<FTransform> AStage2TileMarker::GetZombieSpawnTransforms(bool bIncludeRootIfNoChildren) const
{
	TArray<FTransform> SpawnTransforms;

	if (!ZombieSpawnRoot)
	{
		return SpawnTransforms;
	}

	const TArray<USceneComponent*>& SpawnChildren = ZombieSpawnRoot->GetAttachChildren();
	for (USceneComponent* SpawnChild : SpawnChildren)
	{
		if (!IsValid(SpawnChild))
		{
			continue;
		}

		SpawnTransforms.Add(SpawnChild->GetComponentTransform());
	}

	if (SpawnTransforms.Num() == 0 && bIncludeRootIfNoChildren)
	{
		SpawnTransforms.Add(ZombieSpawnRoot->GetComponentTransform());
	}

	return SpawnTransforms;
}

void AStage2TileMarker::ResetNextTileTrigger()
{
	bHasTriggeredNextTile = false;
}

// 다음 타일 스폰 트리거 끄는 함수 ( 골인지점에서 필요 없으니까)
void AStage2TileMarker::SetNextTileTriggerEnabled(bool bEnabled)
{
	if (!NextTileTrigger)
	{
		return;
	}

	NextTileTrigger->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
// 
void AStage2TileMarker::HandleNextTileTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{

	// Engine overlap signature 때문에 매개변수 많음 OtherActor만 사용이 된다.

	if (!OtherActor)
	{
		return;
	}

	ATruck* TriggerTruck = Cast<ATruck>(OtherActor);
	if (!TriggerTruck)
	{
		return;
	}

	if (bHasTriggeredNextTile)
	{
		return;
	}

	bHasTriggeredNextTile = true;
	OnNextTileTriggerEntered.Broadcast(this, TriggerTruck);
}
