#include "Stage2/Stage2FinalDoor.h"
#include "Components/Button.h"
#include "Components/InteractTriggerComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "ClientPacketHandler.h"
#include "FPSProjectGameInstance.h"
#include "HUD/InteractUIClass.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Protocol.pb.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
UButton* FindEndingExitButton(UUserWidget* EndingWidget)
{
	if (EndingWidget == nullptr || EndingWidget->WidgetTree == nullptr)
	{
		return nullptr;
	}

	static const FName ExitButtonNames[] =
	{
		TEXT("EndB"),
		TEXT("ExitButton"),
		TEXT("QuitButton"),
		TEXT("LeaveButton"),
		TEXT("ExitB"),
		TEXT("QuitB")
	};

	for (const FName& ButtonName : ExitButtonNames)
	{
		if (UButton* Button = Cast<UButton>(EndingWidget->WidgetTree->FindWidget(ButtonName)))
		{
			return Button;
		}
	}

	TArray<UWidget*> Widgets;
	EndingWidget->WidgetTree->GetAllWidgets(Widgets);

	UButton* OnlyButton = nullptr;
	int32 ButtonCount = 0;
	for (UWidget* Widget : Widgets)
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			OnlyButton = Button;
			++ButtonCount;
		}
	}

	return ButtonCount == 1 ? OnlyButton : nullptr;
}

UTextBlock* FindGameClearSurvivorTextBlock(UUserWidget* EndingWidget)
{
	if (EndingWidget == nullptr || EndingWidget->WidgetTree == nullptr)
	{
		return nullptr;
	}

	static const FName SurvivorTextNames[] =
	{
		TEXT("SurvivorNameText"),
		TEXT("SurvivorNamesText"),
		TEXT("SurvivorText"),
		TEXT("SurvivorListText"),
		TEXT("AlivePlayerText"),
		TEXT("AlivePlayersText"),
		TEXT("AlivePlayerNameText"),
		TEXT("PlayerNameText"),
		TEXT("PlayerNamesText"),
		TEXT("PlayerListText"),
		TEXT("NameText"),
		TEXT("NameListText")
	};

	for (const FName& TextName : SurvivorTextNames)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(EndingWidget->WidgetTree->FindWidget(TextName)))
		{
			return TextBlock;
		}
	}

	TArray<UWidget*> Widgets;
	EndingWidget->WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
		if (TextBlock && TextBlock->GetText().ToString().Contains(TEXT("살아남은 플레이어")))
		{
			return TextBlock;
		}
	}

	return nullptr;
}
}

AStage2FinalDoor::AStage2FinalDoor()
{
	bQuitGameAfterCinematic = true;
	NoSequenceFallbackDelay = 1.0f;
	FinalDoorInteractText = FText::FromString(TEXT("\uBB38 \uC5F4\uAE30"));

	static ConstructorHelpers::FClassFinder<UUserWidget> GameOverWidgetFinder(TEXT("/Game/HUD/WBP_GameOver"));
	if (GameOverWidgetFinder.Succeeded())
	{
		GameOverWidgetClass = GameOverWidgetFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> GameClearWidgetFinder(TEXT("/Game/HUD/WBP_GameClear"));
	if (GameClearWidgetFinder.Succeeded())
	{
		GameClearWidgetClass = GameClearWidgetFinder.Class;
	}
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
	if (bEndingTriggered || bEndingStartRequested)
	{
		return;
	}

	if (bEnableEndingOnInteract)
	{
		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(Character->GetGameInstance()))
		{
			if (GameInstance->ShouldUseLocalInteractionFallback() || NetworkDoorId == 0)
			{
				StartEndingSequence();
				return;
			}
		}

		Protocol::C_TOGGLE_DOOR ToggleDoorPkt;
		ToggleDoorPkt.set_door_id(NetworkDoorId);
		bEndingStartRequested = true;
		SEND_PACKET(ToggleDoorPkt);
		return;
	}

	Super::Interact_Implementation(Character);
}

void AStage2FinalDoor::ApplyDoorState(bool bShouldOpen)
{
	const bool bWasOpen = bOpen;

	if (bEnableEndingOnInteract)
	{
		bOpen = bShouldOpen;
		Target = OriginalRotation;
		SetActorTickEnabled(false);
		UpdateFinalDoorInteractText();
		OnFinalDoorStateChanged(bOpen);

		if (!bWasOpen && bShouldOpen && !bEndingTriggered)
		{
			TriggerEndingSequence();
		}
		return;
	}

	Super::ApplyDoorState(bShouldOpen);

	OnFinalDoorStateChanged(bOpen);
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

bool AStage2FinalDoor::TryPlayPlacedEndingSequence(ULevelSequence* SequenceToPlay)
{
	if (SequenceToPlay == nullptr)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	ALevelSequenceActor* FallbackSequenceActor = nullptr;
	ULevel* DoorLevel = GetLevel();
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		ALevelSequenceActor* SequenceActor = *It;
		if (!IsValid(SequenceActor))
		{
			continue;
		}

		ULevelSequence* ActorSequence = Cast<ULevelSequence>(SequenceActor->GetSequence());
		if (ActorSequence != SequenceToPlay)
		{
			continue;
		}

		if (SequenceActor->GetLevel() == DoorLevel)
		{
			EndingSequenceActor = SequenceActor;
			break;
		}

		if (FallbackSequenceActor == nullptr)
		{
			FallbackSequenceActor = SequenceActor;
		}
	}

	if (EndingSequenceActor == nullptr)
	{
		EndingSequenceActor = FallbackSequenceActor;
	}

	if (EndingSequenceActor == nullptr)
	{
		return false;
	}

	ConfigureEndingSequenceActor(EndingSequenceActor.Get());

	EndingSequencePlayer = EndingSequenceActor->GetSequencePlayer();
	if (EndingSequencePlayer == nullptr)
	{
		return false;
	}

	bDestroyEndingSequenceActorOnFinish = false;
	EndingSequencePlayer->OnFinished.RemoveDynamic(this, &AStage2FinalDoor::HandleEndingSequenceFinished);
	EndingSequencePlayer->OnFinished.AddDynamic(this, &AStage2FinalDoor::HandleEndingSequenceFinished);
	EndingSequencePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(0.0f, EUpdatePositionMethod::Jump));
	EndingSequencePlayer->Play();

	UE_LOG(LogTemp, Log, TEXT("[Stage2FinalDoor] Playing placed ending sequence actor %s in level %s."),
		*GetNameSafe(EndingSequenceActor.Get()),
		*GetNameSafe(DoorLevel));

	return true;
}

bool AStage2FinalDoor::TryGetRuntimeLevelTransform(FTransform& OutLevelTransform) const
{
	OutLevelTransform = FTransform::Identity;

	ULevel* DoorLevel = GetLevel();
	if (DoorLevel == nullptr)
	{
		return false;
	}

	if (ULevelStreaming* StreamingLevel = ULevelStreaming::FindStreamingLevel(DoorLevel))
	{
		OutLevelTransform = StreamingLevel->LevelTransform;
		return true;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel && StreamingLevel->GetLoadedLevel() == DoorLevel)
		{
			OutLevelTransform = StreamingLevel->LevelTransform;
			return true;
		}
	}

	return false;
}

void AStage2FinalDoor::ConfigureEndingSequenceActor(ALevelSequenceActor* SequenceActor)
{
	if (!bUseRuntimeLevelTransformOrigin || SequenceActor == nullptr)
	{
		return;
	}

	FTransform RuntimeLevelTransform;
	if (!TryGetRuntimeLevelTransform(RuntimeLevelTransform))
	{
		return;
	}

	UDefaultLevelSequenceInstanceData* InstanceData =
		Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData);
	if (InstanceData == nullptr)
	{
		InstanceData = NewObject<UDefaultLevelSequenceInstanceData>(SequenceActor, TEXT("RuntimeEndingInstanceData"));
		SequenceActor->DefaultInstanceData = InstanceData;
	}

	InstanceData->TransformOriginActor = nullptr;
	InstanceData->TransformOrigin = RuntimeLevelTransform;
	SequenceActor->bOverrideInstanceData = true;

	UE_LOG(LogTemp, Log, TEXT("[Stage2FinalDoor] Ending sequence transform origin set to %s for %s."),
		*RuntimeLevelTransform.ToHumanReadableString(),
		*GetNameSafe(SequenceActor));
}

void AStage2FinalDoor::TriggerEndingSequence()
{
	bEndingStartRequested = true;
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

	if (TryPlayPlacedEndingSequence(SequenceToPlay))
	{
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
	bDestroyEndingSequenceActorOnFinish = true;

	ConfigureEndingSequenceActor(EndingSequenceActor.Get());

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
	EndingSequencePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(0.0f, EUpdatePositionMethod::Jump));
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

	if (EndingSequenceActor && bDestroyEndingSequenceActorOnFinish)
	{
		EndingSequenceActor->Destroy();
	}

	EndingSequenceActor = nullptr;
	bDestroyEndingSequenceActorOnFinish = false;

	OnEndingSequenceFinished();

	if (!LevelNameAfterCinematic.IsNone())
	{
		UGameplayStatics::OpenLevel(this, LevelNameAfterCinematic);
		return;
	}

	if (bQuitGameAfterCinematic)
	{
		if (ShowGameClearScreen())
		{
			return;
		}

		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		{
			GameInstance->QuitGame();
			return;
		}
	}

	SetEndingCinematicMode(false);
}

bool AStage2FinalDoor::ShowGameClearScreen()
{
	SetEndingCinematicMode(false);

	if (GameClearWidget && GameClearWidget->IsInViewport())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || GameClearWidgetClass == nullptr)
	{
		return false;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (PlayerController == nullptr)
	{
		return false;
	}

	GameClearWidget = CreateWidget<UUserWidget>(PlayerController, GameClearWidgetClass);
	if (GameClearWidget == nullptr)
	{
		return false;
	}

	GameClearWidget->AddToViewport(1000);
	ApplyGameClearSurvivorNames();
	BindGameClearExitButton();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameClearWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);

	return true;
}

void AStage2FinalDoor::ApplyGameClearSurvivorNames()
{
	if (GameClearWidget == nullptr)
	{
		return;
	}

	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	if (GameInstance == nullptr)
	{
		return;
	}

	TArray<FString> SurvivorNames;
	GameInstance->GetSurvivingPlayerNicknames(SurvivorNames);
	if (SurvivorNames.Num() <= 0)
	{
		return;
	}

	UTextBlock* SurvivorTextBlock = FindGameClearSurvivorTextBlock(GameClearWidget);
	if (SurvivorTextBlock == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2FinalDoor: survivor name text block was not found in GameClear widget."));
		return;
	}

	const FString JoinedNames = FString::Join(SurvivorNames, TEXT(" / "));
	const FString CurrentText = SurvivorTextBlock->GetText().ToString();
	const bool bTextBlockHasLabel = CurrentText.Contains(TEXT("살아남은 플레이어"));
	const FString DisplayText = bTextBlockHasLabel
		? FString::Printf(TEXT("살아남은 플레이어\n%s"), *JoinedNames)
		: JoinedNames;

	SurvivorTextBlock->SetText(FText::FromString(DisplayText));
}

void AStage2FinalDoor::BindGameClearExitButton()
{
	UButton* ExitButton = FindEndingExitButton(GameClearWidget);
	if (ExitButton == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage2FinalDoor: GameClear exit button was not found."));
		return;
	}

	ExitButton->OnClicked.AddUniqueDynamic(this, &AStage2FinalDoor::OnGameClearExitClicked);
}

void AStage2FinalDoor::OnGameClearExitClicked()
{
	if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
	{
		GameInstance->QuitGame();
		return;
	}

	UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
}

void AStage2FinalDoor::SetEndingCinematicMode(bool bEnable)
{
	if (!bUseCinematicMode)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetCinematicMode(bEnable, true, true, true, true);
	}
}
