#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieFallZone.generated.h"

class AActor;
class ABaseZombie;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
struct FHitResult;

UCLASS(Blueprintable)
class FPSPROJECT_API AZombieFallZone : public AActor
{
	GENERATED_BODY()

public:
	AZombieFallZone();

	static void GetRegisteredFallZones(UWorld* World, TArray<AZombieFallZone*>& OutZones);

	UFUNCTION(BlueprintCallable, Category = "Zombie|Fall Zone")
	bool CanGuideZombieTowardTarget(
		ABaseZombie* Zombie,
		const AActor* TargetActor,
		FVector& OutApproachLocation,
		FVector& OutCommitLocation,
		float& OutScore);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	TObjectPtr<UBoxComponent> ZoneVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	TObjectPtr<UArrowComponent> DropDirection = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MinTargetDropHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxTargetDropHeight = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MinTargetDistance2D = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxTargetDistance2D = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxZombieDistance2D = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingHalfAngleDegrees = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	bool bRequireTargetInFront = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	bool bBlendTowardDropDirection = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropDirectionBlendAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float FallTargetOvershootDistance = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float ApproachDistanceInsideZone = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float SlotSpacing = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float SlotLateralPadding = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float OccupiedSlotScorePenalty = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float ExistingSlotScoreBonus = 250.0f;

private:
	void RegisterInitialOverlappingZombies();
	void CleanupInvalidZombieAssignments();
	int32 GetSlotCount(float LateralHalfWidth) const;
	float GetSlotOffset(int32 SlotIndex, int32 SlotCount, float LateralHalfWidth) const;
	int32 CountZombiesAssignedToSlot(int32 SlotIndex) const;
	int32 GetOrAssignSlotIndex(ABaseZombie* Zombie, float LateralHalfWidth, const FVector& ZombieLocation);

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<ABaseZombie>, int32> ZombieSlotAssignments;

	UFUNCTION()
	void HandleZoneBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleZoneEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};
