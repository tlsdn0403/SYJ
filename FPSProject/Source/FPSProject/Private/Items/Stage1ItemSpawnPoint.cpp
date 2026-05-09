// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Stage1ItemSpawnPoint.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
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
		//게임 시작할 떄 아이템을 스폰함
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
	// Yaw 랜덤 회전
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
		// 실제로 아이템을 생성, 먹었을 떄 파괴될 수 있도록 델리게이트.
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

// 어떤 아이템을 생성할지 고름
TSubclassOf<ALootItemBase> AStage1ItemSpawnPoint::ChooseItemClass() const
{
	float TotalWeight = 0.0f;
	// 전체 Weight 계산
	for (const FStage1ItemSpawnOption& SpawnOption : SpawnOptions)
	{
		if (SpawnOption.ItemClass && SpawnOption.Weight > 0.0f)
		{
			TotalWeight += SpawnOption.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		// 총 가중치가 0이면 nullptr 반환
		return nullptr;
	}

	// total weight 범위에서 랜덤한 값 선택
	float TargetWeight = FMath::FRandRange(0.0f, TotalWeight);
	for (const FStage1ItemSpawnOption& SpawnOption : SpawnOptions)
	{
		if (!SpawnOption.ItemClass || SpawnOption.Weight <= 0.0f)
		{
			continue;
		}
		// 랜덤 값에서 각 아이템의 Weight를 차례대로 빼다가 0 이하가 되는 순간 그 아이템을 선택
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
