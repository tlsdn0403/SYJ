#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieFallZone.generated.h"

class AActor;
class ABaseZombie;
class UArrowComponent;
class UBoxComponent;
class USceneComponent;

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

	// Only allow drop guidance when the zombie is meaningfully above the target.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MinTargetDropHeight = 150.0f;

	// Cap the drop height so unusably tall falls are ignored.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxTargetDropHeight = 3000.0f;

	// Ignore targets that are too close or too far for a useful fall route.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MinTargetDistance2D = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxTargetDistance2D = 12000.0f;

	// A zombie that is too far from this zone should keep using normal pursuit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxZombieDistance2D = 3500.0f;

	// Valid target direction range measured from the zone's forward vector.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingHalfAngleDegrees = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	bool bRequireTargetInFront = true;

	// Blend the approach point between the zombie's side and the target's side.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	bool bBlendTowardDropDirection = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropDirectionBlendAlpha = 0.35f;

	// Push the commit point slightly past the box so the drop reliably starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float FallTargetOvershootDistance = 140.0f;

	// Keep the approach point inside the box so the zombie has room to align first.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float ApproachDistanceInsideZone = 90.0f;

	// Reserve a small edge margin so the approach point stays inside the volume.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float SlotLateralPadding = 20.0f;
};
