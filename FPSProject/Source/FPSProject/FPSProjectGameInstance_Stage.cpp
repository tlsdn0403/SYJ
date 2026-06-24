#include "FPSProjectGameInstance.h"
#include "FPSStageFlowManager.h"

bool UFPSProjectGameInstance::ShouldDelayEnterGameRequest() const
{
	return StageFlowManager && StageFlowManager->ShouldDelayEnterGameRequest();
}

void UFPSProjectGameInstance::RequestEnterGameWhenReady()
{
	if (StageFlowManager)
	{
		StageFlowManager->RequestEnterGameWhenReady();
	}
}

bool UFPSProjectGameInstance::TrySendEnterGamePacket()
{
	return StageFlowManager && StageFlowManager->TrySendEnterGamePacket();
}

void UFPSProjectGameInstance::RefreshStage2StartupActorHold()
{
	if (StageFlowManager)
	{
		StageFlowManager->RefreshStage2StartupActorHold();
	}
}

void UFPSProjectGameInstance::SetEntryLoadingWidgetClass(TSubclassOf<UUserWidget> WidgetClass)
{
	if (StageFlowManager)
	{
		StageFlowManager->SetEntryLoadingWidgetClass(WidgetClass);
	}
}

void UFPSProjectGameInstance::ShowEntryLoadingWidget()
{
	if (StageFlowManager)
	{
		StageFlowManager->ShowEntryLoadingWidget();
	}
}

void UFPSProjectGameInstance::RegisterEntryLoadingWidget(UUserWidget* Widget)
{
	if (StageFlowManager)
	{
		StageFlowManager->RegisterEntryLoadingWidget(Widget);
	}
}

void UFPSProjectGameInstance::RemoveEntryLoadingWidget()
{
	if (StageFlowManager)
	{
		StageFlowManager->RemoveEntryLoadingWidget();
	}
}

void UFPSProjectGameInstance::ApplyEntryLoadingReadyCount(int32 ReadyCount)
{
	if (StageFlowManager)
	{
		StageFlowManager->ApplyEntryLoadingReadyCount(ReadyCount);
	}
}

void UFPSProjectGameInstance::ApplyStageTimerToLocalUI()
{
	if (StageFlowManager)
	{
		StageFlowManager->ApplyStageTimerToLocalUI();
	}
}

void UFPSProjectGameInstance::ProcessPendingStage2Spawns()
{
	if (StageFlowManager)
	{
		StageFlowManager->ProcessPendingStage2Spawns();
	}
}

void UFPSProjectGameInstance::TryDistributeStage1CargoItemsToPlayers()
{
	if (StageFlowManager)
	{
		StageFlowManager->TryDistributeStage1CargoItemsToPlayers();
	}
}

void UFPSProjectGameInstance::HandleStageTimer(const Protocol::S_STAGE_TIMER& pkt)
{
	if (StageFlowManager)
	{
		StageFlowManager->HandleStageTimer(pkt);
	}
}

void UFPSProjectGameInstance::HandleStage1ItemSeed(const Protocol::S_STAGE1_ITEM_SEED& pkt)
{
	if (StageFlowManager)
	{
		StageFlowManager->HandleStage1ItemSeed(pkt);
	}
}

void UFPSProjectGameInstance::HandleStageTransition(const Protocol::S_STAGE_TRANSITION& pkt)
{
	if (StageFlowManager)
	{
		StageFlowManager->HandleStageTransition(pkt);
	}
}

void UFPSProjectGameInstance::TickStageFlow()
{
	if (StageFlowManager)
	{
		StageFlowManager->TickStageFlow();
	}
}