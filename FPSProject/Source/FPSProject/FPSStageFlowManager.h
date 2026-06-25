#pragma once

#include "CoreMinimal.h"
#include "Protocol.pb.h"

class UUserWidget;
class UFPSProjectGameInstance;
class ALevelSequenceActor;
class ULevelSequencePlayer;

class FFPSStageFlowManager
{
public:
	explicit FFPSStageFlowManager(UFPSProjectGameInstance& InOwner);

	bool ShouldDelayEnterGameRequest() const;
	void RequestEnterGameWhenReady();
	bool TrySendEnterGamePacket();
	void RefreshStage2StartupActorHold();
	void SetEntryLoadingWidgetClass(TSubclassOf<UUserWidget> WidgetClass);
	void ShowEntryLoadingWidget();
	void RegisterEntryLoadingWidget(UUserWidget* Widget);
	void RemoveEntryLoadingWidget();
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void CompleteStage2MapLoad();
	void ApplyEntryLoadingReadyCount(int32 ReadyCount);
	void ProcessPendingStage2Spawns();
	void TryDistributeStage1CargoItemsToPlayers();
	void HandleStageTimer(const Protocol::S_STAGE_TIMER& Pkt);
	void HandleStage1ItemSeed(const Protocol::S_STAGE1_ITEM_SEED& Pkt);
	void HandleStageTransition(const Protocol::S_STAGE_TRANSITION& Pkt);
	void TickStageFlow();
	void ApplyStageTimerToLocalUI();

private:
	void ApplyStage1ItemSpawnSeed();
	bool TryPlayStageTransitionCinematic();
	void FinishStageTransitionCinematic();
	void OpenPendingStageTransitionLevel();
	void SetStageTransitionCinematicMode(bool bEnable);
	void PrepareStageTransitionCinematicActors();

	UFPSProjectGameInstance& Owner;
	bool bPendingEnterGameRequest = false;
	bool bEnterGamePacketSent = false;
	bool bShouldShowEntryLoadingWidget = false;
	bool bWaitingForStage2MapLoad = false;
	bool bStageTransitionCinematicPlaying = false;
	FString PendingStageTransitionLevelName;
	FTimerHandle StageTransitionCinematicTimerHandle;
	TWeakObjectPtr<ALevelSequenceActor> StageTransitionSequenceActor;
	TWeakObjectPtr<ULevelSequencePlayer> StageTransitionSequencePlayer;
	int32 CachedEntryLoadingReadyCount = 0;
	int32 CachedStageTimerRemainingSeconds = INDEX_NONE;
	uint32 CachedStage1ItemSpawnSeed = 0;
	bool bHasStage1ItemSpawnSeed = false;
	bool bHasAppliedStage1ItemSpawns = false;
	bool bHasDistributedStage1CargoItems = false;
};
