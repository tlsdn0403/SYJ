#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieSpawner.generated.h"

class ABaseZombie;
class ATargetPoint;
class USceneComponent;

UCLASS(Blueprintable)
class FPSPROJECT_API AZombieSpawner : public AActor
{
	GENERATED_BODY()

public:
	AZombieSpawner();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "ZombieSpawner")
	void SpawnZombies();

	UFUNCTION(BlueprintCallable, Category = "ZombieSpawner")
	void ClearSpawnedZombies();

	UFUNCTION(BlueprintCallable, Category = "ZombieSpawner")
	void SetSpawnOnBeginPlayEnabled(bool bEnabled) { bSpawnOnBeginPlay = bEnabled; }

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "ZombieSpawner")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZombieSpawner")
	TSubclassOf<ABaseZombie> ZombieClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZombieSpawner")
	TArray<TObjectPtr<ATargetPoint>> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZombieSpawner")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZombieSpawner")
	bool bSpawnOnlyOnce = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZombieSpawner")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZombieSpawner")
	TArray<TObjectPtr<ABaseZombie>> SpawnedZombies;

private:
	bool bHasSpawnedOnce = false;
};
