#pragma once

#include "CoreMinimal.h"
#include "ADoor.h"
#include "Stage2FinalDoor.generated.h"

class ALevelSequenceActor;
class ULevelSequence;
class ULevelSequencePlayer;
class UUserWidget;

UCLASS()
class FPSPROJECT_API AStage2FinalDoor : public AADoor
{
	GENERATED_BODY()

public:
	AStage2FinalDoor();

	virtual void Interact_Implementation(AFPSBaseCharacter* Character) override;
	virtual void ApplyDoorState(bool bShouldOpen) override;

	virtual void WidgetStart(AActor* OtherActor) override;

	virtual void WidgetEnd(AActor* OtherActor) override;

	UFUNCTION(BlueprintCallable, Category = "Ending")
	void StartEndingSequence();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleEndingSequenceFinished();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void OnEndingSequenceStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void OnEndingSequenceFinished();

	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnFinalDoorStateChanged(bool bIsOpen);

	bool SetFinalDoorInteractText(UUserWidget* Widget) const;
	bool UpdateFinalDoorInteractText();
	void PlayFinalDoorInteractWidgets();
	ULevelSequence* ResolveEndingSequence();
	bool TryPlayPlacedEndingSequence(ULevelSequence* SequenceToPlay);
	bool TryGetRuntimeLevelTransform(FTransform& OutLevelTransform) const;
	void ConfigureEndingSequenceActor(ALevelSequenceActor* SequenceActor);
	void TriggerEndingSequence();
	void FinishEndingSequence();
	bool ShowGameClearScreen();
	void BindGameClearExitButton();
	void SetEndingCinematicMode(bool bEnable);

	UFUNCTION()
	void OnGameClearExitClicked();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	TObjectPtr<ULevelSequence> EndingSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	bool bEnableEndingOnInteract = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText FinalDoorInteractText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	FName LevelNameAfterCinematic = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	bool bQuitGameAfterCinematic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending|UI")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending|UI")
	TSubclassOf<UUserWidget> GameClearWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GameClearWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	bool bDisableInteractionAfterTriggered = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	bool bUseCinematicMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	bool bUseRuntimeLevelTransformOrigin = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending", meta = (ClampMin = "0.1"))
	float NoSequenceFallbackDelay = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ending")
	bool bEndingTriggered = false;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> EndingSequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> EndingSequenceActor;

	bool bDestroyEndingSequenceActorOnFinish = false;

	FTimerHandle EndingFallbackTimerHandle;
};
