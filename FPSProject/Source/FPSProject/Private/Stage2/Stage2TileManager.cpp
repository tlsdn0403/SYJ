#include "Stage2/Stage2TileManager.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Components/ModelComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelUtils.h"
#include "UObject/SoftObjectPath.h"
#include "Zombie/BaseZombie.h"

namespace
{
constexpr EStage2TileType PlayableTileSequence[] =
{
	EStage2TileType::Left,
	EStage2TileType::Right,
	EStage2TileType::Straight
};

constexpr int32 Stage2TileTypeCode_None = 0;
constexpr int32 Stage2TileTypeCode_Straight = 1;
constexpr int32 Stage2TileTypeCode_Left = 2;
constexpr int32 Stage2TileTypeCode_Right = 3;
constexpr int32 Stage2TileTypeCode_Start = 4;

constexpr int32 GetStage2ServerTileTypeCode(EStage2TileType TileType)
{
	switch (TileType)
	{
	case EStage2TileType::Start:
		return Stage2TileTypeCode_Start;
	case EStage2TileType::Straight:
		return Stage2TileTypeCode_Straight;
	case EStage2TileType::Left:
		return Stage2TileTypeCode_Left;
	case EStage2TileType::Right:
		return Stage2TileTypeCode_Right;
	default:
		return Stage2TileTypeCode_None;
	}
}
}

AStage2TileManager::AStage2TileManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	LeftLandscapeLevels.Add(TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/Maps/map_level2/L2_LEFTlandscape.L2_LEFTlandscape"))));
	RightLandscapeLevels.Add(TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/Maps/map_level2/L2_right_Land.L2_right_Land"))));

	NextSpawnTransform = GetManagerTileTransform();
}

void AStage2TileManager::BeginPlay()
{
	Super::BeginPlay();

	NextSpawnTransform = GetManagerTileTransform();
	EnsureDefaultLandscapeLevelSettings();

	if (!GetActorScale3D().Equals(FVector::OneVector))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Stage2TileManager '%s' has non-identity scale %s. Tile streaming ignores manager scale to keep landscape geometry stable."),
			*GetNameSafe(this),
			*GetActorScale3D().ToString());
	}

	if (bSpawnOnBeginPlay)
	{
		StartGeneration();
	}
}

void AStage2TileManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TryFinalizePooledTiles();
	TryFinalizeLandscapeLevels();

	if (bGenerationStarted && IsTilePoolReady() && ActiveTiles.Num() == 0)
	{
		SpawnNextTile();
	}

	if (bTilePoolReady && !HasPendingLandscapeLevels())
	{
		SetActorTickEnabled(false);
	}
}

void AStage2TileManager::StartGeneration()
{
	if (bGenerationStarted)
	{
		return;
	}
	// 풀 전용 방식에서는 시작할 때 한 번만 초기화하고, 이후에는 풀에 있는 타일을 이동해서 재사용한다.
	ResetGenerationState();
	bGenerationStarted = true;

	// 타일이 배치 될 위치는 매니저 액터의 위치
	NextSpawnTransform = GetManagerTileTransform();

	// 시드값이 고정되어 있으면 랜덤 스트림을 초기화된 시드값으로 초기화, 그렇지 않으면 새로운 시드값 생성
	if (bUseDeterministicSeed)
	{
		RandomStream.Initialize(RandomSeed);
	}
	else
	{
		RandomStream.GenerateNewSeed();
	}

	// 타일 풀을 먼저 채운 뒤, 준비가 끝나면 Tick에서 첫 타일을 꺼낸다.
	PreloadTilePool();
	SpawnNextTile();
}

void AStage2TileManager::SpawnNextTile()
{
	if (!bGenerationStarted)
	{
		return;
	}
	
	// 풀에서 꺼내쓸 수 있는지
	if (!IsTilePoolReady())
	{
		PreloadTilePool();
		return;
	}

	const bool bNeedsStartTile = ActiveTiles.Num() == 0;

	if (!bNeedsStartTile && ActiveTiles.Num() >= MaxActiveTiles)
	{
		TrimOldTiles(FMath::Max(0, MaxActiveTiles - 1));
	}

	// start 타일이 필요하다면 다음 타일 타입을 start tile로
	const EStage2TileType NextTileType = (bNeedsStartTile && IsPoolTileAvailable(EStage2TileType::Start))
		? EStage2TileType::Start
		: ChooseNextTileType();
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


bool AStage2TileManager::AreInitialTilesReady() const
{
	return bInitialTilesReady;
}

bool AStage2TileManager::TryBuildWorldTransformForTileLocalPoint(
	EStage2TileType TileType,
	const FVector& LocalLocation,
	float LocalYaw,
	int32 TileOccurrenceIndex,
	FTransform& OutTransform) const
{
	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (!LoadedTile.bInitialized || LoadedTile.TileType != TileType)
		{
			continue;
		}

		if (LoadedTile.TileOccurrenceIndex != TileOccurrenceIndex)
		{
			continue;
		}

		const FTransform LocalTransform(FRotator(0.0f, LocalYaw, 0.0f), LocalLocation);
		OutTransform = LocalTransform * LoadedTile.AppliedLevelTransform;
		return true;
	}

	return false;
}

void AStage2TileManager::GetActiveTileTypeCodes(TArray<int32>& OutTileTypeCodes) const
{
	OutTileTypeCodes.Reset();

	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (!LoadedTile.bInitialized)
		{
			continue;
		}

		const int32 TileTypeCode = GetStage2ServerTileTypeCode(LoadedTile.TileType);
		if (TileTypeCode != Stage2TileTypeCode_None)
		{
			OutTileTypeCodes.Add(TileTypeCode);
		}
	}
}

void AStage2TileManager::GetPlannedStage2ZombieTileTypeCodes(TArray<int32>& OutTileTypeCodes) const
{
	OutTileTypeCodes.Reset();

	const int32 StartTileTypeCode = GetStage2ServerTileTypeCode(EStage2TileType::Start);
	if (StartTileTypeCode != Stage2TileTypeCode_None)
	{
		OutTileTypeCodes.Add(StartTileTypeCode);
	}

	const int32 PlannedPlayableTileCount = FMath::Max(0, GetEffectiveGoalAfterPlayableTileCount());
	for (int32 PlayableTileIndex = 0; PlayableTileIndex < PlannedPlayableTileCount; ++PlayableTileIndex)
	{
		const EStage2TileType TileType =
			PlayableTileSequence[PlayableTileIndex % UE_ARRAY_COUNT(PlayableTileSequence)];
		const int32 TileTypeCode = GetStage2ServerTileTypeCode(TileType);
		if (TileTypeCode != Stage2TileTypeCode_None)
		{
			OutTileTypeCodes.Add(TileTypeCode);
		}
	}
}

void AStage2TileManager::PreloadTilePool()
{
	if (bTilePoolPreloadStarted)
	{
		return;
	}

	bTilePoolPreloadStarted = true;
	bTilePoolReady = false;
	NextPoolParkingIndex = 0;
	SetActorTickEnabled(true);

	// 각 타입별 타일을 미리 보이지 않는 위치에 로드해 두고, 런타임에는 새로 로드하지 않는다.
	QueueTilePoolLevels(StartTileLevels, EStage2TileType::Start);
	QueueTilePoolLevels(StraightTileLevels, EStage2TileType::Straight);
	QueueTilePoolLevels(LeftTileLevels, EStage2TileType::Left);
	QueueTilePoolLevels(RightTileLevels, EStage2TileType::Right);
	QueueTilePoolLevels(GoalTileLevels, EStage2TileType::Goal, 1);

	if (TilePool.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Tile pool is empty. Check Stage2 tile level settings."));
		SetActorTickEnabled(false);
	}
}

void AStage2TileManager::QueueTilePoolLevels(
	const TArray<TSoftObjectPtr<UWorld>>& LevelArray,
	EStage2TileType TileType,
	int32 OverridePoolCount)
{
	if (LevelArray.Num() == 0)
	{
		return;
	}

	const int32 PoolCount = OverridePoolCount == INDEX_NONE
		? FMath::Max(1, PreloadedTilesPerType)
		: FMath::Max(1, OverridePoolCount);
	for (int32 PoolIndex = 0; PoolIndex < PoolCount; ++PoolIndex)
	{
		const TSoftObjectPtr<UWorld>& TileLevel = LevelArray[PoolIndex % LevelArray.Num()];
		LoadPooledTileLevel(TileLevel, TileType);
	}
}

void AStage2TileManager::LoadPooledTileLevel(const TSoftObjectPtr<UWorld>& TileLevel, EStage2TileType TileType)
{
	if (TileLevel.IsNull())
	{
		return;
	}

	// 구석에다가 타일을 주차해놓는 위치
	const FTransform ParkingTransform = MakePoolParkingTransform();

	bool bLoadSucceeded = false;
	// 레벨들을 미리 로드 하는데, 플레이어가 보지 못할 위치에 생성해둠
	ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		TileLevel,
		ParkingTransform,
		bLoadSucceeded);

	if (!bLoadSucceeded || !StreamingLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Failed to preload pooled tile %s"),
			*TileLevel.ToSoftObjectPath().ToString());
		return;
	}

	// 풀에 로드한 타일의 정보를 저장
	FStage2LoadedTile& PooledTile = TilePool.AddDefaulted_GetRef();
	PooledTile.SourceLevel = TileLevel;
	PooledTile.StreamingLevel = StreamingLevel;
	PooledTile.TileType = TileType;
	PooledTile.RequestedEntryTransform = ParkingTransform;
	PooledTile.AppliedLevelTransform = ParkingTransform;
	PooledTile.bInitialized = false;

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Requested pooled load for %s type %d at %s"),
			*TileLevel.ToSoftObjectPath().ToString(),
			static_cast<int32>(TileType),
			*ParkingTransform.GetLocation().ToString());
	}
}

void AStage2TileManager::TryFinalizePooledTiles()
{
	if (!bTilePoolPreloadStarted || bTilePoolReady)
	{
		return;
	}

	// 풀 타일만 비동기 로드를 기다린다. 활성 타일은 이미 로드된 풀 타일을 이동시키기만 한다.
	for (int32 PoolIndex = 0; PoolIndex < TilePool.Num(); ++PoolIndex)
	{
		FStage2LoadedTile& PooledTile = TilePool[PoolIndex];
		if (PooledTile.bInitialized || !PooledTile.StreamingLevel)
		{
			continue;
		}
		// 풀 타일이 보이고 , 로딩이 되었는지 확인
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
		// 로드가 완료 되었다면 타일 마커를 찾아서 풀 타일 정보에 저장
		PooledTile.TileMarker = TileMarker;
		FinalizePooledTile(PoolIndex);
	}

	bool bAllPooledTilesInitialized = TilePool.Num() > 0;
	for (const FStage2LoadedTile& PooledTile : TilePool)
	{
		if (!PooledTile.bInitialized)
		{
			bAllPooledTilesInitialized = false;
			break;
		}
	}

	bTilePoolReady = bAllPooledTilesInitialized;
	if (bTilePoolReady && bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Preloaded %d pooled tile instance(s)."), TilePool.Num());
	}
	if (bTilePoolReady)
	{
		SetActorTickEnabled(false);
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

	EnsureUniqueLandscapeGuids(PooledTile);
	PooledTile.TileMarker->OnNextTileTriggerEntered.RemoveAll(this);
	PooledTile.TileMarker->ResetNextTileTrigger();
	PooledTile.TileMarker->SetNextTileTriggerEnabled(false);
	PooledTile.bInitialized = true;

	ApplyTilePerformanceSettings(PooledTile);
	SetTileRenderingEnabled(PooledTile, false);
	SetTileCollisionEnabled(PooledTile, false);
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

	// 타일 풀에서 사용할 타일을 하나 꺼냄
	FStage2LoadedTile ActivatedTile = TilePool[PoolIndex];
	TilePool.RemoveAt(PoolIndex);

	// 타일이 와야할 위치를 정해줌
	const FTransform EntryLocalTransform = ActivatedTile.bHasEntryLocalTransform
		? ActivatedTile.EntryLocalTransform
		: ActivatedTile.TileMarker->GetEntryTransform().GetRelativeTransform(ActivatedTile.AppliedLevelTransform);
	const FTransform LevelTransformToApply = EntryLocalTransform.Inverse() * EntryTransform;

	// 이제 실제로 타일을 위치로 옮겨줌
	if (!TryMoveTileTolocation(ActivatedTile, LevelTransformToApply))
	{
		TilePool.Add(ActivatedTile);
		return false;
	}

	ActivatedTile.RequestedEntryTransform = EntryTransform;
	ActivatedTile.TileType = TileType;
	ActivatedTile.TileOccurrenceIndex = 0;
	ActivatedTile.LandscapeLevels.Empty();
	LoadLandscapeLevelsForTile(ActivatedTile, LevelTransformToApply);
	// 풀에서 꺼낸 타일은 이미 로드되어 있으므로, 활성화 단계에서는 위치 이동과 트리거 재연결만 다시 처리한다.
	ActivatedTile.bInitialized = false;

	ApplyTilePerformanceSettings(ActivatedTile);
	SetTileRenderingEnabled(ActivatedTile, true);
	SetTileCollisionEnabled(ActivatedTile, true);

	const int32 ActiveTileIndex = ActiveTiles.Add(ActivatedTile);
	FinalizeLoadedTile(ActiveTileIndex);
	return true;
}

void AStage2TileManager::EnsureDefaultLandscapeLevelSettings()
{
	RemoveLandscapeLevel(LeftLandscapeLevels, TEXT("/Game/Maps/map_level2/L2_leftLandScape.L2_leftLandScape"));
	EnsureDefaultLandscapeLevel(LeftLandscapeLevels, TEXT("/Game/Maps/map_level2/L2_LEFTlandscape.L2_LEFTlandscape"));
	EnsureDefaultLandscapeLevel(RightLandscapeLevels, TEXT("/Game/Maps/map_level2/L2_right_Land.L2_right_Land"));
}

void AStage2TileManager::EnsureDefaultLandscapeLevel(
	TArray<TSoftObjectPtr<UWorld>>& LandscapeLevelArray,
	const TCHAR* LandscapeLevelPath) const
{
	const FSoftObjectPath DefaultPath(LandscapeLevelPath);
	for (const TSoftObjectPtr<UWorld>& LandscapeLevel : LandscapeLevelArray)
	{
		if (LandscapeLevel.ToSoftObjectPath() == DefaultPath)
		{
			return;
		}
	}

	LandscapeLevelArray.Add(TSoftObjectPtr<UWorld>(DefaultPath));
}

void AStage2TileManager::RemoveLandscapeLevel(
	TArray<TSoftObjectPtr<UWorld>>& LandscapeLevelArray,
	const TCHAR* LandscapeLevelPath) const
{
	const FSoftObjectPath RemovedPath(LandscapeLevelPath);
	LandscapeLevelArray.RemoveAll([&RemovedPath](const TSoftObjectPtr<UWorld>& LandscapeLevel)
	{
		return LandscapeLevel.ToSoftObjectPath() == RemovedPath;
	});
}

const TArray<TSoftObjectPtr<UWorld>>& AStage2TileManager::GetLandscapeLevelsForTileType(EStage2TileType TileType) const
{
	switch (TileType)
	{
	case EStage2TileType::Start:
		return StartLandscapeLevels;
	case EStage2TileType::Straight:
		return StraightLandscapeLevels;
	case EStage2TileType::Left:
		return LeftLandscapeLevels;
	case EStage2TileType::Right:
		return RightLandscapeLevels;
	case EStage2TileType::Goal:
		return GoalLandscapeLevels;
	default:
		return StraightLandscapeLevels;
	}
}

void AStage2TileManager::LoadLandscapeLevelsForTile(FStage2LoadedTile& LoadedTile, const FTransform& LevelTransform)
{
	const TArray<TSoftObjectPtr<UWorld>>& LandscapeLevelsToLoad = GetLandscapeLevelsForTileType(LoadedTile.TileType);
	if (LandscapeLevelsToLoad.Num() == 0)
	{
		return;
	}

	for (const TSoftObjectPtr<UWorld>& LandscapeLevel : LandscapeLevelsToLoad)
	{
		if (LandscapeLevel.IsNull())
		{
			continue;
		}

		bool bLoadSucceeded = false;
		ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
			this,
			LandscapeLevel,
			LevelTransform,
			bLoadSucceeded);

		if (!bLoadSucceeded || !StreamingLevel)
		{
			UE_LOG(LogTemp, Warning, TEXT("Stage2TileManager: Failed to load landscape tile %s"),
				*LandscapeLevel.ToSoftObjectPath().ToString());
			continue;
		}

		FStage2LoadedLandscapeLevel& LoadedLandscape = LoadedTile.LandscapeLevels.AddDefaulted_GetRef();
		LoadedLandscape.SourceLevel = LandscapeLevel;
		LoadedLandscape.StreamingLevel = StreamingLevel;
		LoadedLandscape.AppliedLevelTransform = LevelTransform;
		LoadedLandscape.bInitialized = false;

		if (bVerboseLog)
		{
			UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Requested landscape load for %s at %s"),
				*LandscapeLevel.ToSoftObjectPath().ToString(),
				*LevelTransform.GetLocation().ToString());
		}
	}

	if (LoadedTile.LandscapeLevels.Num() > 0)
	{
		SetActorTickEnabled(true);
	}
}

void AStage2TileManager::TryFinalizeLandscapeLevels()
{
	bool bFinalizedAnyLandscape = false;

	for (FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		for (FStage2LoadedLandscapeLevel& LandscapeLevel : LoadedTile.LandscapeLevels)
		{
			if (LandscapeLevel.bInitialized || !LandscapeLevel.StreamingLevel)
			{
				continue;
			}

			if (!LandscapeLevel.StreamingLevel->IsLevelLoaded() ||
				!LandscapeLevel.StreamingLevel->IsLevelVisible())
			{
				continue;
			}

			FinalizeLandscapeLevel(LandscapeLevel);
			bFinalizedAnyLandscape = true;
		}
	}

	if (bFinalizedAnyLandscape)
	{
		MarkInitialTilesReadyIfNeeded();
	}
}

void AStage2TileManager::FinalizeLandscapeLevel(FStage2LoadedLandscapeLevel& LandscapeLevel)
{
	if (LandscapeLevel.bInitialized || !LandscapeLevel.StreamingLevel)
	{
		return;
	}

	FStage2LoadedTile LandscapeTile;
	LandscapeTile.SourceLevel = LandscapeLevel.SourceLevel;
	LandscapeTile.StreamingLevel = LandscapeLevel.StreamingLevel;
	LandscapeTile.AppliedLevelTransform = LandscapeLevel.AppliedLevelTransform;

	EnsureUniqueLandscapeGuids(LandscapeTile);
	RefreshLandscapeState(LandscapeTile, true);
	LandscapeLevel.bInitialized = true;

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log, TEXT("Stage2TileManager: Finalized landscape level %s"),
			*LandscapeLevel.SourceLevel.ToSoftObjectPath().ToString());
	}
}

bool AStage2TileManager::HasPendingLandscapeLevels() const
{
	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		for (const FStage2LoadedLandscapeLevel& LandscapeLevel : LoadedTile.LandscapeLevels)
		{
			if (!LandscapeLevel.bInitialized)
			{
				return true;
			}
		}
	}

	return false;
}

bool AStage2TileManager::AreLandscapeLevelsReady(const FStage2LoadedTile& LoadedTile) const
{
	for (const FStage2LoadedLandscapeLevel& LandscapeLevel : LoadedTile.LandscapeLevels)
	{
		if (!LandscapeLevel.bInitialized)
		{
			return false;
		}
	}

	return true;
}

void AStage2TileManager::UnloadLandscapeLevelsForTile(FStage2LoadedTile& LoadedTile)
{
	for (FStage2LoadedLandscapeLevel& LandscapeLevel : LoadedTile.LandscapeLevels)
	{
		if (LandscapeLevel.StreamingLevel)
		{
			LandscapeLevel.StreamingLevel->SetShouldBeLoaded(false);
			LandscapeLevel.StreamingLevel->SetShouldBeVisible(false);
			LandscapeLevel.StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
		}
	}

	LoadedTile.LandscapeLevels.Empty();
}

bool AStage2TileManager::TryMoveTileTolocation(FStage2LoadedTile& LoadedTile, const FTransform& NewLevelTransform)
{
	if (!LoadedTile.StreamingLevel)
	{
		return false;
	}

	//스트리밍 레벨을 관리하는 객체에서 실제로 로드된 레벨을 가져옴
	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return false;
	}

	// 현재 이 타일에 적용되어 있는 위치(플레이어에게 안보이는 위치)
	const FTransform OldLevelTransform = LoadedTile.AppliedLevelTransform;
	// 바꿀 위치랑 같다면 굳이 적용 안함
	if (!OldLevelTransform.Equals(NewLevelTransform))
	{
		//타일의 위치를 옮길 때 얼마나 위치를 옮겨야 하는지
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
				ModelComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
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
				ActorRootComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
			}
		}
	}

	LoadedTile.StreamingLevel->LevelTransform = NewLevelTransform;
	LoadedTile.AppliedLevelTransform = NewLevelTransform;
	RefreshTilePhysicsState(LoadedTile);
	return true;
}

void AStage2TileManager::ApplyTilePerformanceSettings(const FStage2LoadedTile& LoadedTile) const
{
	if (!LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	for (UModelComponent* ModelComponent : LoadedLevel->ModelComponents)
	{
		ApplyPrimitivePerformanceSettings(ModelComponent);
	}

	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (!IsValid(LevelActor))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(LevelActor);
		LevelActor->GetComponents(PrimitiveComponents);

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			ApplyPrimitivePerformanceSettings(PrimitiveComponent);
		}
	}
}

void AStage2TileManager::ApplyPrimitivePerformanceSettings(UPrimitiveComponent* PrimitiveComponent) const
{
	if (!IsValid(PrimitiveComponent) || !PrimitiveComponent->IsRegistered())
	{
		return;
	}

	const float BoundsRadius = PrimitiveComponent->Bounds.SphereRadius;
	const bool bSmallPrimitive = TileSmallPrimitiveBoundsRadius > 0.0f && BoundsRadius <= TileSmallPrimitiveBoundsRadius;

	if (bApplyTileCullDistances)
	{
		const float CullDistance = bSmallPrimitive ? TileSmallPrimitiveCullDistance : TilePrimitiveCullDistance;
		if (CullDistance > 0.0f)
		{
			PrimitiveComponent->SetCullDistance(CullDistance);
		}
	}

	if (bDisableSmallTilePrimitiveShadows && bSmallPrimitive)
	{
		PrimitiveComponent->SetCastShadow(false);
	}
}

void AStage2TileManager::EnsureUniqueLandscapeGuids(const FStage2LoadedTile& LoadedTile) const
{
	if (!LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	TMap<FGuid, FGuid> RemappedGuids;
	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (!IsValid(LevelActor))
		{
			continue;
		}

		EnsureLandscapeProxyHasUniqueGuid(Cast<ALandscapeProxy>(LevelActor), LoadedTile, RemappedGuids);
	}
}

void AStage2TileManager::EnsureLandscapeProxyHasUniqueGuid(
	ALandscapeProxy* LandscapeProxy,
	const FStage2LoadedTile& LoadedTile,
	TMap<FGuid, FGuid>& RemappedGuids) const
{
	if (!IsValid(LandscapeProxy))
	{
		return;
	}

	const FGuid OriginalGuid = LandscapeProxy->GetLandscapeGuid();
	FGuid& RemappedGuid = RemappedGuids.FindOrAdd(OriginalGuid);
	if (!RemappedGuid.IsValid())
	{
		RemappedGuid = FGuid::NewGuid();
		while (RemappedGuid == OriginalGuid)
		{
			RemappedGuid = FGuid::NewGuid();
		}
	}

	LandscapeProxy->SetLandscapeGuid(RemappedGuid, false);
	LandscapeProxy->CreateLandscapeInfo(false, true);
	LandscapeProxy->ReregisterAllComponents();
	RefreshLandscapeProxyState(LandscapeProxy, true);

	if (bVerboseLog)
	{
		UE_LOG(LogTemp, Log,
			TEXT("Stage2TileManager: Remapped landscape guid for %s in %s from %s to %s"),
			*GetNameSafe(LandscapeProxy),
			*LoadedTile.SourceLevel.ToSoftObjectPath().ToString(),
			*OriginalGuid.ToString(),
			*RemappedGuid.ToString());
	}
}

void AStage2TileManager::SetTileRenderingEnabled(const FStage2LoadedTile& LoadedTile, bool bEnabled) const
{
	if (!bHidePooledTiles || !LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	// 모델 컴포넌트 숨기거나 안숨겨지게 함
	for (UModelComponent* ModelComponent : LoadedLevel->ModelComponents)
	{
		if (ModelComponent)
		{
			ModelComponent->SetHiddenInGame(!bEnabled);
		}
	}
	// 엑터 컴포넌트 숨기거나 안숨겨지게 함
	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (IsValid(LevelActor))
		{
			LevelActor->SetActorHiddenInGame(!bEnabled);
		}
	}
}

void AStage2TileManager::SetTileCollisionEnabled(const FStage2LoadedTile& LoadedTile, bool bEnabled)
{
	if (!bDisablePooledTileCollision || !LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	for (UModelComponent* ModelComponent : LoadedLevel->ModelComponents)
	{
		if (!IsValid(ModelComponent) || !ModelComponent->IsRegistered())
		{
			continue;
		}

		const TObjectKey<UPrimitiveComponent> ComponentKey(ModelComponent);
		if (bEnabled)
		{
			if (const ECollisionEnabled::Type* CachedCollision = CachedTileCollisionStates.Find(ComponentKey))
			{
				ModelComponent->SetCollisionEnabled(*CachedCollision);
			}
		}
		else
		{
			CachedTileCollisionStates.FindOrAdd(ComponentKey, ModelComponent->GetCollisionEnabled());
			ModelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		ModelComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
	}

	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (!IsValid(LevelActor))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(LevelActor);
		LevelActor->GetComponents(PrimitiveComponents);

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent) || !PrimitiveComponent->IsRegistered())
			{
				continue;
			}

			const TObjectKey<UPrimitiveComponent> ComponentKey(PrimitiveComponent);
			if (bEnabled)
			{
				if (const ECollisionEnabled::Type* CachedCollision = CachedTileCollisionStates.Find(ComponentKey))
				{
					PrimitiveComponent->SetCollisionEnabled(*CachedCollision);
				}
				else if (Cast<ULandscapeHeightfieldCollisionComponent>(PrimitiveComponent))
				{
					PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				}
			}
			else
			{
				CachedTileCollisionStates.FindOrAdd(ComponentKey, PrimitiveComponent->GetCollisionEnabled());
				PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}

			PrimitiveComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
		}
	}

	RefreshLandscapeState(LoadedTile, bEnabled);
	RefreshTilePhysicsState(LoadedTile);
}

void AStage2TileManager::RefreshLandscapeState(const FStage2LoadedTile& LoadedTile, bool bRecreateCollision) const
{
	if (!LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		RefreshLandscapeProxyState(Cast<ALandscapeProxy>(LevelActor), bRecreateCollision);
	}
}

void AStage2TileManager::RefreshLandscapeProxyState(ALandscapeProxy* LandscapeProxy, bool bRecreateCollision) const
{
	if (!IsValid(LandscapeProxy))
	{
		return;
	}

	TSet<ULandscapeHeightfieldCollisionComponent*> RefreshedCollisionComponents;

	TInlineComponentArray<ULandscapeComponent*> LandscapeComponents;
	LandscapeProxy->GetComponents(LandscapeComponents);
	for (ULandscapeComponent* LandscapeComponent : LandscapeComponents)
	{
		if (!IsValid(LandscapeComponent) || !LandscapeComponent->IsRegistered())
		{
			continue;
		}

		LandscapeComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
		LandscapeComponent->UpdateBounds();

		RefreshLandscapeCollisionComponent(
			LandscapeComponent->GetCollisionComponent(),
			RefreshedCollisionComponents,
			bRecreateCollision);
	}

	TInlineComponentArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;
	LandscapeProxy->GetComponents(CollisionComponents);
	for (ULandscapeHeightfieldCollisionComponent* CollisionComponent : CollisionComponents)
	{
		RefreshLandscapeCollisionComponent(CollisionComponent, RefreshedCollisionComponents, bRecreateCollision);
	}
}

void AStage2TileManager::RefreshLandscapeCollisionComponent(
	ULandscapeHeightfieldCollisionComponent* CollisionComponent,
	TSet<ULandscapeHeightfieldCollisionComponent*>& RefreshedCollisionComponents,
	bool bRecreateCollision) const
{
	if (!IsValid(CollisionComponent) ||
		!CollisionComponent->IsRegistered() ||
		RefreshedCollisionComponents.Contains(CollisionComponent))
	{
		return;
	}

	RefreshedCollisionComponents.Add(CollisionComponent);
	CollisionComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);

	if (CollisionComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return;
	}

	if (bRecreateCollision)
	{
		CollisionComponent->RecreateCollision();
	}
	else
	{
		CollisionComponent->RecreatePhysicsState();
	}
}

void AStage2TileManager::RefreshTilePhysicsState(const FStage2LoadedTile& LoadedTile) const
{
	if (!LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	for (UModelComponent* ModelComponent : LoadedLevel->ModelComponents)
	{
		if (!ModelComponent || !ModelComponent->IsRegistered())
		{
			continue;
		}

		ModelComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
		if (ModelComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			ModelComponent->RecreatePhysicsState();
		}
	}

	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (!IsValid(LevelActor))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(LevelActor);
		LevelActor->GetComponents(PrimitiveComponents);

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent) || !PrimitiveComponent->IsRegistered())
			{
				continue;
			}

			PrimitiveComponent->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
			if (PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				PrimitiveComponent->RecreatePhysicsState();
			}
		}
	}

	RefreshLandscapeState(LoadedTile, true);
}

void AStage2TileManager::ForgetTileCollisionStates(const FStage2LoadedTile& LoadedTile)
{
	if (!LoadedTile.StreamingLevel)
	{
		return;
	}

	ULevel* LoadedLevel = LoadedTile.StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return;
	}

	for (UModelComponent* ModelComponent : LoadedLevel->ModelComponents)
	{
		if (ModelComponent)
		{
			CachedTileCollisionStates.Remove(TObjectKey<UPrimitiveComponent>(ModelComponent));
		}
	}

	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (!IsValid(LevelActor))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(LevelActor);
		LevelActor->GetComponents(PrimitiveComponents);

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent)
			{
				CachedTileCollisionStates.Remove(TObjectKey<UPrimitiveComponent>(PrimitiveComponent));
			}
		}
	}
}

FTransform AStage2TileManager::GetManagerTileTransform() const
{
	return FTransform(GetActorRotation(), GetActorLocation(), FVector::OneVector);
}

FTransform AStage2TileManager::MakePoolParkingTransform()
{
	
	const FVector ParkingLocation =
		GetActorLocation() +
		PoolParkingOffset +
		FVector(NextPoolParkingIndex * PoolParkingSpacing, 0.0f, 0.0f);

	++NextPoolParkingIndex;
	return FTransform(GetActorRotation(), ParkingLocation, FVector::OneVector);
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
	switch (TileType)
	{
	case EStage2TileType::Start:
		LoadedTile.TileOccurrenceIndex = NextStartTileOccurrenceIndex++;
		break;
	case EStage2TileType::Straight:
		LoadedTile.TileOccurrenceIndex = NextStraightTileOccurrenceIndex++;
		break;
	case EStage2TileType::Left:
		LoadedTile.TileOccurrenceIndex = NextLeftTileOccurrenceIndex++;
		break;
	case EStage2TileType::Right:
		LoadedTile.TileOccurrenceIndex = NextRightTileOccurrenceIndex++;
		break;
	case EStage2TileType::Goal:
		LoadedTile.TileOccurrenceIndex = NextGoalTileOccurrenceIndex++;
		break;
	default:
		LoadedTile.TileOccurrenceIndex = 0;
		break;
	}

	// 재활용될 때도 같은 기준점으로 맞출 수 있도록, 타일의 Entry 위치만 저장한다.
	LoadedTile.EntryLocalTransform = TileMarker->GetEntryTransform().GetRelativeTransform(LoadedTile.AppliedLevelTransform);
	LoadedTile.bHasEntryLocalTransform = true;

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
			// 시작 타일 보존 옵션은 단순하게 두 번째 타일부터 풀로 돌려보낸다.
			RemoveIndex = 1;
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

	SetTileRenderingEnabled(RecycledTile, false);
	SetTileCollisionEnabled(RecycledTile, false);
	UnloadLandscapeLevelsForTile(RecycledTile);
	TryMoveTileTolocation(RecycledTile, MakePoolParkingTransform());
	RecycledTile.RequestedEntryTransform = RecycledTile.AppliedLevelTransform;
	RecycledTile.bInitialized = true;
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
		ForgetTileCollisionStates(LoadedTile);
		LoadedTile.StreamingLevel->SetShouldBeLoaded(false);
		LoadedTile.StreamingLevel->SetShouldBeVisible(false);
		LoadedTile.StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
	}

	UnloadLandscapeLevelsForTile(LoadedTile);
}

void AStage2TileManager::ResetGenerationState()
{
	bGenerationStarted = false;
	bGoalTileSpawnRequested = false;
	bInitialTilesReady = false;
	bTilePoolPreloadStarted = false;
	bTilePoolReady = false;
	ConsecutiveLeftTurns = 0;
	ConsecutiveRightTurns = 0;
	NextStartTileOccurrenceIndex = 0;
	NextStraightTileOccurrenceIndex = 0;
	NextLeftTileOccurrenceIndex = 0;
	NextRightTileOccurrenceIndex = 0;
	NextGoalTileOccurrenceIndex = 0;
	NextPoolParkingIndex = 0;
	SpawnedPlayableTileCount = 0;
	NextSpawnTransform = GetManagerTileTransform();
	SetActorTickEnabled(false);
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
	const uint32 TileGroupHash = HashCombine(
		static_cast<uint32>(LoadedTile.TileType),
		static_cast<uint32>(LoadedTile.TileOccurrenceIndex + 1));
	const int32 ZombieGroupSoundKey = TileGroupHash != 0 ? static_cast<int32>(TileGroupHash) : 1;

	TArray<TEnumAsByte<EObjectTypeQuery>> OverlapObjectTypes;
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<FVector> UsedZombieSpawnLocations;
	const float ZombieSpawnMinSpacingSq = ZombieSpawnMinSpacing * ZombieSpawnMinSpacing;

	for (int32 SpawnIteration = 0; SpawnIteration < DesiredSpawnCount && CandidateSpawnTransforms.Num() > 0; ++SpawnIteration)
	{
		int32 SpawnTransformIndex = INDEX_NONE;
		if (ZombieSpawnMinSpacing > 0.0f && UsedZombieSpawnLocations.Num() > 0)
		{
			const int32 StartIndex = RandomStream.RandRange(0, CandidateSpawnTransforms.Num() - 1);
			for (int32 CandidateOffset = 0; CandidateOffset < CandidateSpawnTransforms.Num(); ++CandidateOffset)
			{
				const int32 CandidateIndex = (StartIndex + CandidateOffset) % CandidateSpawnTransforms.Num();
				const FVector CandidateLocation = CandidateSpawnTransforms[CandidateIndex].GetLocation();

				bool bTooClose = false;
				for (const FVector& UsedLocation : UsedZombieSpawnLocations)
				{
					if (FVector::DistSquared2D(CandidateLocation, UsedLocation) < ZombieSpawnMinSpacingSq)
					{
						bTooClose = true;
						break;
					}
				}

				if (!bTooClose)
				{
					SpawnTransformIndex = CandidateIndex;
					break;
				}
			}
		}

		if (SpawnTransformIndex == INDEX_NONE)
		{
			SpawnTransformIndex = RandomStream.RandRange(0, CandidateSpawnTransforms.Num() - 1);
		}

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
			SpawnedZombie->SetZombieGroupSoundKey(ZombieGroupSoundKey);
			LoadedTile.SpawnedZombies.Add(SpawnedZombie);
			UsedZombieSpawnLocations.Add(SpawnedZombie->GetActorLocation());
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

int32 AStage2TileManager::GetEffectiveGoalAfterPlayableTileCount() const
{
	return bDebugSpawnGoalAfterOnePlayableTile
		? 1
		: GoalAfterPlayableTileCount;
}

EStage2TileType AStage2TileManager::ChooseNextTileType()
{
	if (!bGoalTileSpawnRequested &&
		GoalTileLevels.Num() > 0 &&
		SpawnedPlayableTileCount >= GetEffectiveGoalAfterPlayableTileCount() &&
		IsPoolTileAvailable(EStage2TileType::Goal))
	{
		return EStage2TileType::Goal;
	}

	const int32 SequenceIndex = SpawnedPlayableTileCount % UE_ARRAY_COUNT(PlayableTileSequence);
	for (int32 Offset = 0; Offset < UE_ARRAY_COUNT(PlayableTileSequence); ++Offset)
	{
		const EStage2TileType CandidateTileType =
			PlayableTileSequence[(SequenceIndex + Offset) % UE_ARRAY_COUNT(PlayableTileSequence)];
		if (IsPoolTileAvailable(CandidateTileType))
		{
			return CandidateTileType;
		}
	}

	return EStage2TileType::Straight;
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

void AStage2TileManager::MarkInitialTilesReadyIfNeeded()
{
	if (bInitialTilesReady)
	{
		return;
	}

	const int32 RequiredInitialTileCount = FMath::Max(1, InitialTilesToSpawn);
	if (GetInitializedTileCount() < RequiredInitialTileCount)
	{
		return;
	}

	int32 ReadyInitialTileCount = 0;
	for (const FStage2LoadedTile& LoadedTile : ActiveTiles)
	{
		if (LoadedTile.bInitialized && AreLandscapeLevelsReady(LoadedTile))
		{
			++ReadyInitialTileCount;
		}

		if (ReadyInitialTileCount >= RequiredInitialTileCount)
		{
			break;
		}
	}

	if (ReadyInitialTileCount < RequiredInitialTileCount)
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