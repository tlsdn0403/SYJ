#include "Zombie/ZombieDropNavLinkProxy.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Zombie/BaseZombie.h"

AZombieDropNavLinkProxy::AZombieDropNavLinkProxy()
{
	bSmartLinkIsRelevant = true;
}

void AZombieDropNavLinkProxy::BeginPlay()
{
	Super::BeginPlay();

	SetSmartLinkEnabled(true);
	OnSmartLinkReached.AddDynamic(this, &AZombieDropNavLinkProxy::HandleSmartLinkReached);
}

void AZombieDropNavLinkProxy::HandleSmartLinkReached(AActor* MovingActor, const FVector& DestinationPoint)
{
	if (!MovingActor)
	{
		return;
	}

	if (bOnlyAffectZombies && !MovingActor->IsA<ABaseZombie>())
	{
		ResumePathFollowing(MovingActor);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(MovingActor);
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !MovementComponent)
	{
		ResumePathFollowing(MovingActor);
		return;
	}

	const FVector CurrentLocation = Character->GetActorLocation();
	const FVector DeltaToDestination = DestinationPoint - CurrentLocation;
	const FVector Direction2D = FVector(DeltaToDestination.X, DeltaToDestination.Y, 0.0f).GetSafeNormal();
	const bool bDestinationLower = DeltaToDestination.Z <= -DropHeightThreshold;

	FVector LaunchVelocity = Direction2D * TraverseForwardSpeed;
	if (!bDestinationLower)
	{
		LaunchVelocity.Z = TraverseUpwardSpeed;
	}

	MovementComponent->SetMovementMode(MOVE_Falling);
	Character->LaunchCharacter(LaunchVelocity, true, true);

	const float ResumeDelay = FMath::Clamp(
		FMath::Abs(DeltaToDestination.Z) / 900.0f,
		MinResumePathDelay,
		MaxResumePathDelay);

	FTimerDelegate ResumeDelegate;
	ResumeDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AZombieDropNavLinkProxy, ResumeAgentPathFollowing), MovingActor);
	FTimerHandle ResumeTimerHandle;
	GetWorldTimerManager().SetTimer(ResumeTimerHandle, ResumeDelegate, ResumeDelay, false);
}

void AZombieDropNavLinkProxy::ResumeAgentPathFollowing(AActor* MovingActor)
{
	ResumePathFollowing(MovingActor);
}
