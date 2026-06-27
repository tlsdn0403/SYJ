#include "Stage2/Stage2FinalDoor.h"
#include "Components/InteractTriggerComponent.h"
#include "Components/WidgetComponent.h"
#include "FPSProjectGameInstance.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Kismet/GameplayStatics.h"

AStage2FinalDoor::AStage2FinalDoor()
{
	bQuitGameAfterCinematic = true;
	NoSequenceFallbackDelay = 1.0f;
}

void AStage2FinalDoor::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (bEndingTriggered)
	{
		return;
	}

	Super::Interact_Implementation(Character);
}

void AStage2FinalDoor::ApplyDoorState(bool bShouldOpen)
{
	const bool bWasOpen = bOpen;
	Super::ApplyDoorState(bShouldOpen);
	OnFinalDoorStateChanged(bOpen);

	if (!bWasOpen && bShouldOpen && !bEndingTriggered)
	{
		TriggerEndingSequence();
	}
}

void AStage2FinalDoor::StartEndingSequence()
{
	if (!bEndingTriggered)
	{
		TriggerEndingSequence();
	}
}

void AStage2FinalDoor::TriggerEndingSequence()
{
	bEndingTriggered = true;

	if (bDisableInteractionAfterTriggered)
	{
		if (InteractTrigger)
		{
			InteractTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (WidgetComp)
		{
			WidgetComp->SetVisibility(false);
		}
	}

	SetEndingCinematicMode(true);
	OnEndingSequenceStarted();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		FinishEndingSequence();
		return;
	}

	if (EndingSequence == nullptr)
	{
		World->GetTimerManager().SetTimer(
			EndingFallbackTimerHandle,
			this,
			&AStage2FinalDoor::FinishEndingSequence,
			NoSequenceFallbackDelay,
			false);
		return;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;

	ALevelSequenceActor* CreatedSequenceActor = nullptr;
	EndingSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		World,
		EndingSequence,
		PlaybackSettings,
		CreatedSequenceActor);
	EndingSequenceActor = CreatedSequenceActor;

	if (EndingSequencePlayer == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage2FinalDoor] Failed to create ending sequence player."));
		World->GetTimerManager().SetTimer(
			EndingFallbackTimerHandle,
			this,
			&AStage2FinalDoor::FinishEndingSequence,
			NoSequenceFallbackDelay,
			false);
		return;
	}

	EndingSequencePlayer->OnFinished.AddDynamic(this, &AStage2FinalDoor::HandleEndingSequenceFinished);
	EndingSequencePlayer->Play();
}

void AStage2FinalDoor::HandleEndingSequenceFinished()
{
	FinishEndingSequence();
}

void AStage2FinalDoor::FinishEndingSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndingFallbackTimerHandle);
	}

	if (EndingSequencePlayer)
	{
		EndingSequencePlayer->OnFinished.RemoveDynamic(this, &AStage2FinalDoor::HandleEndingSequenceFinished);
		EndingSequencePlayer->Stop();
		EndingSequencePlayer = nullptr;
	}

	if (EndingSequenceActor)
	{
		EndingSequenceActor->Destroy();
		EndingSequenceActor = nullptr;
	}

	OnEndingSequenceFinished();

	if (!LevelNameAfterCinematic.IsNone())
	{
		UGameplayStatics::OpenLevel(this, LevelNameAfterCinematic);
		return;
	}

	if (bQuitGameAfterCinematic)
	{
		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		{
			GameInstance->QuitGame();
			return;
		}
	}

	SetEndingCinematicMode(false);
}

void AStage2FinalDoor::SetEndingCinematicMode(bool bEnable)
{
	if (!bUseCinematicMode)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetCinematicMode(bEnable, false, true, true, true);
	}
}
