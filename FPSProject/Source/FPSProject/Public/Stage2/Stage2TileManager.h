#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage2/Stage2TileMarker.h"
#include "Stage2TileManager.generated.h"

class ULevelStreamingDynamic;
class UWorld;

USTRUCT(BlueprintType)
struct FStage2LoadedTile
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	TSoftObjectPtr<UWorld> SourceLevel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	TObjectPtr<ULevelStreamingDynamic> StreamingLevel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	TObjectPtr<AStage2TileMarker> TileMarker = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	EStage2TileType TileType = EStage2TileType::Straight;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	FTransform RequestedEntryTransform;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	FTransform AppliedLevelTransform;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	bool bInitialized = false;
};

UCLASS(Blueprintable)
class FPSPROJECT_API AStage2TileManager : public AActor
{
	GENERATED_BODY()

public:
	AStage2TileManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	void StartGeneration();

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	void SpawnNextTile();

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	void ClearGeneratedTiles();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Tiles")
	TArray<TSoftObjectPtr<UWorld>> StartTileLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Tiles")
	TArray<TSoftObjectPtr<UWorld>> StraightTileLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Tiles")
	TArray<TSoftObjectPtr<UWorld>> LeftTileLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Tiles")
	TArray<TSoftObjectPtr<UWorld>> RightTileLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Tiles")
	TArray<TSoftObjectPtr<UWorld>> GoalTileLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "1"))
	int32 InitialTilesToSpawn = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "2"))
	int32 MaxActiveTiles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "1"))
	int32 GoalAfterPlayableTileCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "1"))
	int32 MaxSameTurnStreak = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "0.0"))
	float StraightWeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "0.0"))
	float LeftWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules", meta = (ClampMin = "0.0"))
	float RightWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules")
	bool bKeepStartTileLoaded = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules")
	bool bUseDeterministicSeed = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Rules")
	int32 RandomSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Debug")
	bool bVerboseLog = true;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	TArray<FStage2LoadedTile> ActiveTiles;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	FTransform NextSpawnTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	int32 SpawnedPlayableTileCount = 0;

private:
	bool bGenerationStarted = false;
	bool bGoalTileSpawnRequested = false;
	bool bLoggedKeepStartConflict = false;
	int32 ConsecutiveLeftTurns = 0;
	int32 ConsecutiveRightTurns = 0;
	FRandomStream RandomStream;
	TMap<FSoftObjectPath, FTransform> CachedEntryLocalTransforms;

	bool TrySpawnTileLevel(const TSoftObjectPtr<UWorld>& TileLevel, EStage2TileType TileType, const FTransform& SpawnTransform);
	void TryFinalizeLoadedTiles();
	void FinalizeLoadedTile(int32 TileIndex);
	void UpdateNextSpawnTransformFromTile(const AStage2TileMarker* TileMarker);
	void TrimOldTiles();
	void ResetGenerationState();
	void UpdateTurnHistory(EStage2TileType TileType);
	EStage2TileType ChooseNextTileType();
	TSoftObjectPtr<UWorld> ChooseLevelForTileType(EStage2TileType TileType);
	TSoftObjectPtr<UWorld> ChooseRandomLevelFromArray(const TArray<TSoftObjectPtr<UWorld>>& LevelArray);
	AStage2TileMarker* FindTileMarkerFromStreamingLevel(ULevelStreamingDynamic* StreamingLevel) const;
	int32 GetInitializedTileCount() const;
	bool HasPendingUninitializedTile() const;

	UFUNCTION()
	void HandleTileTrigger(AStage2TileMarker* TileMarker, AActor* TriggeringActor);
};
