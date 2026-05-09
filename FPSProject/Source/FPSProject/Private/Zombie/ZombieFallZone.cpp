#include "Zombie/ZombieFallZone.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Zombie/BaseZombie.h"

AZombieFallZone::AZombieFallZone()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ZoneVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneVolume"));
	ZoneVolume->SetupAttachment(SceneRoot);
	ZoneVolume->SetBoxExtent(FVector(140.0f, 320.0f, 120.0f));
	ZoneVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneVolume->SetGenerateOverlapEvents(true);

	DropDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("DropDirection"));
	DropDirection->SetupAttachment(SceneRoot);
	DropDirection->ArrowLength = 200.0f;
	DropDirection->ArrowSize = 1.5f;
}

void AZombieFallZone::BeginPlay()
{
	Super::BeginPlay();

	if (ZoneVolume)
	{
		ZoneVolume->OnComponentBeginOverlap.AddDynamic(this, &AZombieFallZone::HandleZoneBeginOverlap);
		ZoneVolume->OnComponentEndOverlap.AddDynamic(this, &AZombieFallZone::HandleZoneEndOverlap);
	}
}

bool AZombieFallZone::CanGuideZombieTowardTarget(
	const ABaseZombie* Zombie,
	const AActor* TargetActor,
	FVector& OutTargetLocation,
	float& OutScore) const
{
	if (!Zombie || !TargetActor || !ZoneVolume)
	{
		return false;
	}

	const FVector ZombieLocation = Zombie->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector ZoneLocation = ZoneVolume->GetComponentLocation();
	const float DropHeight = ZombieLocation.Z - TargetLocation.Z;
	const float Distance2D = FVector::Dist2D(ZombieLocation, TargetLocation);

	if (DropHeight < MinTargetDropHeight || DropHeight > MaxTargetDropHeight)
	{
		return false;
	}

	if (Distance2D < MinTargetDistance2D || Distance2D > MaxTargetDistance2D)
	{
		return false;
	}

	float AlignmentScore = 1.0f;
	if (bRequireTargetInFront && DropDirection)
	{
		FVector ZoneForward2D = DropDirection->GetForwardVector();
		ZoneForward2D.Z = 0.0f;
		ZoneForward2D.Normalize();

		FVector ZoneToTarget2D = TargetLocation - ZoneLocation;
		ZoneToTarget2D.Z = 0.0f;
		ZoneToTarget2D.Normalize();

		if (ZoneForward2D.IsNearlyZero() || ZoneToTarget2D.IsNearlyZero())
		{
			return false;
		}

		const float RequiredDot = FMath::Cos(FMath::DegreesToRadians(FacingHalfAngleDegrees));
		const float DirectionDot = FVector::DotProduct(ZoneForward2D, ZoneToTarget2D);
		if (DirectionDot < RequiredDot)
		{
			return false;
		}

		AlignmentScore = DirectionDot;
	}

	FVector PursuitTarget = TargetLocation;
	if (bBlendTowardDropDirection && DropDirection)
	{
		FVector ForwardTarget = ZoneLocation + DropDirection->GetForwardVector().GetSafeNormal2D() * 1000.0f;
		PursuitTarget = FMath::Lerp(TargetLocation, ForwardTarget, FMath::Clamp(DropDirectionBlendAlpha, 0.0f, 1.0f));
		PursuitTarget.Z = TargetLocation.Z;
	}

	OutTargetLocation = PursuitTarget;

	const FVector ZoneExtent = ZoneVolume->GetScaledBoxExtent();
	const float ZoneDistancePenalty =
		FMath::Clamp(FVector::Dist2D(ZombieLocation, ZoneLocation) / FMath::Max(FMath::Max(ZoneExtent.X, ZoneExtent.Y), 1.0f), 0.0f, 2.0f);
	OutScore = AlignmentScore * 1000.0f - ZoneDistancePenalty * 100.0f;
	return true;
}

void AZombieFallZone::HandleZoneBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (ABaseZombie* Zombie = Cast<ABaseZombie>(OtherActor))
	{
		Zombie->RegisterFallZone(this);
	}
}

void AZombieFallZone::HandleZoneEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (ABaseZombie* Zombie = Cast<ABaseZombie>(OtherActor))
	{
		Zombie->UnregisterFallZone(this);
	}
}
