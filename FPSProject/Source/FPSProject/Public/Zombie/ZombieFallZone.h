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

	UFUNCTION(BlueprintCallable, Category = "Zombie|Fall Zone")
	bool CanGuideZombieTowardTarget(
		const ABaseZombie* Zombie,
		const AActor* TargetActor,
		FVector& OutTargetLocation,
		float& OutScore) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	TObjectPtr<UBoxComponent> ZoneVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	TObjectPtr<UArrowComponent> DropDirection = nullptr;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MinTargetDropHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxTargetDropHeight = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MinTargetDistance2D = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0"))
	float MaxTargetDistance2D = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingHalfAngleDegrees = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	bool bRequireTargetInFront = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone")
	bool bBlendTowardDropDirection = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fall Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropDirectionBlendAlpha = 0.35f;

private:
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
