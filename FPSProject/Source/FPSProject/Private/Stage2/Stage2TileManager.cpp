#include "Stage2/Stage2TileManager.h"

#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Components/ModelComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelUtils.h"
#include "NavigationSystem.h"
#include "Zombie/BaseZombie.h"

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

	TryFinalizePooledTiles();
	TryFinalizeLoadedTiles();

	if (bGenerationStarted && IsTilePoolReady() && ActiveTiles.Num() == 0)
	{
		SpawnNextTile();
	}
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

	PreloadTilePool();
	SpawnNextTile();
}

void AStage2TileManager::SpawnNextTile()
{
	if (!bGenerationStarted)
	{
		return;
	}

	if (!IsTilePoolReady())
	{
		PreloadTilePool();
		return;
	}

	if (HasPendingUninitializedTile())
	{
		return;
	}

	const bool bNeedsStartTile = ActiveTiles.Num() == 0;

	if (!bNeedsStartTile && ActiveTiles.Num() >= MaxActiveTiles)
	{
		TrimOldTiles(FMath::Max(0, MaxActiveTiles - 1));
	}

	const EStage2TileType NextTileType = bNeedsStartTile ? EStage2TileType::Start : ChooseNextTileType();
	if (!TryActivatePooledTile(NextTileType, NextSpawnTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: No preloaded pooled tile is available for type %d"), static_cast<int32>(NextTileType));
	}
}

void AStage2TileManager::ClearGeneratedTiles()
{
	for (FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		UnloadTile(LoadedTile);
	}

	ActiveTiles.Empty();

	for (FStage2LoadedTile& PooledTile : TilePool)
	{
		UnloadTile(PooledTile);
	}

	TilePool.Empty();
	ResetGenerationState();
}

bool AStage2TileManager::HasInitializedTiles() const
{
	return GetInitializedTileCount() > 0;
}

bool AStage2TileManager::TryGetInitialPlayerSpawnTransform(FTransform& OutTransform) const
{
	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (!LoadedTile.bInitialized || !LoadedTile.TileMarker)
		{
			continue;
		}

		if (LoadedTile.TileType == EStage2TileType::Start)
		{
			OutTransform = LoadedTile.TileMarker->GetEntryTransform();
			return true;
		}
	}

	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (!LoadedTile.bInitialized || !LoadedTile.TileMarker)
		{
			continue;
		}

		OutTransform = LoadedTile.TileMarker->GetEntryTransform();
		return true;
	}

	return false;
}

bool AStage2TileManager::HasCompletedInitialGeneration() const
{
	return GetInitializedTileCount() >= InitialTilesToSpawn;
}

bool AStage2TileManager::AreInitialTilesReady() const
{
	return bInitialTilesReady;
}

void AStage2TileManager::PreloadTilePool()
{
	if (bTilePoolPreloadStarted)
	{
		return;
	}

	bTilePoolPreloadStarted = true;
	bTilePoolReady = false;
	ExpectedPooledTileCount = 0;
	NextPoolParkingIndex = 0;

	QueueTilePoolLevels(StartTileLevels, EStage2TileType::Start);
	QueueTilePoolLevels(StraightTileLevels, EStage2TileType::Straight);
	QueueTilePoolLevels(LeftTileLevels, EStage2TileType::Left);
	QueueTilePoolLevels(RightTileLevels, EStage2TileType::Right);
	QueueTilePoolLevels(GoalTileLevels, EStage2TileType::Goal);

	if (ExpectedPooledTileCount == 0)
	{
		bTilePoolReady = true;
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Tile pool has no configured level assets."));
	}
}

void AStage2TileManager::QueueTilePoolLevels(const TArray<TSoftObjectPtr<UWorld>>& LevelArray, EStage2TileType TileType)
{
	if (LevelArray.Num() == 0)
	{
		return;
	}

	const int32 PoolCount = FMath::Max(1, PreloadedTilesPerType);
	for (int32 PoolIndex = 0; PoolIndex < PoolCount; ++PoolIndex)
	{
		const TSoftObjectPtr<UWorld>& TileLevel = LevelArray[PoolIndex % LevelArray.Num()];
		if (TryLoadPooledTileLevel(TileLevel, TileType))
		{
			++ExpectedPooledTileCount;
		}
	}
}

bool AStage2TileManager::TryLoadPooledTileLevel(const TSoftObjectPtr<UWorld>& TileLevel, EStage2TileType TileType)
{
	if (TileLevel.IsNull())
	{
		return false;
	}

	const FTransform ParkingTransform = MakePoolParkingTransform();

	bool bLoadSucceeded = false;
	ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		TileLevel,
		ParkingTransform,
		bLoadSucceeded);

	if (!bLoadSucceeded || !StreamingLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Failed to preload pooled tile %s"),
			*TileLevel.ToSoftObjectPath().ToString());
		return false;
	}

	FStage2LoadedTile& PooledTile = TilePool.AddDefaulted_GetRef();
	PooledTile.SourceLevel = TileLevel;
	PooledTile.StreamingLevel = StreamingLevel;
	PooledTile.TileType = TileType;
	PooledTile.RequestedEntryTransform = ParkingTransform;
	PooledTile.AppliedLevelTransform = ParkingTransform;
	PooledTile.bInitialized = false;
	PooledTile.bActivatedFromPool = false;

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Requested pooled load for %s type %d at %s"),
			*TileLevel.ToSoftObjectPath().ToString(),
			static_cast<int32>(TileType),
			*ParkingTransform.GetLocation().ToString());
	}

	return true;
}

void AStage2TileManager::TryFinalizePooledTiles()
{
	if (!bTilePoolPreloadStarted || bTilePoolReady)
	{
		return;
	}

	for (int32 PoolIndex = 0; PoolIndex < TilePool.Num(); ++PoolIndex)
	{
		FStage2LoadedTile& PooledTile = TilePool[PoolIndex];
		if (PooledTile.bInitialized || !PooledTile.StreamingLevel)
		{
			continue;
		}

		if (!PooledTile.StreamingLevel->IsLevelLoaded() ||
			!PooledTile.StreamingLevel->IsLevelVisible())
		{
			continue;
		}

		AStage2TileMarker* TileMarker = FindTileMarkerFromStreamingLevel(PooledTile.StreamingLevel);
		if (!TileMarker)
		{
			continue;
		}

		PooledTile.TileMarker = TileMarker;
		FinalizePooledTile(PoolIndex);
	}

	bool bAllPooledTilesInitialized =
		ExpectedPooledTileCount > 0 &&
		TilePool.Num() >= ExpectedPooledTileCount;

	if (bAllPooledTilesInitialized)
	{
		for (const FStage2LoadedTile& PooledTile : TilePool)
		{
			if (!PooledTile.bInitialized)
			{
				bAllPooledTilesInitialized = false;
				break;
			}
		}
	}

	bTilePoolReady = bAllPooledTilesInitialized;
	if (bTilePoolReady && bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Preloaded %d pooled tile instance(s)."), TilePool.Num());
	}
}

void AStage2TileManager::FinalizePooledTile(int32 PoolIndex)
{
	if (!TilePool.IsValidIndex(PoolIndex))
	{
		return;
	}

	FStage2LoadedTile& PooledTile = TilePool[PoolIndex];
	if (PooledTile.bInitialized || !PooledTile.TileMarker)
	{
		return;
	}

	PooledTile.EntryLocalTransform = PooledTile.TileMarker->GetEntryTransform().GetRelativeTransform(PooledTile.AppliedLevelTransform);
	PooledTile.bHasEntryLocalTransform = true;

	const FSoftObjectPath SourceLevelPath = PooledTile.SourceLevel.ToSoftObjectPath();
	if (SourceLevelPath.IsValid())
	{
		CachedEntryLocalTransforms.Add(SourceLevelPath, PooledTile.EntryLocalTransform);
	}

	PooledTile.TileMarker->OnNextTileTriggerEntered.RemoveAll(this);
	PooledTile.TileMarker->ResetNextTileTrigger();
	PooledTile.TileMarker->SetNextTileTriggerEnabled(false);
	PooledTile.bInitialized = true;
}

bool AStage2TileManager::IsTilePoolReady() const
{
	return bTilePoolReady;
}

bool AStage2TileManager::IsPoolTileAvailable(EStage2TileType TileType) const
{
	for (const FStage2LoadedTile& PooledTile : TilePool)
	{
		if (PooledTile.bInitialized && PooledTile.TileType == TileType)
		{
			return true;
		}
	}

	return false;
}

bool AStage2TileManager::TryActivatePooledTile(EStage2TileType TileType, const FTransform& EntryTransform)
{
	TArray<int32> CandidatePoolIndexes;
	for (int32 PoolIndex = 0; PoolIndex < TilePool.Num(); ++PoolIndex)
	{
		const FStage2LoadedTile& PooledTile = TilePool[PoolIndex];
		if (PooledTile.bInitialized && PooledTile.TileType == TileType && PooledTile.TileMarker)
		{
			CandidatePoolIndexes.Add(PoolIndex);
		}
	}

	if (CandidatePoolIndexes.Num() == 0)
	{
		return false;
	}

	const int32 CandidateIndex = RandomStream.RandRange(0, CandidatePoolIndexes.Num() - 1);
	const int32 PoolIndex = CandidatePoolIndexes[CandidateIndex];

	FStage2LoadedTile ActivatedTile = TilePool[PoolIndex];
	TilePool.RemoveAt(PoolIndex);

	const FTransform EntryLocalTransform = ActivatedTile.bHasEntryLocalTransform
		? ActivatedTile.EntryLocalTransform
		: ActivatedTile.TileMarker->GetEntryTransform().GetRelativeTransform(ActivatedTile.AppliedLevelTransform);
	const FTransform LevelTransformToApply = EntryLocalTransform.Inverse() * EntryTransform;

	if (!MoveLoadedTileToLevelTransform(ActivatedTile, LevelTransformToApply))
	{
		TilePool.Add(ActivatedTile);
		return false;
	}

	ActivatedTile.RequestedEntryTransform = EntryTransform;
	ActivatedTile.TileType = TileType;
	ActivatedTile.bInitialized = false;
	ActivatedTile.bActivatedFromPool = true;

	const int32 ActiveTileIndex = ActiveTiles.Add(ActivatedTile);
	FinalizeLoadedTile(ActiveTileIndex);
	return true;
}

bool AStage2TileManager::MoveLoadedTileToLevelTransform(FStage2LoadedTile& LoadedTile, const FTransform& NewLevelTransform) const
{
	if (!LoadedTile.StreamingLevel)
	{
		return false;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return false;
	}

	const FTransform OldLevelTransform = LoadedTile.AppliedLevelTransform;
	if (!OldLevelTransform.Equals(NewLevelTransform))
	{
		const FTransform DeltaTransform = OldLevelTransform.Inverse() * NewLevelTransform;
		FLevelUtils::FApplyLevelTransformParams TransformParams(LoadedLevel, DeltaTransform);
		TransformParams.bSetRelativeTransformDirectly = true;

#if WITH_EDITOR
		TransformParams.bDoPostEditMove = false;
#endif

		FLevelUtils::ApplyLevelTransform(TransformParams);

		for (UModelComponent* ModelComponent : LoadedLevel->ModelComponents)
		{
			if (ModelComponent)
			{
				ModelComponent->UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate, ETeleportType::TeleportPhysics);
			}
		}

		for (AActor* LevelActor : LoadedLevel->Actors)
		{
			if (!IsValid(LevelActor))
			{
				continue;
			}

			if (USceneComponent* ActorRootComponent = LevelActor->GetRootComponent())
			{
				ActorRootComponent->UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate, ETeleportType::TeleportPhysics);
			}
		}
	}

	LoadedTile.StreamingLevel->LevelTransform = NewLevelTransform;
	LoadedTile.AppliedLevelTransform = NewLevelTransform;
	return true;
}

FTransform AStage2TileManager::MakePoolParkingTransform()
{
	const FVector ParkingLocation =
		GetActorLocation() +
		PoolParkingOffset +
		FVector(NextPoolParkingIndex * PoolParkingSpacing, 0.0f, 0.0f);

	++NextPoolParkingIndex;
	return FTransform(GetActorRotation(), ParkingLocation, GetActorScale3D());
}

bool AStage2TileManager::TrySpawnTileLevel(const TSoftObjectPtr<UWorld>& TileLevel, EStage2TileType TileType, const FTransform& SpawnTransform)
{
	if (TileLevel.IsNull())
	{
		return false;
	}

	FTransform LevelTransformToApply = SpawnTransform;
	const FSoftObjectPath LevelPath = TileLevel.ToSoftObjectPath();
	if (const FTransform* CachedEntryLocalTransform = CachedEntryLocalTransforms.Find(LevelPath))
	{
		LevelTransformToApply = CachedEntryLocalTransform->Inverse() * SpawnTransform;
	}

	bool bLoadSucceeded = false;
	ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		TileLevel,
		LevelTransformToApply,
		bLoadSucceeded);

	if (!bLoadSucceeded || !StreamingLevel)
	{
		return false;
	}

	FStage2LoadedTile& LoadedTile = ActiveTiles.AddDefaulted_GetRef();
	LoadedTile.SourceLevel = TileLevel;
	LoadedTile.StreamingLevel = StreamingLevel;
	LoadedTile.TileType = TileType;
	LoadedTile.RequestedEntryTransform = SpawnTransform;
	LoadedTile.AppliedLevelTransform = LevelTransformToApply;
	LoadedTile.bInitialized = false;
	LoadedTile.bActivatedFromPool = false;

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Requested load for %s | Requested=%s | Applied=%s"),
			*TileLevel.ToSoftObjectPath().ToString(),
			*SpawnTransform.GetLocation().ToString(),
			*LevelTransformToApply.GetLocation().ToString());
	}

	return true;
}

void AStage2TileManager::TryFinalizeLoadedTiles()
{
	for (int32 TileIndex = 0; TileIndex < ActiveTiles.Num(); ++TileIndex)
	{
		FStage2LoadedTile& LoadedTile = ActiveTiles[TileIndex];

		if (LoadedTile.bInitialized || !LoadedTile.StreamingLevel)
		{
			continue;
		}

		if (!LoadedTile.StreamingLevel->IsLevelLoaded() ||
			!LoadedTile.StreamingLevel->IsLevelVisible())
		{
			continue;
		}

		AStage2TileMarker* TileMarker = FindTileMarkerFromStreamingLevel(LoadedTile.StreamingLevel);
		if (!TileMarker)
		{
			continue;
		}

		LoadedTile.TileMarker = TileMarker;
		FinalizeLoadedTile(TileIndex);
		return;
	}
}

void AStage2TileManager::FinalizeLoadedTile(int32 TileIndex)
{
	if (!ActiveTiles.IsValidIndex(TileIndex))
	{
		return;
	}

	FStage2LoadedTile& LoadedTile = ActiveTiles[TileIndex];
	if (LoadedTile.bInitialized || !LoadedTile.TileMarker)
	{
		return;
	}

	AStage2TileMarker* TileMarker = LoadedTile.TileMarker;
	const EStage2TileType TileType = LoadedTile.TileType;
	const FSoftObjectPath SourceLevelPath = LoadedTile.SourceLevel.ToSoftObjectPath();

	if (SourceLevelPath.IsValid())
	{
		LoadedTile.EntryLocalTransform = TileMarker->GetEntryTransform().GetRelativeTransform(LoadedTile.AppliedLevelTransform);
		LoadedTile.bHasEntryLocalTransform = true;
		CachedEntryLocalTransforms.Add(SourceLevelPath, LoadedTile.EntryLocalTransform);
	}

	TileMarker->ResetNextTileTrigger();
	TileMarker->SetNextTileTriggerEnabled(TileType != EStage2TileType::Goal);
	TileMarker->OnNextTileTriggerEntered.AddUniqueDynamic(this, &AStage2TileManager::HandleTileTrigger);

	UpdateNextSpawnTransformFromTile(TileMarker);
	LoadedTile.bInitialized = true;

	if (TileType == EStage2TileType::Straight ||
		TileType == EStage2TileType::Left ||
		TileType == EStage2TileType::Right)
	{
		++SpawnedPlayableTileCount;
		UpdateTurnHistory(TileType);
	}

	if (TileType == EStage2TileType::Goal)
	{
		bGoalTileSpawnRequested = true;
	}

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Finalized tile %s (%d)"),
			*GetNameSafe(TileMarker),
			static_cast<int32>(TileType));
	}

	SpawnZombiesForTile(LoadedTile);
	TrimOldTiles();
	RefreshNavigationForStreamingTile(LoadedTile);
	MarkInitialTilesReadyIfNeeded();

	if (GetInitializedTileCount() < InitialTilesToSpawn && TileType != EStage2TileType::Goal)
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

	const FTransform ExitTransform = TileMarker->GetExitTransform();
	NextSpawnTransform = TileMarker->GetNextTileSpawnTransform();

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Next spawn transform from %s | Entry=%s Exit=%s Spawn=%s"),
			*GetNameSafe(TileMarker),
			*TileMarker->GetEntryTransform().GetLocation().ToString(),
			*ExitTransform.GetLocation().ToString(),
			*NextSpawnTransform.GetLocation().ToString());
	}
}

void AStage2TileManager::TrimOldTiles(int32 DesiredMaxActiveTiles)
{
	const int32 EffectiveMaxActiveTiles = DesiredMaxActiveTiles == INDEX_NONE
		? MaxActiveTiles
		: DesiredMaxActiveTiles;

	while (ActiveTiles.Num() > EffectiveMaxActiveTiles)
	{
		int32 RemoveIndex = 0;
		if (bKeepStartTileLoaded &&
			ActiveTiles.IsValidIndex(0) &&
			ActiveTiles[0].TileType == EStage2TileType::Start &&
			ActiveTiles.Num() > 1)
		{
			if (MaxActiveTiles >= 3)
			{
				RemoveIndex = 1;
			}
			else if (!bLoggedKeepStartConflict)
			{
				bLoggedKeepStartConflict = true;
				UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: bKeepStartTileLoaded is enabled but MaxActiveTiles is %d. The start tile will be recycled first to avoid removing the bridge tile the truck is currently using."), MaxActiveTiles);
			}
		}

		RecycleActiveTileAt(RemoveIndex);
	}
}

void AStage2TileManager::RecycleActiveTileAt(int32 TileIndex)
{
	if (!ActiveTiles.IsValidIndex(TileIndex))
	{
		return;
	}

	FStage2LoadedTile RecycledTile = ActiveTiles[TileIndex];
	ActiveTiles.RemoveAt(TileIndex);

	DestroySpawnedZombiesForTile(RecycledTile);

	if (RecycledTile.TileMarker)
	{
		RecycledTile.TileMarker->OnNextTileTriggerEntered.RemoveAll(this);
		RecycledTile.TileMarker->ResetNextTileTrigger();
		RecycledTile.TileMarker->SetNextTileTriggerEnabled(false);
	}

	MoveLoadedTileToLevelTransform(RecycledTile, MakePoolParkingTransform());
	RecycledTile.RequestedEntryTransform = RecycledTile.AppliedLevelTransform;
	RecycledTile.bInitialized = true;
	RecycledTile.bActivatedFromPool = false;
	TilePool.Add(RecycledTile);
}

void AStage2TileManager::UnloadTile(FStage2LoadedTile& LoadedTile)
{
	DestroySpawnedZombiesForTile(LoadedTile);

	if (LoadedTile.TileMarker)
	{
		LoadedTile.TileMarker->OnNextTileTriggerEntered.RemoveAll(this);
		LoadedTile.TileMarker->SetNextTileTriggerEnabled(false);
	}

	if (LoadedTile.StreamingLevel)
	{
		LoadedTile.StreamingLevel->SetShouldBeLoaded(false);
		LoadedTile.StreamingLevel->SetShouldBeVisible(false);
		LoadedTile.StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
	}
}

void AStage2TileManager::ResetGenerationState()
{
	bGenerationStarted = false;
	bGoalTileSpawnRequested = false;
	bInitialTilesReady = false;
	bLoggedKeepStartConflict = false;
	bTilePoolPreloadStarted = false;
	bTilePoolReady = false;
	ConsecutiveLeftTurns = 0;
	ConsecutiveRightTurns = 0;
	ExpectedPooledTileCount = 0;
	NextPoolParkingIndex = 0;
	SpawnedPlayableTileCount = 0;
	NextSpawnTransform = GetActorTransform();
}

void AStage2TileManager::SpawnZombiesForTile(FStage2LoadedTile& LoadedTile)
{
	if (!HasAuthority() || !LoadedTile.TileMarker || ZombieClasses.Num() == 0 || !GetWorld())
	{
		return;
	}

	const bool bIsPlayableTile =
		LoadedTile.TileType == EStage2TileType::Straight ||
		LoadedTile.TileType == EStage2TileType::Left ||
		LoadedTile.TileType == EStage2TileType::Right;

	if (!bIsPlayableTile &&
		!(bSpawnZombiesOnStartTile && LoadedTile.TileType == EStage2TileType::Start) &&
		!(bSpawnZombiesOnGoalTile && LoadedTile.TileType == EStage2TileType::Goal))
	{
		return;
	}

	if (ZombieSpawnChancePerTile < 1.0f && RandomStream.FRand() > ZombieSpawnChancePerTile)
	{
		return;
	}

	TArray<FTransform> CandidateSpawnTransforms = LoadedTile.TileMarker->GetZombieSpawnTransforms(false);
	if (CandidateSpawnTransforms.Num() == 0)
	{
		if (bVerboseLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Tile %s has no zombie spawn points under ZombieSpawnRoot."),
				*GetNameSafe(LoadedTile.TileMarker));
		}
		return;
	}

	const int32 EffectiveMinSpawnCount = FMath::Max(0, MinZombiesPerPlayableTile);
	const int32 EffectiveMaxSpawnCount = FMath::Max(EffectiveMinSpawnCount, MaxZombiesPerPlayableTile);
	const int32 DesiredSpawnCount = FMath::Min(
		CandidateSpawnTransforms.Num(),
		RandomStream.RandRange(EffectiveMinSpawnCount, EffectiveMaxSpawnCount));

	TArray<TEnumAsByte<EObjectTypeQuery>> OverlapObjectTypes;
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	for (int32 SpawnIteration = 0; SpawnIteration < DesiredSpawnCount && CandidateSpawnTransforms.Num() > 0; ++SpawnIteration)
	{
		const int32 SpawnTransformIndex = RandomStream.RandRange(0, CandidateSpawnTransforms.Num() - 1);
		const FTransform SpawnTransform = CandidateSpawnTransforms[SpawnTransformIndex];
		CandidateSpawnTransforms.RemoveAtSwap(SpawnTransformIndex);

		TArray<AActor*> BlockingActors;
		if (ZombieSpawnCollisionRadius > 0.0f &&
			UKismetSystemLibrary::SphereOverlapActors(
				GetWorld(),
				SpawnTransform.GetLocation(),
				ZombieSpawnCollisionRadius,
				OverlapObjectTypes,
				AActor::StaticClass(),
				TArray<AActor*>(),
				BlockingActors))
		{
			continue;
		}

		const int32 ZombieClassIndex = RandomStream.RandRange(0, ZombieClasses.Num() - 1);
		TSubclassOf<ABaseZombie> ZombieClass = ZombieClasses[ZombieClassIndex];
		if (!ZombieClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (ABaseZombie* SpawnedZombie = GetWorld()->SpawnActor<ABaseZombie>(ZombieClass, SpawnTransform, SpawnParameters))
		{
			LoadedTile.SpawnedZombies.Add(SpawnedZombie);
		}
	}

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Spawned %d zombies for tile %s."),
			LoadedTile.SpawnedZombies.Num(),
			*GetNameSafe(LoadedTile.TileMarker));
	}
}

void AStage2TileManager::DestroySpawnedZombiesForTile(FStage2LoadedTile& LoadedTile)
{
	if (!HasAuthority())
	{
		return;
	}

	for (ABaseZombie* SpawnedZombie : LoadedTile.SpawnedZombies)
	{
		if (IsValid(SpawnedZombie))
		{
			SpawnedZombie->Destroy();
		}
	}

	LoadedTile.SpawnedZombies.Empty();
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
		SpawnedPlayableTileCount >= GoalAfterPlayableTileCount &&
		IsPoolTileAvailable(EStage2TileType::Goal))
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
		if (IsPoolTileAvailable(EStage2TileType::Straight))
		{
			WeightedCandidates.Add({ EStage2TileType::Straight, StraightWeight });
		}
	}

	if (LeftTileLevels.Num() > 0 &&
		LeftWeight > 0.0f &&
		ConsecutiveLeftTurns < MaxSameTurnStreak &&
		IsPoolTileAvailable(EStage2TileType::Left))
	{
		WeightedCandidates.Add({ EStage2TileType::Left, LeftWeight });
	}

	if (RightTileLevels.Num() > 0 &&
		RightWeight > 0.0f &&
		ConsecutiveRightTurns < MaxSameTurnStreak &&
		IsPoolTileAvailable(EStage2TileType::Right))
	{
		WeightedCandidates.Add({ EStage2TileType::Right, RightWeight });
	}

	if (WeightedCandidates.Num() == 0)
	{
		if (IsPoolTileAvailable(EStage2TileType::Straight))
		{
			return EStage2TileType::Straight;
		}

		if (IsPoolTileAvailable(EStage2TileType::Left))
		{
			return EStage2TileType::Left;
		}

		if (IsPoolTileAvailable(EStage2TileType::Right))
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

void AStage2TileManager::RefreshNavigationForStreamingTile(const FStage2LoadedTile& LoadedTile) const
{
	if (!bRebuildNavigationAfterTileLoad)
	{
		return;
	}

	if (LoadedTile.bActivatedFromPool && !bRebuildNavigationWhenActivatingPooledTile)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavigationSystem)
	{
		return;
	}

	NavigationSystem->Build();

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Requested navigation rebuild after loading %s"),
			*LoadedTile.SourceLevel.ToSoftObjectPath().ToString());
	}
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

void AStage2TileManager::MarkInitialTilesReadyIfNeeded()
{
	if (bInitialTilesReady)
	{
		return;
	}

	const int32 RequiredInitialTileCount = FMath::Max(1, InitialTilesToSpawn);
	if (GetInitializedTileCount() < RequiredInitialTileCount || HasPendingUninitializedTile())
	{
		return;
	}

	bInitialTilesReady = true;

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Initial %d tile(s) are ready."), RequiredInitialTileCount);
	}

	OnInitialTilesReady.Broadcast();
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
