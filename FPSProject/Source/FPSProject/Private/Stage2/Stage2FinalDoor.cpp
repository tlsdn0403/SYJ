#include "Stage2/Stage2FinalDoor.h"
#include "Components/InteractTriggerComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "FPSProjectGameInstance.h"
#include "HUD/InteractUIClass.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Kismet/GameplayStatics.h"

AStage2FinalDoor::AStage2FinalDoor()
{
	bQuitGameAfterCinematic = true;
	NoSequenceFallbackDelay = 1.0f;
	FinalDoorInteractText = FText::FromString(TEXT("\uBB38 \uC5F4\uAE30"));
}

void AStage2FinalDoor::BeginPlay()
{
	Super::BeginPlay();

	if (bEnableEndingOnInteract)
	{
		if (InteractTrigger)
		{
			InteractTrigger->OnEnter.AddUniqueDynamic(this, &AStage2FinalDoor::WidgetStart);
			InteractTrigger->OnExit.AddUniqueDynamic(this, &AStage2FinalDoor::WidgetEnd);
		}

		UpdateFinalDoorInteractText();
	}
}

void AStage2FinalDoor::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (bEndingTriggered)
	{
		return;
	}

	Super::Interact_Implementation(Character);

	if (bEnableEndingOnInteract)
	{
		StartEndingSequence();
	}
}

void AStage2FinalDoor::ApplyDoorState(bool bShouldOpen)
{
	const bool bWasOpen = bOpen;
	Super::ApplyDoorState(bShouldOpen);

	if (bEnableEndingOnInteract)
	{
		UpdateFinalDoorInteractText();
	}

	OnFinalDoorStateChanged(bOpen);

	if (bEnableEndingOnInteract && !bWasOpen && bShouldOpen && !bEndingTriggered)
	{
		TriggerEndingSequence();
	}
}

void AStage2FinalDoor::WidgetStart(AActor* OtherActor)
{
	Super::WidgetStart(OtherActor);

	if (bEnableEndingOnInteract && Cast<AFPSBaseCharacter>(OtherActor))
	{
		PlayFinalDoorInteractWidgets();
	}
}

void AStage2FinalDoor::WidgetEnd(AActor* OtherActor)
{
	Super::WidgetEnd(OtherActor);
}

void AStage2FinalDoor::StartEndingSequence()
{
	if (!bEndingTriggered)
	{
		TriggerEndingSequence();
	}
}

bool AStage2FinalDoor::SetFinalDoorInteractText(UUserWidget* Widget) const
{
	if (Widget == nullptr)
	{
		return false;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(Widget))
	{
		UI->SetInteractText(FinalDoorInteractText);
		return true;
	}

	if (Widget->WidgetTree)
	{
		if (UTextBlock* InteractText = Widget->WidgetTree->FindWidget<UTextBlock>(TEXT("InteractText")))
		{
			InteractText->SetText(FinalDoorInteractText);
			return true;
		}
	}

	return false;
}

bool AStage2FinalDoor::UpdateFinalDoorInteractText()
{
	bool bUpdatedAnyWidget = false;

	TArray<UWidgetComponent*> WidgetComponents;
	GetComponents<UWidgetComponent>(WidgetComponents);
	if (WidgetComp && !WidgetComponents.Contains(WidgetComp))
	{
		WidgetComponents.Add(WidgetComp);
	}

	for (UWidgetComponent* CandidateWidgetComp : WidgetComponents)
	{
		if (CandidateWidgetComp == nullptr)
		{
			continue;
		}

		if (CandidateWidgetComp->GetUserWidgetObject() == nullptr)
		{
			CandidateWidgetComp->InitWidget();
		}

		if (SetFinalDoorInteractText(CandidateWidgetComp->GetUserWidgetObject()))
		{
			bUpdatedAnyWidget = true;
		}
	}

	return bUpdatedAnyWidget;
}

void AStage2FinalDoor::PlayFinalDoorInteractWidgets()
{
	TArray<UWidgetComponent*> WidgetComponents;
	GetComponents<UWidgetComponent>(WidgetComponents);
	if (WidgetComp && !WidgetComponents.Contains(WidgetComp))
	{
		WidgetComponents.Add(WidgetComp);
	}

	for (UWidgetComponent* CandidateWidgetComp : WidgetComponents)
	{
		if (CandidateWidgetComp == nullptr)
		{
			continue;
		}

		CandidateWidgetComp->SetVisibility(true);
		CandidateWidgetComp->SetHiddenInGame(false);
		if (CandidateWidgetComp->GetUserWidgetObject() == nullptr)
		{
			CandidateWidgetComp->InitWidget();
		}

		UUserWidget* UserWidget = CandidateWidgetComp->GetUserWidgetObject();
		SetFinalDoorInteractText(UserWidget);
		if (UInteractUIClass* UI = Cast<UInteractUIClass>(UserWidget))
		{
			UI->PlayAni_PopUp(false);
		}
	}
}

ULevelSequence* AStage2FinalDoor::ResolveEndingSequence()
{
	if (EndingSequence)
	{
		return EndingSequence;
	}

	EndingSequence = LoadObject<ULevelSequence>(
		nullptr,
		TEXT("/Game/Maps/map_level2/LS_Ending.LS_Ending"));
	return EndingSequence;
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

	ULevelSequence* SequenceToPlay = ResolveEndingSequence();
	if (SequenceToPlay == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage2FinalDoor] Failed to load ending sequence: /Game/Maps/map_level2/LS_Ending.LS_Ending"));
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
		SequenceToPlay,
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
