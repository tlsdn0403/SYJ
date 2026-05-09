#include "Stage2/Stage2TileManager.h"

#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "NavigationSystem.h"

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
	// 첫 타일 생성 위치를 Manger의 위치로
	NextSpawnTransform = GetActorTransform();

	// Deterministic Seed 사용: 매번 같은 타일 순서 생성
	if (bUseDeterministicSeed)
	{
		RandomStream.Initialize(RandomSeed);
	}
	else
	{
		RandomStream.GenerateNewSeed();
	}
	// 첫 타일을 생성
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

	//현재 생성된 타일이 없으면 Start 타일을 생성
	const bool bNeedsStartTile = ActiveTiles.Num() == 0;
	const EStage2TileType NextTileType = bNeedsStartTile ? EStage2TileType::Start : ChooseNextTileType();

	// 해당 레벨을 NextSpawnTransform 위치에 스트리밍 로드
	const TSoftObjectPtr<UWorld> LevelToSpawn = ChooseLevelForTileType(NextTileType);


	if (LevelToSpawn.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: No level asset configured for tile type %d"), static_cast<int32>(NextTileType));
		return;
	}
	// 타일을 스폰
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

	// 다음 타일의 EntryArrow가 원하는 위치에 정확히 오도록 레벨 전체 Transform을 보정
	FTransform LevelTransformToApply = SpawnTransform;
	// 레벨의 경로를 읽어온다.
	const FSoftObjectPath LevelPath = TileLevel.ToSoftObjectPath();
	if (const FTransform* CachedEntryLocalTransform = CachedEntryLocalTransforms.Find(LevelPath))
	{
		// 보정을 해준다.
		LevelTransformToApply = CachedEntryLocalTransform->Inverse() * SpawnTransform;
	}


	bool bLoadSucceeded = false;

	// 런타임 중에 레벨 인스턴스를 동적으로 로드
	ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		TileLevel,
		LevelTransformToApply,
		bLoadSucceeded);

	if (!bLoadSucceeded || !StreamingLevel)
	{
		return false;
	}

	// Load한 Tile의 정보를 저장한다. 나중에 로드 완료된 Tile을 Finalize할 때 사용

	// ActiveTiles 배열에 새 원소를 추가하고, 그 참조를 가져온다
	FStage2LoadedTile& LoadedTile = ActiveTiles.AddDefaulted_GetRef();
	LoadedTile.SourceLevel = TileLevel;				//어떤 레벨 에셋에서 로드된 타일인지 저장
	LoadedTile.StreamingLevel = StreamingLevel;			
	LoadedTile.TileType = TileType;
	LoadedTile.RequestedEntryTransform = SpawnTransform;
	LoadedTile.AppliedLevelTransform = LevelTransformToApply;
	LoadedTile.bInitialized = false;

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
	//현재 로드 중이거나 로드 완료된 모든 타일을 검사
	for (int32 TileIndex = 0; TileIndex < ActiveTiles.Num(); ++TileIndex)
	{
		FStage2LoadedTile& LoadedTile = ActiveTiles[TileIndex];

		// 이미 초기화된 타일 건너뜀.
		if (LoadedTile.bInitialized || !LoadedTile.StreamingLevel)
		{
			continue;
		}
		
		//레벨이 로드되고, 월드에 활성화 되었는지
		if (!LoadedTile.StreamingLevel->IsLevelLoaded() ||
			!LoadedTile.StreamingLevel->IsLevelVisible())
		{
			continue;
		}
		 //마커 
		AStage2TileMarker* TileMarker =
			FindTileMarkerFromStreamingLevel(LoadedTile.StreamingLevel);

		// 마커를 못찾으면 대기한다.
		if (!TileMarker)
		{
			continue;
		}
		// 찾았으면 등록
		LoadedTile.TileMarker = TileMarker;


		FinalizeLoadedTile(TileIndex);

		// 한 Tick에 하나만 처리
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
		CachedEntryLocalTransforms.Add(SourceLevelPath, TileMarker->GetEntryTransform().GetRelativeTransform(LoadedTile.AppliedLevelTransform));
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

	TrimOldTiles();
	RefreshNavigationForStreamingTile(LoadedTile);

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
			if (MaxActiveTiles >= 3)
			{
				RemoveIndex = 1;
			}
			else if (!bLoggedKeepStartConflict)
			{
				bLoggedKeepStartConflict = true;
				UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: bKeepStartTileLoaded is enabled but MaxActiveTiles is %d. The start tile will be unloaded first to avoid removing the bridge tile the truck is currently using."), MaxActiveTiles);
			}
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
	bLoggedKeepStartConflict = false;
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

void AStage2TileManager::RefreshNavigationForStreamingTile(const FStage2LoadedTile& LoadedTile) const
{
	if (!bRebuildNavigationAfterTileLoad)
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
