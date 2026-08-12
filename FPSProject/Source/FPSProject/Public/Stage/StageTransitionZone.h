#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageTransitionZone.generated.h"

class ATruck;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class FPSPROJECT_API AStageTransitionZone : public AActor
{
	GENERATED_BODY()

public:
	AStageTransitionZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Transition")
	TObjectPtr<UBoxComponent> TransitionBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Transition")
	FName TargetLevelName = TEXT("/Game/Maps/map_level2/0812_NEWMAP_Ba");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Transition")
	bool bTriggerOnce = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Transition")
	bool bRequireLoadingPhaseFinished = true;

	UFUNCTION(BlueprintCallable, Category = "Stage Transition")
	void TravelToTargetLevel(ATruck* TriggerTruck);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTransitionBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	bool bHasTriggered = false;
};
