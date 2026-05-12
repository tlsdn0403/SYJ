#include "Zombie/ZombieSpawner.h"

#include "Components/SceneComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "FPSProjectGameInstance.h"
#include "Zombie/BaseZombie.h"

AZombieSpawner::AZombieSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AZombieSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (const UFPSProjectGameInstance* GameInstance = GetWorld() ? Cast<UFPSProjectGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (GameInstance->IsConnectedToGameServer())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s BeginPlay skipped auto-spawn because GameServer is connected"), *GetName());
			return;
		}
	}

	if (bSpawnOnBeginPlay)
	{
		SpawnZombies();
	}
}

void AZombieSpawner::SpawnZombies()
{
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s SpawnZombies failed: World is null"), *GetName());
		return;
	}

	if (bSpawnOnlyOnce && bHasSpawnedOnce)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s SpawnZombies skipped: already spawned once"), *GetName());
		return;
	}

	if (!ZombieClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] ZombieSpawner %s has no ZombieClass assigned."), *GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s SpawnZombies start. SpawnPointCount=%d"), *GetName(), SpawnPoints.Num());

	for (ATargetPoint* SpawnPoint : SpawnPoints)
	{
		if (!IsValid(SpawnPoint))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s has an invalid SpawnPoint"), *GetName());
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = SpawnCollisionHandling;

		if (ABaseZombie* SpawnedZombie = GetWorld()->SpawnActor<ABaseZombie>(
			ZombieClass,
			SpawnPoint->GetActorLocation(),
			SpawnPoint->GetActorRotation(),
			SpawnParameters))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] Spawned zombie %s at %s"), *GetNameSafe(SpawnedZombie), *SpawnPoint->GetActorLocation().ToString());
			SpawnedZombies.Add(SpawnedZombie);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] Failed to spawn zombie from spawner %s at %s"), *GetName(), *SpawnPoint->GetActorLocation().ToString());
		}
	}

	bHasSpawnedOnce = true;
	UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s SpawnZombies complete. SpawnedCount=%d"), *GetName(), SpawnedZombies.Num());
}

void AZombieSpawner::ClearSpawnedZombies()
{
	for (ABaseZombie* SpawnedZombie : SpawnedZombies)
	{
		if (IsValid(SpawnedZombie))
		{
			SpawnedZombie->Destroy();
		}
	}

	SpawnedZombies.Empty();
	bHasSpawnedOnce = false;
}