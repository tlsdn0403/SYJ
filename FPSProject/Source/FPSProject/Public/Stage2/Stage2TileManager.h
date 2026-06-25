#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Stage2/Stage2TileMarker.h"
#include "UObject/ObjectKey.h"
#include "Stage2TileManager.generated.h"

class ULevelStreamingDynamic;
class UWorld;
class ABaseZombie;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStage2InitialTilesReadySignature);

USTRUCT(BlueprintType)
struct FStage2LoadedTile
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	TSoftObjectPtr<UWorld> SourceLevel = nullptr;

	//동적으로 로드된 타일 레벨 인스턴스 포인터
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	TObjectPtr<ULevelStreamingDynamic> StreamingLevel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	TObjectPtr<AStage2TileMarker> TileMarker = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	EStage2TileType TileType = EStage2TileType::Straight;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	int32 TileOccurrenceIndex = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	FTransform RequestedEntryTransform;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	FTransform AppliedLevelTransform;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	FTransform EntryLocalTransform;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	bool bHasEntryLocalTransform = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2")
	bool bInitialized = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage2|Zombie")
	TArray<TObjectPtr<ABaseZombie>> SpawnedZombies;
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

	UFUNCTION(BlueprintPure, Category = "Stage2")
	bool HasInitializedTiles() const;

	UFUNCTION(BlueprintPure, Category = "Stage2")
	bool TryGetInitialPlayerSpawnTransform(FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "Stage2")
	bool AreInitialTilesReady() const;

	bool TryBuildWorldTransformForTileLocalPoint(
		EStage2TileType TileType,
		const FVector& LocalLocation,
		float LocalYaw,
		int32 TileOccurrenceIndex,
		FTransform& OutTransform) const;

	UPROPERTY(BlueprintAssignable, Category = "Stage2")
	FStage2InitialTilesReadySignature OnInitialTilesReady;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Pool", meta = (ClampMin = "1"))
	int32 PreloadedTilesPerType = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Pool")
	FVector PoolParkingOffset = FVector(0.0f, 0.0f, -1000000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Pool", meta = (ClampMin = "1000.0"))
	float PoolParkingSpacing = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Pool")
	bool bHidePooledTiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Pool")
	bool bDisablePooledTileCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Performance")
	bool bApplyTileCullDistances = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Performance", meta = (ClampMin = "0.0", EditCondition = "bApplyTileCullDistances"))
	float TilePrimitiveCullDistance = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Performance", meta = (ClampMin = "0.0", EditCondition = "bApplyTileCullDistances"))
	float TileSmallPrimitiveCullDistance = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Performance", meta = (ClampMin = "0.0", EditCondition = "bApplyTileCullDistances"))
	float TileSmallPrimitiveBoundsRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Performance")
	bool bDisableSmallTilePrimitiveShadows = true;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie")
	TArray<TSubclassOf<ABaseZombie>> ZombieClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie", meta = (ClampMin = "0"))
	int32 MinZombiesPerPlayableTile = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie", meta = (ClampMin = "0"))
	int32 MaxZombiesPerPlayableTile = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ZombieSpawnChancePerTile = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie")
	bool bSpawnZombiesOnStartTile = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie")
	bool bSpawnZombiesOnGoalTile = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Zombie", meta = (ClampMin = "0.0"))
	float ZombieSpawnCollisionRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2|Debug")
	bool bVerboseLog = true;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	TArray<FStage2LoadedTile> ActiveTiles;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	TArray<FStage2LoadedTile> TilePool;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	FTransform NextSpawnTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Stage2")
	int32 SpawnedPlayableTileCount = 0;

private:
	bool bGenerationStarted = false;
	bool bGoalTileSpawnRequested = false;
	bool bInitialTilesReady = false;
	bool bTilePoolPreloadStarted = false;
	bool bTilePoolReady = false;
	int32 ConsecutiveLeftTurns = 0;
	int32 ConsecutiveRightTurns = 0;
	int32 NextRightTileOccurrenceIndex = 0;
	int32 NextPoolParkingIndex = 0;
	FRandomStream RandomStream;
	TMap<TObjectKey<UPrimitiveComponent>, ECollisionEnabled::Type> CachedTileCollisionStates;

	void PreloadTilePool();
	void QueueTilePoolLevels(const TArray<TSoftObjectPtr<UWorld>>& LevelArray, EStage2TileType TileType);
	void LoadPooledTileLevel(const TSoftObjectPtr<UWorld>& TileLevel, EStage2TileType TileType);
	void TryFinalizePooledTiles();
	void FinalizePooledTile(int32 PoolIndex);
	bool IsTilePoolReady() const;
	bool IsPoolTileAvailable(EStage2TileType TileType) const;
	bool TryActivatePooledTile(EStage2TileType TileType, const FTransform& EntryTransform);
	bool TryMoveTileTolocation(FStage2LoadedTile& LoadedTile, const FTransform& NewLevelTransform);
	void SetTileRenderingEnabled(const FStage2LoadedTile& LoadedTile, bool bEnabled) const;
	void SetTileCollisionEnabled(const FStage2LoadedTile& LoadedTile, bool bEnabled);
	void ApplyTilePerformanceSettings(const FStage2LoadedTile& LoadedTile) const;
	void ApplyPrimitivePerformanceSettings(UPrimitiveComponent* PrimitiveComponent) const;
	void RefreshTilePhysicsState(const FStage2LoadedTile& LoadedTile) const;
	void ForgetTileCollisionStates(const FStage2LoadedTile& LoadedTile);
	FTransform MakePoolParkingTransform();
	void FinalizeLoadedTile(int32 TileIndex);
	void UpdateNextSpawnTransformFromTile(const AStage2TileMarker* TileMarker);
	void TrimOldTiles(int32 DesiredMaxActiveTiles = INDEX_NONE);
	void RecycleActiveTileAt(int32 TileIndex);
	void UnloadTile(FStage2LoadedTile& LoadedTile);
	void ResetGenerationState();
	void SpawnZombiesForTile(FStage2LoadedTile& LoadedTile);
	void DestroySpawnedZombiesForTile(FStage2LoadedTile& LoadedTile);
	void UpdateTurnHistory(EStage2TileType TileType);
	EStage2TileType ChooseNextTileType();
	AStage2TileMarker* FindTileMarkerFromStreamingLevel(ULevelStreamingDynamic* StreamingLevel) const;
	int32 GetInitializedTileCount() const;
	void MarkInitialTilesReadyIfNeeded();

	UFUNCTION()
	void HandleTileTrigger(AStage2TileMarker* TileMarker, AActor* TriggeringActor);
};
