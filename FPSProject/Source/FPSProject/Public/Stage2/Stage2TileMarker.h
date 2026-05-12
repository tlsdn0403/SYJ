#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage2TileMarker.generated.h"

class AActor;
class UArrowComponent;
class UBoxComponent;
class USceneComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EStage2TileType : uint8
{
	Start UMETA(DisplayName = "Start"),
	Straight UMETA(DisplayName = "Straight"),
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	Goal UMETA(DisplayName = "Goal")
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStage2TileTriggerSignature, AStage2TileMarker*, TileMarker, AActor*, TriggeringActor);

UCLASS(Blueprintable)
class FPSPROJECT_API AStage2TileMarker : public AActor
{
	GENERATED_BODY()

public:
	AStage2TileMarker();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage2")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage2")
	UArrowComponent* EntryArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage2")
	UArrowComponent* ExitArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage2")
	UBoxComponent* NextTileTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage2")
	USceneComponent* ZombieSpawnRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2")
	EStage2TileType TileType = EStage2TileType::Straight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage2")
	bool bTriggerOnlyOnce = true;

	UPROPERTY(BlueprintAssignable, Category = "Stage2")
	FStage2TileTriggerSignature OnNextTileTriggerEntered;

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	FTransform GetEntryTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	FTransform GetExitTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	FTransform GetNextTileSpawnTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Stage2|Zombie")
	TArray<FTransform> GetZombieSpawnTransforms(bool bIncludeRootIfNoChildren = false) const;

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	void ResetNextTileTrigger();

	UFUNCTION(BlueprintCallable, Category = "Stage2")
	void SetNextTileTriggerEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleNextTileTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	bool bHasTriggeredNextTile = false;
};
