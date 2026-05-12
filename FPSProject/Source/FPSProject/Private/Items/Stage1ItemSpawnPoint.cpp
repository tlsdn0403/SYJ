// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Stage1ItemSpawnPoint.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "FPSProjectGameInstance.h"
#include "TimerManager.h"

AStage1ItemSpawnPoint::AStage1ItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EditorPreviewSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EditorPreviewSphere"));
	EditorPreviewSphere->SetupAttachment(SceneRoot);
	EditorPreviewSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorPreviewSphere->SetGenerateOverlapEvents(false);
	EditorPreviewSphere->SetHiddenInGame(true);
	EditorPreviewSphere->bDrawOnlyIfSelected = false;
	EditorPreviewSphere->ShapeColor = FColor(80, 220, 255, 255);
	EditorPreviewSphere->SetSphereRadius(PreviewSphereRadius);
}

void AStage1ItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		if (const UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
		{
			if (GameInstance->IsConnectedToGameServer())
			{
				return;
			}
		}

		SpawnItem();
	}
}

void AStage1ItemSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EditorPreviewSphere)
	{
		EditorPreviewSphere->SetSphereRadius(PreviewSphereRadius, false);
	}
}

void AStage1ItemSpawnPoint::SpawnItem()
{
	if (SpawnedItem != nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const TSubclassOf<ALootItemBase> ItemClass = ChooseItemClass();
	if (!ItemClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage1ItemSpawnPoint '%s' has no valid SpawnOptions."), *GetName());
		return;
	}

	FTransform SpawnTransform = GetActorTransform();
	if (bRandomYaw)
	{
		FRotator SpawnRotation = SpawnTransform.Rotator();
		SpawnRotation.Yaw = FMath::FRandRange(0.0f, 360.0f);
		SpawnTransform.SetRotation(SpawnRotation.Quaternion());
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = SpawnCollisionHandlingOverride;

	SpawnedItem = World->SpawnActor<ALootItemBase>(ItemClass, SpawnTransform, SpawnParameters);
	if (SpawnedItem)
	{
		SpawnedItem->ConfigureNetworkItem(ResolveSpawnedItemNetworkId(), bRespawnOnPickup, RespawnDelay);
		SpawnedItem->OnDestroyed.AddDynamic(this, &AStage1ItemSpawnPoint::HandleSpawnedItemDestroyed);
	}
}

void AStage1ItemSpawnPoint::SpawnItemFromRandomStream(FRandomStream& RandomStream)
{
	if (SpawnedItem != nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const TSubclassOf<ALootItemBase> ItemClass = ChooseItemClass(&RandomStream);
	if (!ItemClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage1ItemSpawnPoint '%s' has no valid SpawnOptions."), *GetName());
		return;
	}

	FTransform SpawnTransform = GetActorTransform();
	if (bRandomYaw)
	{
		FRotator SpawnRotation = SpawnTransform.Rotator();
		SpawnRotation.Yaw = RandomStream.FRandRange(0.0f, 360.0f);
		SpawnTransform.SetRotation(SpawnRotation.Quaternion());
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = SpawnCollisionHandlingOverride;

	SpawnedItem = World->SpawnActor<ALootItemBase>(ItemClass, SpawnTransform, SpawnParameters);
	if (SpawnedItem)
	{
		SpawnedItem->ConfigureNetworkItem(ResolveSpawnedItemNetworkId(), bRespawnOnPickup, RespawnDelay);
		SpawnedItem->OnDestroyed.AddDynamic(this, &AStage1ItemSpawnPoint::HandleSpawnedItemDestroyed);
	}
}

void AStage1ItemSpawnPoint::ClearSpawnedItem()
{
	if (SpawnedItem == nullptr)
	{
		return;
	}

	ALootItemBase* ItemToDestroy = SpawnedItem;
	SpawnedItem = nullptr;
	ItemToDestroy->Destroy();
}

void AStage1ItemSpawnPoint::HandleSpawnedItemDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor != SpawnedItem)
	{
		return;
	}

	SpawnedItem = nullptr;
	ScheduleRespawn();
}

void AStage1ItemSpawnPoint::ScheduleRespawn()
{
	if (!bRespawnOnPickup || RespawnDelay < 0.0f || GetWorld() == nullptr)
	{
		return;
	}

	if (const UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
	{
		if (GameInstance->IsConnectedToGameServer())
		{
			return;
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&AStage1ItemSpawnPoint::SpawnItem,
		RespawnDelay,
		false);
}

TSubclassOf<ALootItemBase> AStage1ItemSpawnPoint::ChooseItemClass(FRandomStream* RandomStream) const
{
	float TotalWeight = 0.0f;
	for (const FStage1ItemSpawnOption& SpawnOption : SpawnOptions)
	{
		if (SpawnOption.ItemClass && SpawnOption.Weight > 0.0f)
		{
			TotalWeight += SpawnOption.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float TargetWeight = (RandomStream != nullptr)
		? RandomStream->FRandRange(0.0f, TotalWeight)
		: FMath::FRandRange(0.0f, TotalWeight);

	for (const FStage1ItemSpawnOption& SpawnOption : SpawnOptions)
	{
		if (!SpawnOption.ItemClass || SpawnOption.Weight <= 0.0f)
		{
			continue;
		}

		TargetWeight -= SpawnOption.Weight;
		if (TargetWeight <= 0.0f)
		{
			return SpawnOption.ItemClass;
		}
	}

	for (const FStage1ItemSpawnOption& SpawnOption : SpawnOptions)
	{
		if (SpawnOption.ItemClass && SpawnOption.Weight > 0.0f)
		{
			return SpawnOption.ItemClass;
		}
	}

	return nullptr;
}

uint64 AStage1ItemSpawnPoint::ResolveSpawnedItemNetworkId() const
{
	return static_cast<uint64>(GetTypeHash(FPSProjectStableActorIdUtils::BuildStableActorKey(this) + TEXT(":spawned")));
}
