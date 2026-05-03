// Fill out your copyright notice in the Description page of Project Settings.

#include "Zombie/AIZombieController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Truck/Truck.h"
#include "Zombie/BaseZombie.h"

const FName AAIZombieController::TargetPlayerKey = FName("TargetPlayer");
const FName AAIZombieController::PlayerLocationKey = FName("PlayerLocation");

namespace
{
	AActor* ResolveTargetActorFromPawn(APawn* PlayerPawn)
	{
		AFPSBaseCharacter* PlayerCharacter = Cast<AFPSBaseCharacter>(PlayerPawn);
		if (PlayerCharacter &&
			IsValid(PlayerCharacter->CurrentTruck) &&
			(PlayerCharacter->IsDrivingTruck() ||
				PlayerCharacter->IsOnTruckCargo() ||
				PlayerCharacter->IsUsingMountedWeapon()))
		{
			return PlayerCharacter->CurrentTruck;
		}

		return PlayerPawn;
	}

	FVector GetClosestPointOnTarget(AActor* TargetActor, const FVector& FromLocation)
	{
		if (!TargetActor)
		{
			return FromLocation;
		}

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
		{
			FVector ClosestPoint = TargetActor->GetActorLocation();
			if (PrimitiveComponent->GetClosestPointOnCollision(FromLocation, ClosestPoint) >= 0.0f)
			{
				return ClosestPoint;
			}
		}

		return TargetActor->GetActorLocation();
	}
}

AAIZombieController::AAIZombieController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = ControllerUpdateInterval;

	ZombiePerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ZombiePerception"));
	SetPerceptionComponent(*ZombiePerceptionComponent);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
}

void AAIZombieController::BeginPlay()
{
	Super::BeginPlay();

	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	LastPlayerPawnRefreshTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastPlayerPawnRefreshTime;
	RefreshPerceptionConfig();

	if (ZombiePerceptionComponent)
	{
		ZombiePerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AAIZombieController::HandleTargetPerceptionUpdated);
	}

	if (ZombieBehaviorTree)
	{
		RunBehaviorTree(ZombieBehaviorTree);
	}
}

void AAIZombieController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!BlackboardComponent)
	{
		return;
	}

	if (!IsZombieAlive())
	{
		ClearCurrentTarget(BlackboardComponent);
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (!IsValid(PlayerPawn) || (CurrentTime - LastPlayerPawnRefreshTime) >= PlayerPawnRefreshInterval)
	{
		PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		LastPlayerPawnRefreshTime = CurrentTime;
	}
	AActor* PrimaryTargetActor = ResolvePrimaryTargetActor();

	if (PrimaryTargetActor &&
		(HasActivePerceptionFor(PrimaryTargetActor) || CanForceAwarenessFor(PrimaryTargetActor)))
	{
		RememberTarget(PrimaryTargetActor, PrimaryTargetActor->GetActorLocation());
	}
	else if (CurrentTargetActor.IsValid() &&
		HasActivePerceptionFor(CurrentTargetActor.Get()))
	{
		RememberTarget(CurrentTargetActor.Get(), CurrentTargetActor->GetActorLocation());
	}

	AActor* TargetActor = CurrentTargetActor.Get();
	const bool bHasValidTargetActor = IsValid(TargetActor);
	const bool bCanUseMemory =
		bHasValidTargetActor &&
		bHasKnownTarget &&
		(CurrentTime - LastTargetSeenTime) <= GetMemoryDurationForTarget(TargetActor);

	if (bHasValidTargetActor &&
		(HasActivePerceptionFor(TargetActor) || CanForceAwarenessFor(TargetActor) || bCanUseMemory))
	{
		const FVector TargetLocation = (HasActivePerceptionFor(TargetActor) || CanForceAwarenessFor(TargetActor))
			? TargetActor->GetActorLocation()
			: LastKnownTargetLocation;
		UpdateBlackboardTarget(BlackboardComponent, TargetActor, TargetLocation);
		return;
	}

	ClearCurrentTarget(BlackboardComponent);
}

void AAIZombieController::RefreshPerceptionConfig()
{
	if (!ZombiePerceptionComponent || !SightConfig || !HearingConfig)
	{
		return;
	}

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = SightHalfAngleDegrees;
	SightConfig->AutoSuccessRangeFromLastSeenLocation = SightAutoSuccessRange;
	SightConfig->SetMaxAge(TargetMemoryDuration);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	HearingConfig->HearingRange = HearingRange;
	HearingConfig->SetMaxAge(HearingMemoryDuration);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

	ZombiePerceptionComponent->ConfigureSense(*SightConfig);
	ZombiePerceptionComponent->ConfigureSense(*HearingConfig);
	ZombiePerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	ZombiePerceptionComponent->RequestStimuliListenerUpdate();
}

AActor* AAIZombieController::ResolvePrimaryTargetActor() const
{
	return ResolveTargetActorFromPawn(PlayerPawn);
}

bool AAIZombieController::IsZombieAlive() const
{
	const ABaseZombie* Zombie = Cast<ABaseZombie>(GetPawn());
	return Zombie && Zombie->IsAlive();
}

bool AAIZombieController::HasActivePerceptionFor(AActor* TargetActor) const
{
	if (!ZombiePerceptionComponent || !TargetActor)
	{
		return false;
	}

	FActorPerceptionBlueprintInfo PerceptionInfo;
	ZombiePerceptionComponent->GetActorsPerception(TargetActor, PerceptionInfo);
	for (const FAIStimulus& Stimulus : PerceptionInfo.LastSensedStimuli)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			return true;
		}
	}

	return false;
}

bool AAIZombieController::CanForceAwarenessFor(AActor* TargetActor) const
{
	const APawn* ZombiePawn = GetPawn();
	if (!ZombiePawn || !TargetActor)
	{
		return false;
	}

	const FVector ZombieLocation = ZombiePawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float Distance2D = FVector::Dist2D(ZombieLocation, TargetLocation);
	const float HeightDelta = FMath::Abs(ZombieLocation.Z - TargetLocation.Z);

	if (TargetActor->IsA<ATruck>())
	{
		return Distance2D <= TruckAwarenessDistance;
	}

	return Distance2D <= PlayerAwarenessDistance && HeightDelta <= PlayerAwarenessHeightTolerance;
}

bool AAIZombieController::HasReachableNavigationPathTo(AActor* TargetActor)
{
	if (!bRequireReachableNavigationPath)
	{
		return true;
	}

	APawn* ZombiePawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ZombiePawn || !TargetActor || !World)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CachedReachabilityTargetActor == TargetActor &&
		(CurrentTime - LastReachabilityCheckTime) < NavigationPathCheckInterval)
	{
		return bCachedReachabilityResult;
	}

	bool bReachable = false;

	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		const FVector QueryExtent(250.0f, 250.0f, 400.0f);
		FNavLocation ProjectedStartLocation;
		FNavLocation ProjectedGoalLocation;
		const FVector StartLocation = ZombiePawn->GetActorLocation();
		const FVector GoalLocation = GetClosestPointOnTarget(TargetActor, StartLocation);
		const FNavAgentProperties& AgentProperties = ZombiePawn->GetNavAgentPropertiesRef();

		if (NavigationSystem->ProjectPointToNavigation(StartLocation, ProjectedStartLocation, QueryExtent, &AgentProperties) &&
			NavigationSystem->ProjectPointToNavigation(GoalLocation, ProjectedGoalLocation, QueryExtent, &AgentProperties))
		{
			if (UNavigationPath* NavigationPath = NavigationSystem->FindPathToLocationSynchronously(
				World,
				ProjectedStartLocation.Location,
				ProjectedGoalLocation.Location,
				ZombiePawn))
			{
				bReachable =
					NavigationPath->IsValid() &&
					!NavigationPath->IsPartial() &&
					NavigationPath->PathPoints.Num() > 1;
			}
		}
	}

	CachedReachabilityTargetActor = TargetActor;
	LastReachabilityCheckTime = CurrentTime;
	bCachedReachabilityResult = bReachable;
	return bReachable;
}

float AAIZombieController::GetMemoryDurationForTarget(AActor* TargetActor) const
{
	return TargetActor && TargetActor->IsA<ATruck>() ? TruckTargetMemoryDuration : TargetMemoryDuration;
}

void AAIZombieController::RememberTarget(AActor* TargetActor, const FVector& KnownLocation)
{
	if (!TargetActor)
	{
		return;
	}

	CurrentTargetActor = TargetActor;
	LastTargetSeenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastTargetSeenTime;
	LastKnownTargetLocation = KnownLocation;
	bHasKnownTarget = true;
}

void AAIZombieController::ClearCurrentTarget(UBlackboardComponent* BlackboardComponent)
{
	ClearFocus(EAIFocusPriority::Gameplay);
	CurrentTargetActor.Reset();
	bHasKnownTarget = false;

	if (BlackboardComponent)
	{
		BlackboardComponent->ClearValue(TargetPlayerKey);
		BlackboardComponent->ClearValue(PlayerLocationKey);
	}
}

void AAIZombieController::UpdateBlackboardTarget(UBlackboardComponent* BlackboardComponent, AActor* TargetActor, const FVector& TargetLocation)
{
	if (!BlackboardComponent || !TargetActor)
	{
		return;
	}

	SetFocus(TargetActor);

	if (BlackboardComponent->GetValueAsObject(TargetPlayerKey) != TargetActor)
	{
		BlackboardComponent->SetValueAsObject(TargetPlayerKey, TargetActor);
	}

	const FVector CurrentStoredLocation = BlackboardComponent->GetValueAsVector(PlayerLocationKey);
	if (FVector::DistSquared(CurrentStoredLocation, TargetLocation) >= FMath::Square(BlackboardLocationUpdateDistance))
	{
		BlackboardComponent->SetValueAsVector(PlayerLocationKey, TargetLocation);
	}
}

void AAIZombieController::HandleTargetPerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus)
{
	if (!UpdatedActor)
	{
		return;
	}

	AActor* PrimaryTargetActor = ResolvePrimaryTargetActor();
	if (!PrimaryTargetActor)
	{
		return;
	}

	const bool bMatchesTrackedPlayer =
		UpdatedActor == PlayerPawn ||
		UpdatedActor == PrimaryTargetActor;
	if (!bMatchesTrackedPlayer)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		AActor* SensedActor = UpdatedActor == PlayerPawn ? PrimaryTargetActor : UpdatedActor;
		RememberTarget(SensedActor, Stimulus.StimulusLocation);
	}
}
