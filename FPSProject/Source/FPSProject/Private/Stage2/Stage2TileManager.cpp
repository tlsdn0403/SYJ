#include "Stage2/Stage2TileManager.h"

#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"

AStage2TileManager::AStage2TileManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	NextSpawnTransform = GetActorTransform();
}

void AStage2TileManager::BeginPlay()
{
	Super::BeginPlay();

	NextSpawnTransform = GetActorTransform();

	if (bSpawnOnBeginPlay)
	{
		StartGeneration();
	}
}

void AStage2TileManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TryFinalizeLoadedTiles();
}

void AStage2TileManager::StartGeneration()
{
	if (bGenerationStarted)
	{
		return;
	}

	ResetGenerationState();
	bGenerationStarted = true;
	NextSpawnTransform = GetActorTransform();

	if (bUseDeterministicSeed)
	{
		RandomStream.Initialize(RandomSeed);
	}
	else
	{
		RandomStream.GenerateNewSeed();
	}

	SpawnNextTile();
}

void AStage2TileManager::SpawnNextTile()
{
	if (!bGenerationStarted)
	{
		return;
	}

	if (HasPendingUninitializedTile())
	{
		return;
	}

	const bool bNeedsStartTile = ActiveTiles.Num() == 0;
	const EStage2TileType NextTileType = bNeedsStartTile ? EStage2TileType::Start : ChooseNextTileType();
	const TSoftObjectPtr<UWorld> LevelToSpawn = ChooseLevelForTileType(NextTileType);
	if (LevelToSpawn.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: No level asset configured for tile type %d"), static_cast<int32>(NextTileType));
		return;
	}

	if (!TrySpawnTileLevel(LevelToSpawn, NextTileType, NextSpawnTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Failed to spawn tile level for type %d"), static_cast<int32>(NextTileType));
	}
}

void AStage2TileManager::ClearGeneratedTiles()
{
	for (FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (LoadedTile.TileMarker)
		{
			LoadedTile.TileMarker->OnNextTileTriggerEntered.RemoveAll(this);
		}

		if (LoadedTile.StreamingLevel)
		{
			LoadedTile.StreamingLevel->SetShouldBeLoaded(false);
			LoadedTile.StreamingLevel->SetShouldBeVisible(false);
			LoadedTile.StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
		}
	}

	ActiveTiles.Empty();
	ResetGenerationState();
}

bool AStage2TileManager::TrySpawnTileLevel(const TSoftObjectPtr<UWorld>& TileLevel, EStage2TileType TileType, const FTransform& SpawnTransform)
{
	if (TileLevel.IsNull())
	{
		return false;
	}

	bool bLoadSucceeded = false;
	ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		TileLevel,
		SpawnTransform.GetLocation(),
		SpawnTransform.Rotator(),
		bLoadSucceeded);

	if (!bLoadSucceeded || !StreamingLevel)
	{
		return false;
	}

	FStage2LoadedTile& LoadedTile = ActiveTiles.AddDefaulted_GetRef();
	LoadedTile.StreamingLevel = StreamingLevel;
	LoadedTile.TileType = TileType;
	LoadedTile.bInitialized = false;

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Requested load for %s at %s"),
			*TileLevel.ToSoftObjectPath().ToString(),
			*SpawnTransform.GetLocation().ToString());
	}

	return true;
}

void AStage2TileManager::TryFinalizeLoadedTiles()
{
	for (FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (LoadedTile.bInitialized || !LoadedTile.StreamingLevel)
		{
			continue;
		}

		if (!LoadedTile.StreamingLevel->IsLevelLoaded())
		{
			continue;
		}

		AStage2TileMarker* TileMarker = FindTileMarkerFromStreamingLevel(LoadedTile.StreamingLevel);
		if (!TileMarker)
		{
			continue;
		}

		LoadedTile.TileMarker = TileMarker;
		FinalizeLoadedTile(LoadedTile);
	}
}

void AStage2TileManager::FinalizeLoadedTile(FStage2LoadedTile& LoadedTile)
{
	if (LoadedTile.bInitialized || !LoadedTile.TileMarker)
	{
		return;
	}

	LoadedTile.TileMarker->ResetNextTileTrigger();
	LoadedTile.TileMarker->SetNextTileTriggerEnabled(LoadedTile.TileType != EStage2TileType::Goal);
	LoadedTile.TileMarker->OnNextTileTriggerEntered.AddUniqueDynamic(this, &AStage2TileManager::HandleTileTrigger);

	UpdateNextSpawnTransformFromTile(LoadedTile.TileMarker);
	LoadedTile.bInitialized = true;

	if (LoadedTile.TileType == EStage2TileType::Straight ||
		LoadedTile.TileType == EStage2TileType::Left ||
		LoadedTile.TileType == EStage2TileType::Right)
	{
		++SpawnedPlayableTileCount;
		UpdateTurnHistory(LoadedTile.TileType);
	}

	if (LoadedTile.TileType == EStage2TileType::Goal)
	{
		bGoalTileSpawnRequested = true;
	}

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Finalized tile %s (%d)"),
			*GetNameSafe(LoadedTile.TileMarker),
			static_cast<int32>(LoadedTile.TileType));
	}

	TrimOldTiles();

	if (GetInitializedTileCount() < InitialTilesToSpawn && LoadedTile.TileType != EStage2TileType::Goal)
	{
		SpawnNextTile();
	}
}

void AStage2TileManager::UpdateNextSpawnTransformFromTile(const AStage2TileMarker* TileMarker)
{
	if (!TileMarker)
	{
		return;
	}

	NextSpawnTransform = TileMarker->GetExitTransform();
}

void AStage2TileManager::TrimOldTiles()
{
	while (ActiveTiles.Num() > MaxActiveTiles)
	{
		int32 RemoveIndex = 0;
		if (bKeepStartTileLoaded &&
			ActiveTiles.IsValidIndex(0) &&
			ActiveTiles[0].TileType == EStage2TileType::Start &&
			ActiveTiles.Num() > 1)
		{
			RemoveIndex = 1;
		}

		FStage2LoadedTile RemovedTile = ActiveTiles[RemoveIndex];
		ActiveTiles.RemoveAt(RemoveIndex);

		if (RemovedTile.TileMarker)
		{
			RemovedTile.TileMarker->OnNextTileTriggerEntered.RemoveAll(this);
		}

		if (RemovedTile.StreamingLevel)
		{
			RemovedTile.StreamingLevel->SetShouldBeLoaded(false);
			RemovedTile.StreamingLevel->SetShouldBeVisible(false);
			RemovedTile.StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
		}
	}
}

void AStage2TileManager::ResetGenerationState()
{
	bGenerationStarted = false;
	bGoalTileSpawnRequested = false;
	ConsecutiveLeftTurns = 0;
	ConsecutiveRightTurns = 0;
	SpawnedPlayableTileCount = 0;
	NextSpawnTransform = GetActorTransform();
}

void AStage2TileManager::UpdateTurnHistory(EStage2TileType TileType)
{
	switch (TileType)
	{
	case EStage2TileType::Left:
		++ConsecutiveLeftTurns;
		ConsecutiveRightTurns = 0;
		break;
	case EStage2TileType::Right:
		++ConsecutiveRightTurns;
		ConsecutiveLeftTurns = 0;
		break;
	default:
		ConsecutiveLeftTurns = 0;
		ConsecutiveRightTurns = 0;
		break;
	}
}

EStage2TileType AStage2TileManager::ChooseNextTileType()
{
	if (!bGoalTileSpawnRequested &&
		GoalTileLevels.Num() > 0 &&
		SpawnedPlayableTileCount >= GoalAfterPlayableTileCount)
	{
		return EStage2TileType::Goal;
	}

	struct FWeightedTileType
	{
		EStage2TileType TileType;
		float Weight;
	};

	TArray<FWeightedTileType> WeightedCandidates;

	if (StraightTileLevels.Num() > 0 && StraightWeight > 0.0f)
	{
		WeightedCandidates.Add({ EStage2TileType::Straight, StraightWeight });
	}

	if (LeftTileLevels.Num() > 0 &&
		LeftWeight > 0.0f &&
		ConsecutiveLeftTurns < MaxSameTurnStreak)
	{
		WeightedCandidates.Add({ EStage2TileType::Left, LeftWeight });
	}

	if (RightTileLevels.Num() > 0 &&
		RightWeight > 0.0f &&
		ConsecutiveRightTurns < MaxSameTurnStreak)
	{
		WeightedCandidates.Add({ EStage2TileType::Right, RightWeight });
	}

	if (WeightedCandidates.Num() == 0)
	{
		if (StraightTileLevels.Num() > 0)
		{
			return EStage2TileType::Straight;
		}

		if (LeftTileLevels.Num() > 0)
		{
			return EStage2TileType::Left;
		}

		if (RightTileLevels.Num() > 0)
		{
			return EStage2TileType::Right;
		}

		return EStage2TileType::Goal;
	}

	float TotalWeight = 0.0f;
	for (const FWeightedTileType& Candidate : WeightedCandidates)
	{
		TotalWeight += Candidate.Weight;
	}

	float PickedWeight = RandomStream.FRandRange(0.0f, TotalWeight);
	for (const FWeightedTileType& Candidate : WeightedCandidates)
	{
		PickedWeight -= Candidate.Weight;
		if (PickedWeight <= 0.0f)
		{
			return Candidate.TileType;
		}
	}

	return WeightedCandidates.Last().TileType;
}

TSoftObjectPtr<UWorld> AStage2TileManager::ChooseLevelForTileType(EStage2TileType TileType)
{
	switch (TileType)
	{
	case EStage2TileType::Start:
		return ChooseRandomLevelFromArray(StartTileLevels);
	case EStage2TileType::Straight:
		return ChooseRandomLevelFromArray(StraightTileLevels);
	case EStage2TileType::Left:
		return ChooseRandomLevelFromArray(LeftTileLevels);
	case EStage2TileType::Right:
		return ChooseRandomLevelFromArray(RightTileLevels);
	case EStage2TileType::Goal:
		return ChooseRandomLevelFromArray(GoalTileLevels);
	default:
		return nullptr;
	}
}

TSoftObjectPtr<UWorld> AStage2TileManager::ChooseRandomLevelFromArray(const TArray<TSoftObjectPtr<UWorld>>& LevelArray)
{
	if (LevelArray.Num() == 0)
	{
		return nullptr;
	}

	const int32 Index = RandomStream.RandRange(0, LevelArray.Num() - 1);
	return LevelArray[Index];
}

AStage2TileMarker* AStage2TileManager::FindTileMarkerFromStreamingLevel(ULevelStreamingDynamic* StreamingLevel) const
{
	if (!StreamingLevel)
	{
		return nullptr;
	}

	ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return nullptr;
	}

	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (AStage2TileMarker* TileMarker = Cast<AStage2TileMarker>(LevelActor))
		{
			return TileMarker;
		}
	}

	return nullptr;
}

int32 AStage2TileManager::GetInitializedTileCount() const
{
	int32 InitializedCount = 0;
	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (LoadedTile.bInitialized)
		{
			++InitializedCount;
		}
	}

	return InitializedCount;
}

bool AStage2TileManager::HasPendingUninitializedTile() const
{
	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (!LoadedTile.bInitialized)
		{
			return true;
		}
	}

	return false;
}

void AStage2TileManager::HandleTileTrigger(AStage2TileMarker* TileMarker, AActor* TriggeringActor)
{
	if (!TileMarker)
	{
		return;
	}

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Tile trigger fired by %s on %s"),
			*GetNameSafe(TriggeringActor),
			*GetNameSafe(TileMarker));
	}

	SpawnNextTile();
}
