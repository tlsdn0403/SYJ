#include "FPSStageFlowManager.h"
#include "FPSProjectGameInstance.h"
#include "FPSStage2WorldUtils.h"
#include "ClientPacketHandler.h"
#include "Characters/FPSBaseCharacter.h"
#include "Characters/FPSPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "HUD/BaseUI.h"
#include "HUD/LoadingUI.h"
#include "Items/LootItemBase.h"
#include "Items/Stage1ItemSpawnPoint.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Stage/StageTransitionZone.h"
#include "Stage2/Stage2TileManager.h"
#include "Truck/Truck.h"
#include "Algo/Sort.h"

FFPSStageFlowManager::FFPSStageFlowManager(UFPSProjectGameInstance& InOwner)
	: Owner(InOwner)
{
}

bool FFPSStageFlowManager::ShouldDelayEnterGameRequest() const
{
	if (const AStage2TileManager* Stage2TileManager = FPSStage2WorldUtils::FindStage2TileManager(Owner.GetWorld()))
	{
		return !Stage2TileManager->AreInitialTilesReady();
	}

	if (bWaitingForStage2MapLoad && FPSStage2WorldUtils::IsStage2LevelName(PendingStageTransitionLevelName))
	{
		return true;
	}

	if (FPSStage2WorldUtils::IsStage2World(Owner.GetWorld()))
	{
		return true;
	}

	return false;
}

void FFPSStageFlowManager::RequestEnterGameWhenReady()
{
	bPendingEnterGameRequest = true;
	bEnterGamePacketSent = false;
	bShouldShowEntryLoadingWidget = true;
	bStageTimerExpiredTransitionRequested = false;
	CachedEntryLoadingReadyCount = 0;
	CachedStage1ItemSpawnSeed = 0;
	bHasStage1ItemSpawnSeed = false;
	bHasAppliedStage1ItemSpawns = false;
	bHasDistributedStage1CargoItems = false;
}

bool FFPSStageFlowManager::TrySendEnterGamePacket()
{
	if (!bPendingEnterGameRequest || bEnterGamePacketSent)
	{
		return false;
	}

	if (!Owner.IsConnectedToGameServer())
	{
		return false;
	}

	if (ShouldDelayEnterGameRequest())
	{
		return false;
	}

	Protocol::C_ENTER_GAME EnterGamePkt;
	EnterGamePkt.set_playerindex(0);
	Owner.SendPacket(ClientPacketHandler::MakeSendBuffer(EnterGamePkt));

	bEnterGamePacketSent = true;
	bPendingEnterGameRequest = false;
	UE_LOG(LogTemp, Warning, TEXT("[Network] Stage2 ready check passed. C_ENTER_GAME 전송 완료!"));
	return true;
}

void FFPSStageFlowManager::RefreshStage2StartupActorHold()
{
	const bool bShouldHold = Owner.ShouldDelayStage2ActorSpawn();
	if (bShouldHold || Owner.bStage2StartupHoldApplied)
	{
		Owner.ApplyStage2StartupActorHold(bShouldHold);
	}
}

void FFPSStageFlowManager::SetEntryLoadingWidgetClass(TSubclassOf<UUserWidget> WidgetClass)
{
	Owner.EntryLoadingWidgetClass = WidgetClass;
	CachedEntryLoadingReadyCount = 0;
}

void FFPSStageFlowManager::ShowEntryLoadingWidget()
{
	bShouldShowEntryLoadingWidget = true;

	if (Owner.EntryLoadingWidget)
	{
		return;
	}

	if (!Owner.EntryLoadingWidgetClass)
	{
		return;
	}

	UWorld* World = Owner.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(World, Owner.EntryLoadingWidgetClass);
	if (Widget == nullptr)
	{
		return;
	}

	Widget->AddToViewport();
	RegisterEntryLoadingWidget(Widget);
}

void FFPSStageFlowManager::RegisterEntryLoadingWidget(UUserWidget* Widget)
{
	Owner.EntryLoadingWidget = Widget;
	ApplyEntryLoadingReadyCount(CachedEntryLoadingReadyCount);
}

void FFPSStageFlowManager::RemoveEntryLoadingWidget()
{
	bShouldShowEntryLoadingWidget = false;

	if (Owner.EntryLoadingWidget)
	{
		Owner.EntryLoadingWidget->RemoveFromParent();
		Owner.EntryLoadingWidget = nullptr;
	}
}

void FFPSStageFlowManager::HandlePostLoadMap(UWorld* LoadedWorld)
{
	bHasDistributedStage1CargoItems = false;

	if (!FPSStage2WorldUtils::IsStage2World(LoadedWorld))
	{
		bStageTimerExpiredTransitionRequested = false;
	}

	if (Owner.EntryLoadingWidget)
	{
		Owner.EntryLoadingWidget->RemoveFromParent();
		Owner.EntryLoadingWidget = nullptr;
	}

	if (bShouldShowEntryLoadingWidget)
	{
		ShowEntryLoadingWidget();
	}

	bHasAppliedStage1ItemSpawns = false;
	ApplyStage1ItemSpawnSeed();

	if (bWaitingForStage2MapLoad && !FPSStage2WorldUtils::IsStage2World(LoadedWorld))
	{
		bWaitingForStage2MapLoad = false;
		PendingStageTransitionLevelName.Empty();
	}
}

void FFPSStageFlowManager::CompleteStage2MapLoad()
{
	RemoveEntryLoadingWidget();
	bWaitingForStage2MapLoad = false;
	PendingStageTransitionLevelName.Empty();
}

void FFPSStageFlowManager::ApplyEntryLoadingReadyCount(int32 ReadyCount)
{
	CachedEntryLoadingReadyCount = ReadyCount;

	ULoadingUI* LoadingUI = Cast<ULoadingUI>(Owner.EntryLoadingWidget);
	if (LoadingUI == nullptr)
	{
		return;
	}

	LoadingUI->logout();
	LoadingUI->OnlineP = FMath::Clamp(ReadyCount, 0, 3);
	LoadingUI->connect(LoadingUI->OnlineP);
}

void FFPSStageFlowManager::ProcessPendingStage2Spawns()
{
	if (Owner.PendingStage2SpawnInfos.Num() == 0 || Owner.ShouldDelayStage2ActorSpawn())
	{
		return;
	}

	TArray<UFPSProjectGameInstance::FPendingStage2SpawnInfo> SpawnsToProcess = MoveTemp(Owner.PendingStage2SpawnInfos);
	Owner.PendingStage2SpawnInfos.Reset();

	TGuardValue<bool> ProcessingGuard(Owner.bProcessingPendingStage2Spawns, true);
	for (const UFPSProjectGameInstance::FPendingStage2SpawnInfo& PendingSpawn : SpawnsToProcess)
	{
		if (PendingSpawn.bIsMine)
		{
			RemoveEntryLoadingWidget();
			bWaitingForStage2MapLoad = false;
			PendingStageTransitionLevelName.Empty();
		}

		Owner.ProcessSpawnObject(PendingSpawn.ObjectInfo, PendingSpawn.bIsMine);
	}

	TryDistributeStage1CargoItemsToPlayers();
}

void FFPSStageFlowManager::TryDistributeStage1CargoItemsToPlayers()
{
	if (bHasDistributedStage1CargoItems || Owner.RecordedStage1CargoItems.Num() == 0)
	{
		return;
	}

	if (!FPSStage2WorldUtils::IsStage2World(Owner.GetWorld()) || Owner.ShouldDelayStage2ActorSpawn() || Owner.PendingStage2SpawnInfos.Num() > 0)
	{
		return;
	}

	if (Owner.IsConnectedToGameServer() && CachedEntryLoadingReadyCount <= 0)
	{
		return;
	}

	TArray<TPair<uint64, AFPSBaseCharacter*>> Stage2Players;
	Owner.GetValidRegisteredPlayers(Stage2Players);

	const int32 ExpectedPlayerCount = FMath::Max(CachedEntryLoadingReadyCount, 1);
	if (Stage2Players.Num() < ExpectedPlayerCount)
	{
		return;
	}

	const int32 PlayerCount = Stage2Players.Num();
	for (const TPair<EItemType, int32>& CargoEntry : Owner.RecordedStage1CargoItems)
	{
		const EItemType ItemType = CargoEntry.Key;
		const int32 ItemCount = CargoEntry.Value;
		UE_LOG(LogTemp, Verbose, TEXT("[Stage2Cargo] Distribute item type=%d count=%d players=%d"),
			static_cast<int32>(ItemType),
			ItemCount,
			PlayerCount);
		if (ItemType == EItemType::None || ItemCount <= 0)
		{
			continue;
		}

		const int32 BaseShare = ItemCount / PlayerCount;
		const int32 Remainder = ItemCount % PlayerCount;
		auto GrantCargoItem = [ItemType](const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry)
			{
				PlayerEntry.Value->AddStage2DistributedItem(ItemType);
				UE_LOG(LogTemp, Verbose, TEXT("[Stage2Cargo] Grant item type=%d to playerId=%llu"),
					static_cast<int32>(ItemType),
					PlayerEntry.Key);
			};

		for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Stage2Players)
		{
			for (int32 i = 0; i < BaseShare; ++i)
			{
				GrantCargoItem(PlayerEntry);
			}
		}

		if (Remainder > 0)
		{
			for (int32 i = 0; i < Remainder; ++i)
			{
				GrantCargoItem(Stage2Players[i]);
			}
		}
	}

	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Stage2Players)
	{
		PlayerEntry.Value->RefreshStage2ItemUI();
	}

	Owner.ClearRecordedStage1CargoItems();
	bHasDistributedStage1CargoItems = true;
	UE_LOG(LogTemp, Log, TEXT("[Stage2Cargo] Distributed Stage1 cargo to %d players."), PlayerCount);
}

void FFPSStageFlowManager::HandleStageTimer(const Protocol::S_STAGE_TIMER& Pkt)
{
	const bool bWasCountingDown = CachedStageTimerRemainingSeconds > 0;
	CachedStageTimerRemainingSeconds = Pkt.is_loading_phase() ? Pkt.remaining_seconds() : 0;
	ApplyStageTimerToLocalUI();

	if (!bStageTimerExpiredTransitionRequested &&
		(bWasCountingDown || Pkt.is_loading_phase()) &&
		CachedStageTimerRemainingSeconds <= 0)
	{
		RequestStageTransitionAfterFarmingTimer();
	}
}

void FFPSStageFlowManager::HandleStage1ItemSeed(const Protocol::S_STAGE1_ITEM_SEED& Pkt)
{
	CachedStage1ItemSpawnSeed = Pkt.seed();
	bHasStage1ItemSpawnSeed = true;
	bHasAppliedStage1ItemSpawns = false;
	ApplyStage1ItemSpawnSeed();
}

void FFPSStageFlowManager::HandleStageTransition(const Protocol::S_STAGE_TRANSITION& Pkt)
{
	const FString TargetLevelName = UTF8_TO_TCHAR(Pkt.target_level().c_str());
	if (TargetLevelName.IsEmpty())
	{
		return;
	}

	PendingStageTransitionLevelName = TargetLevelName;
	bStageTimerExpiredTransitionRequested = true;
	const bool bTargetIsStage2 = FPSStage2WorldUtils::IsStage2LevelName(TargetLevelName);
	bWaitingForStage2MapLoad = false;

	if (bTargetIsStage2 && TryPlayStageTransitionCinematic())
	{
		return;
	}

	OpenPendingStageTransitionLevel();
}

void FFPSStageFlowManager::TickStageFlow()
{
	RefreshStage2StartupActorHold();
	ProcessPendingStage2Spawns();
	TryDistributeStage1CargoItemsToPlayers();
}

void FFPSStageFlowManager::ApplyStageTimerToLocalUI()
{
	if (CachedStageTimerRemainingSeconds == INDEX_NONE)
	{
		return;
	}

	AFPSPlayerController* PlayerController = Cast<AFPSPlayerController>(UGameplayStatics::GetPlayerController(&Owner, 0));
	if (PlayerController == nullptr || PlayerController->TimerW == nullptr)
	{
		return;
	}

	PlayerController->TimerW->SetRemainingTime(CachedStageTimerRemainingSeconds);
}

void FFPSStageFlowManager::ApplyStage1ItemSpawnSeed()
{
	if (!bHasStage1ItemSpawnSeed || bHasAppliedStage1ItemSpawns)
	{
		return;
	}

	UWorld* World = Owner.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TArray<AStage1ItemSpawnPoint*> SpawnPoints;
	for (TActorIterator<AStage1ItemSpawnPoint> It(World); It; ++It)
	{
		if (AStage1ItemSpawnPoint* SpawnPoint = *It)
		{
			SpawnPoints.Add(SpawnPoint);
		}
	}

	Algo::SortBy(SpawnPoints, [](const AStage1ItemSpawnPoint* SpawnPoint)
	{
		return GetPathNameSafe(SpawnPoint);
	});

	FRandomStream RandomStream(static_cast<int32>(CachedStage1ItemSpawnSeed));
	for (AStage1ItemSpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (SpawnPoint == nullptr)
		{
			continue;
		}

		SpawnPoint->ClearSpawnedItem();
		SpawnPoint->SpawnItemFromRandomStream(RandomStream);
		if (ALootItemBase* LootItem = SpawnPoint->GetSpawnedItem())
		{
			Owner.RegisterNetworkLootItem(LootItem);
		}
	}

	bHasAppliedStage1ItemSpawns = true;
}

void FFPSStageFlowManager::RequestStageTransitionAfterFarmingTimer()
{
	if (bStageTransitionCinematicPlaying || FPSStage2WorldUtils::IsStage2World(Owner.GetWorld()))
	{
		return;
	}

	const FName TargetLevelName = ResolveStageTransitionTargetLevelName();
	if (TargetLevelName.IsNone())
	{
		return;
	}

	bStageTimerExpiredTransitionRequested = true;

	if (Owner.IsConnectedToGameServer())
	{
		uint64 TruckId = 0;
		if (UWorld* World = Owner.GetWorld())
		{
			for (TActorIterator<ATruck> It(World); It; ++It)
			{
				if (ATruck* Truck = *It)
				{
					TruckId = Truck->NetworkTruckId;
					break;
				}
			}
		}

		Protocol::C_STAGE_TRANSITION_REQUEST RequestPkt;
		RequestPkt.set_truck_id(TruckId);
		RequestPkt.set_target_level(TCHAR_TO_UTF8(*TargetLevelName.ToString()));
		Owner.SendPacket(ClientPacketHandler::MakeSendBuffer(RequestPkt));

		UE_LOG(LogTemp, Log,
			TEXT("[StageTransition] Farming timer expired. Requested server transition to '%s' truckId=%llu"),
			*TargetLevelName.ToString(),
			TruckId);
		return;
	}

	PendingStageTransitionLevelName = TargetLevelName.ToString();
	if (!TryPlayStageTransitionCinematic())
	{
		OpenPendingStageTransitionLevel();
	}
}

FName FFPSStageFlowManager::ResolveStageTransitionTargetLevelName() const
{
	if (UWorld* World = Owner.GetWorld())
	{
		for (TActorIterator<AStageTransitionZone> It(World); It; ++It)
		{
			if (const AStageTransitionZone* TransitionZone = *It)
			{
				if (!TransitionZone->TargetLevelName.IsNone())
				{
					return TransitionZone->TargetLevelName;
				}
			}
		}
	}

	return TEXT("map_level2_test");
}

bool FFPSStageFlowManager::TryPlayStageTransitionCinematic()
{
	if (bStageTransitionCinematicPlaying)
	{
		return true;
	}

	UWorld* World = Owner.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	static const TCHAR* StageTransitionSequencePath =
		TEXT("/Game/Maps/Map_Level1/LS_Zombie.LS_Zombie");
	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, StageTransitionSequencePath);
	if (Sequence == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[StageTransition] Failed to load cinematic sequence: %s"),
			StageTransitionSequencePath);
		return false;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;

	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* SequencePlayer =
		ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, PlaybackSettings, SequenceActor);
	if (SequencePlayer == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StageTransition] Failed to create LS_level1 player."));
		return false;
	}

	StageTransitionSequenceActor = SequenceActor;
	StageTransitionSequencePlayer = SequencePlayer;
	bStageTransitionCinematicPlaying = true;
	PrepareStageTransitionCinematicActors();
	SetStageTransitionCinematicMode(true);

	float SequenceDurationSeconds = 0.0f;
	if (const UMovieScene* MovieScene = Sequence->GetMovieScene())
	{
		const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
		if (PlaybackRange.HasLowerBound() && PlaybackRange.HasUpperBound())
		{
			const FFrameNumber DurationFrames =
				PlaybackRange.GetUpperBoundValue() - PlaybackRange.GetLowerBoundValue();
			SequenceDurationSeconds = static_cast<float>(
				MovieScene->GetTickResolution().AsSeconds(DurationFrames));
		}
	}

	SequencePlayer->Play();

	const float SafeDurationSeconds = FMath::Max(SequenceDurationSeconds, 0.1f);
	World->GetTimerManager().SetTimer(
		StageTransitionCinematicTimerHandle,
		FTimerDelegate::CreateRaw(this, &FFPSStageFlowManager::FinishStageTransitionCinematic),
		SafeDurationSeconds,
		false);

	UE_LOG(LogTemp, Log,
		TEXT("[StageTransition] Playing LS_Zombie before opening level '%s' duration=%.2f"),
		*PendingStageTransitionLevelName,
		SafeDurationSeconds);
	return true;
}

void FFPSStageFlowManager::FinishStageTransitionCinematic()
{
	if (!bStageTransitionCinematicPlaying)
	{
		return;
	}

	if (ULevelSequencePlayer* SequencePlayer = StageTransitionSequencePlayer.Get())
	{
		SequencePlayer->Stop();
	}

	if (ALevelSequenceActor* SequenceActor = StageTransitionSequenceActor.Get())
	{
		SequenceActor->Destroy();
	}

	StageTransitionSequencePlayer.Reset();
	StageTransitionSequenceActor.Reset();
	bStageTransitionCinematicPlaying = false;
	SetStageTransitionCinematicMode(false);

	OpenPendingStageTransitionLevel();
}

void FFPSStageFlowManager::OpenPendingStageTransitionLevel()
{
	if (PendingStageTransitionLevelName.IsEmpty())
	{
		return;
	}

	bWaitingForStage2MapLoad = FPSStage2WorldUtils::IsStage2LevelName(PendingStageTransitionLevelName);
	UGameplayStatics::OpenLevel(&Owner, FName(*PendingStageTransitionLevelName));
}

void FFPSStageFlowManager::SetStageTransitionCinematicMode(bool bEnable)
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(&Owner, 0))
	{
		PlayerController->SetCinematicMode(bEnable, false, true, true, true);
	}
}

void FFPSStageFlowManager::PrepareStageTransitionCinematicActors()
{
	UWorld* World = Owner.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ATruck> It(World); It; ++It)
	{
		ATruck* Truck = *It;
		if (!IsValid(Truck))
		{
			continue;
		}

		Truck->SetActorHiddenInGame(false);

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Truck->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent))
			{
				continue;
			}

			PrimitiveComponent->SetHiddenInGame(false, true);
			PrimitiveComponent->SetVisibility(true, true);
		}
	}

	if (AFPSBaseCharacter* LocalCharacter = Owner.MyPlayer)
	{
		FPSStage2WorldUtils::RestoreNetworkCharacterVisibility(LocalCharacter);
	}

	TArray<TPair<uint64, AFPSBaseCharacter*>> RegisteredPlayers;
	Owner.GetValidRegisteredPlayers(RegisteredPlayers);
	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : RegisteredPlayers)
	{
		FPSStage2WorldUtils::RestoreNetworkCharacterVisibility(PlayerEntry.Value);
	}
}
