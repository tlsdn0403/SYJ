// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/LootItemBase.h"
#include "Stage1ItemSpawnPoint.generated.h"

class USceneComponent;

USTRUCT(BlueprintType)
struct FStage1ItemSpawnOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<ALootItemBase> ItemClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

UCLASS()
class FPSPROJECT_API AStage1ItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AStage1ItemSpawnPoint();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void SpawnItem();

	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void ClearSpawnedItem();

	UFUNCTION(BlueprintPure, Category = "Spawn")
	ALootItemBase* GetSpawnedItem() const { return SpawnedItem; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FStage1ItemSpawnOption> SpawnOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	bool bRespawnOnPickup = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	bool bRandomYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

private:
	UFUNCTION()
	void HandleSpawnedItemDestroyed(AActor* DestroyedActor);

	void ScheduleRespawn();
	TSubclassOf<ALootItemBase> ChooseItemClass() const;

	UPROPERTY(Transient)
	ALootItemBase* SpawnedItem = nullptr;

	FTimerHandle RespawnTimerHandle;
};
