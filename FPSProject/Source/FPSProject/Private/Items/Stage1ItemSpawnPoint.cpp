// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Stage1ItemSpawnPoint.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AStage1ItemSpawnPoint::AStage1ItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AStage1ItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnItem();
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

	GetWorld()->GetTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&AStage1ItemSpawnPoint::SpawnItem,
		RespawnDelay,
		false);
}

TSubclassOf<ALootItemBase> AStage1ItemSpawnPoint::ChooseItemClass() const
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

	float TargetWeight = FMath::FRandRange(0.0f, TotalWeight);
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
