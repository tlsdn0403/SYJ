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
			UE_LOG(LogTemp, Verbose, TEXT("[ZombieSync] %s BeginPlay skipped auto-spawn because GameServer is connected"), *GetName());
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
		UE_LOG(LogTemp, Verbose, TEXT("[ZombieSync] %s SpawnZombies skipped: already spawned once"), *GetName());
		return;
	}

	if (!ZombieClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] ZombieSpawner %s has no ZombieClass assigned."), *GetName());
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[ZombieSync] %s SpawnZombies start. SpawnPointCount=%d"), *GetName(), SpawnPoints.Num());

	TArray<FVector> UsedSpawnLocations;
	for (ATargetPoint* SpawnPoint : SpawnPoints)
	{
		if (!IsValid(SpawnPoint))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] %s has an invalid SpawnPoint"), *GetName());
			continue;
		}

		const FVector BaseSpawnLocation = SpawnPoint->GetActorLocation();
		FVector SpawnLocation = BaseSpawnLocation;
		const float MinSpawnSpacingSq = MinSpawnSpacing * MinSpawnSpacing;
		if (MinSpawnSpacing > 0.0f)
		{
			for (int32 AttemptIndex = 0; AttemptIndex < 16; ++AttemptIndex)
			{
				bool bTooClose = false;
				for (const FVector& UsedSpawnLocation : UsedSpawnLocations)
				{
					if (FVector::DistSquared2D(SpawnLocation, UsedSpawnLocation) < MinSpawnSpacingSq)
					{
						bTooClose = true;
						break;
					}
				}

				if (!bTooClose)
				{
					break;
				}

				const float AngleRadians = FMath::DegreesToRadians((UsedSpawnLocations.Num() * 137.5f) + (AttemptIndex * 45.0f));
				const float Radius = MinSpawnSpacing * (1.0f + static_cast<float>(AttemptIndex / 8));
				SpawnLocation = BaseSpawnLocation + FVector(FMath::Cos(AngleRadians) * Radius, FMath::Sin(AngleRadians) * Radius, 0.0f);
			}
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = SpawnCollisionHandling;

		if (ABaseZombie* SpawnedZombie = GetWorld()->SpawnActor<ABaseZombie>(
			ZombieClass,
			SpawnLocation,
			SpawnPoint->GetActorRotation(),
			SpawnParameters))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[ZombieSync] Spawned zombie %s at %s"), *GetNameSafe(SpawnedZombie), *SpawnLocation.ToString());
			SpawnedZombies.Add(SpawnedZombie);
			UsedSpawnLocations.Add(SpawnedZombie->GetActorLocation());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] Failed to spawn zombie from spawner %s at %s"), *GetName(), *SpawnLocation.ToString());
		}
	}

	bHasSpawnedOnce = true;
	UE_LOG(LogTemp, Verbose, TEXT("[ZombieSync] %s SpawnZombies complete. SpawnedCount=%d"), *GetName(), SpawnedZombies.Num());
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
