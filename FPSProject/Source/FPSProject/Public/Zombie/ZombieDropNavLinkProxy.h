#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavLinkProxy.h"
#include "ZombieDropNavLinkProxy.generated.h"

UCLASS(Blueprintable)
class FPSPROJECT_API AZombieDropNavLinkProxy : public ANavLinkProxy
{
	GENERATED_BODY()

public:
	AZombieDropNavLinkProxy();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|NavLink")
	bool bOnlyAffectZombies = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|NavLink", meta = (ClampMin = "0.0"))
	float TraverseForwardSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|NavLink", meta = (ClampMin = "0.0"))
	float TraverseUpwardSpeed = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|NavLink", meta = (ClampMin = "0.0"))
	float DropHeightThreshold = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|NavLink", meta = (ClampMin = "0.05"))
	float MinResumePathDelay = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|NavLink", meta = (ClampMin = "0.05"))
	float MaxResumePathDelay = 1.00f;

	UFUNCTION()
	void HandleSmartLinkReached(AActor* MovingActor, const FVector& DestinationPoint);

	UFUNCTION()
	void ResumeAgentPathFollowing(AActor* MovingActor);
};
