#include "Zombie/ZombieFallZone.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Zombie/BaseZombie.h"

namespace
{
	TArray<TWeakObjectPtr<AZombieFallZone>> GRegisteredFallZones;

	FVector GetGuidanceTargetLocation(const AActor* TargetActor, const FVector& FromLocation)
	{
		if (!TargetActor)
		{
			return FromLocation;
		}

		if (const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
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

	GRegisteredFallZones.RemoveAll([](const TWeakObjectPtr<AZombieFallZone>& ZonePtr)
		{
			return !ZonePtr.IsValid();
		});
	GRegisteredFallZones.AddUnique(this);

	if (ZoneVolume)
	{
		ZoneVolume->OnComponentBeginOverlap.AddDynamic(this, &AZombieFallZone::HandleZoneBeginOverlap);
		ZoneVolume->OnComponentEndOverlap.AddDynamic(this, &AZombieFallZone::HandleZoneEndOverlap);
	}

	RegisterInitialOverlappingZombies();
}

void AZombieFallZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GRegisteredFallZones.RemoveAll([this](const TWeakObjectPtr<AZombieFallZone>& ZonePtr)
		{
			return !ZonePtr.IsValid() || ZonePtr.Get() == this;
		});

	Super::EndPlay(EndPlayReason);
}

void AZombieFallZone::GetRegisteredFallZones(UWorld* World, TArray<AZombieFallZone*>& OutZones)
{
	OutZones.Reset();

	GRegisteredFallZones.RemoveAll([](const TWeakObjectPtr<AZombieFallZone>& ZonePtr)
		{
			return !ZonePtr.IsValid();
		});

	for (const TWeakObjectPtr<AZombieFallZone>& ZonePtr : GRegisteredFallZones)
	{
		if (AZombieFallZone* Zone = ZonePtr.Get())
		{
			if (Zone->GetWorld() == World)
			{
				OutZones.Add(Zone);
			}
		}
	}
}

bool AZombieFallZone::CanGuideZombieTowardTarget(
	ABaseZombie* Zombie,
	const AActor* TargetActor,
	FVector& OutApproachLocation,
	FVector& OutCommitLocation,
	float& OutScore)
{
	if (!Zombie || !TargetActor || !ZoneVolume)
	{
		return false;
	}

	const FVector ZombieLocation = Zombie->GetActorLocation();
	const FVector TargetLocation = GetGuidanceTargetLocation(TargetActor, ZombieLocation);
	const FVector ZoneLocation = ZoneVolume->GetComponentLocation();
	const float ZombieToZoneDistance2D = FVector::Dist2D(ZombieLocation, ZoneLocation);
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

	if (ZombieToZoneDistance2D > MaxZombieDistance2D)
	{
		return false;
	}

	const FVector ZoneExtent = ZoneVolume->GetScaledBoxExtent();
	const FVector ZoneForward = DropDirection ? DropDirection->GetForwardVector().GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
	const FVector ZoneRight = DropDirection ? DropDirection->GetRightVector().GetSafeNormal2D() : GetActorRightVector().GetSafeNormal2D();
	const float LateralHalfWidth = FMath::Max(ZoneExtent.Y - SlotLateralPadding, 0.0f);
	const int32 SlotCount = GetSlotCount(LateralHalfWidth);
	const bool bAlreadyHadSlot = ZombieSlotAssignments.Contains(Zombie);
	const int32 AssignedSlotIndex = GetOrAssignSlotIndex(Zombie, LateralHalfWidth, ZombieLocation);
	const float AssignedSlotOffset = GetSlotOffset(AssignedSlotIndex, SlotCount, LateralHalfWidth);

	float AlignmentScore = 1.0f;
	if (bRequireTargetInFront && DropDirection)
	{
		FVector ZoneToTarget2D = TargetLocation - ZoneLocation;
		ZoneToTarget2D.Z = 0.0f;
		ZoneToTarget2D.Normalize();

		if (ZoneForward.IsNearlyZero() || ZoneToTarget2D.IsNearlyZero())
		{
			return false;
		}

		const float RequiredDot = FMath::Cos(FMath::DegreesToRadians(FacingHalfAngleDegrees));
		const float DirectionDot = FVector::DotProduct(ZoneForward, ZoneToTarget2D);
		if (DirectionDot < RequiredDot)
		{
			return false;
		}

		AlignmentScore = DirectionDot;
	}

	float TargetLateralOffset = AssignedSlotOffset;
	if (bBlendTowardDropDirection && !ZoneRight.IsNearlyZero())
	{
		const FVector TargetOffset = TargetLocation - ZoneLocation;
		const float RawTargetLateral = FVector::DotProduct(TargetOffset, ZoneRight);
		const float ClampedTargetLateral = FMath::Clamp(RawTargetLateral, -LateralHalfWidth, LateralHalfWidth);
		TargetLateralOffset = FMath::Lerp(AssignedSlotOffset, ClampedTargetLateral, FMath::Clamp(DropDirectionBlendAlpha, 0.0f, 1.0f));
	}

	const FVector SlotOrigin = ZoneLocation + ZoneRight * TargetLateralOffset;
	const float ApproachForwardDistance = FMath::Clamp(
		ZoneExtent.X - ApproachDistanceInsideZone,
		0.0f,
		ZoneExtent.X);
	OutApproachLocation = SlotOrigin + ZoneForward * ApproachForwardDistance;
	OutCommitLocation = SlotOrigin + ZoneForward * FMath::Max(ZoneExtent.X + FallTargetOvershootDistance, FallTargetOvershootDistance);

	const float ZoneDistancePenalty =
		FMath::Clamp(ZombieToZoneDistance2D / FMath::Max(FMath::Max(ZoneExtent.X, ZoneExtent.Y), 1.0f), 0.0f, 4.0f);
	const float SlotOccupancyPenalty = CountZombiesAssignedToSlot(AssignedSlotIndex) * OccupiedSlotScorePenalty;
	const float SlotStickinessBonus = bAlreadyHadSlot ? ExistingSlotScoreBonus : 0.0f;
	OutScore = AlignmentScore * 1000.0f - ZoneDistancePenalty * 250.0f - SlotOccupancyPenalty + SlotStickinessBonus;
	return true;
}

void AZombieFallZone::RegisterInitialOverlappingZombies()
{
	if (!ZoneVolume)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	ZoneVolume->GetOverlappingActors(OverlappingActors, ABaseZombie::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (ABaseZombie* Zombie = Cast<ABaseZombie>(Actor))
		{
			Zombie->RegisterFallZone(this);
		}
	}
}

void AZombieFallZone::CleanupInvalidZombieAssignments()
{
	for (auto It = ZombieSlotAssignments.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

int32 AZombieFallZone::GetSlotCount(float LateralHalfWidth) const
{
	if (SlotSpacing <= KINDA_SMALL_NUMBER || LateralHalfWidth <= KINDA_SMALL_NUMBER)
	{
		return 1;
	}

	const float UsableWidth = LateralHalfWidth * 2.0f;
	return FMath::Max(1, FMath::FloorToInt(UsableWidth / SlotSpacing) + 1);
}

float AZombieFallZone::GetSlotOffset(int32 SlotIndex, int32 SlotCount, float LateralHalfWidth) const
{
	if (SlotCount <= 1 || LateralHalfWidth <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float Alpha = static_cast<float>(SlotIndex) / static_cast<float>(SlotCount - 1);
	return FMath::Lerp(-LateralHalfWidth, LateralHalfWidth, Alpha);
}

int32 AZombieFallZone::CountZombiesAssignedToSlot(int32 SlotIndex) const
{
	int32 AssignedCount = 0;
	for (const TPair<TWeakObjectPtr<ABaseZombie>, int32>& Pair : ZombieSlotAssignments)
	{
		if (Pair.Key.IsValid() && Pair.Value == SlotIndex)
		{
			++AssignedCount;
		}
	}

	return AssignedCount;
}

int32 AZombieFallZone::GetOrAssignSlotIndex(ABaseZombie* Zombie, float LateralHalfWidth, const FVector& ZombieLocation)
{
	CleanupInvalidZombieAssignments();

	if (!Zombie)
	{
		return 0;
	}

	const int32 SlotCount = GetSlotCount(LateralHalfWidth);
	if (const int32* ExistingSlot = ZombieSlotAssignments.Find(Zombie))
	{
		return FMath::Clamp(*ExistingSlot, 0, SlotCount - 1);
	}

	const FVector ZoneLocation = ZoneVolume ? ZoneVolume->GetComponentLocation() : GetActorLocation();
	const FVector ZoneRight = DropDirection ? DropDirection->GetRightVector().GetSafeNormal2D() : GetActorRightVector().GetSafeNormal2D();

	int32 BestSlotIndex = 0;
	float BestSlotScore = TNumericLimits<float>::Max();

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		const float SlotOffset = GetSlotOffset(SlotIndex, SlotCount, LateralHalfWidth);
		const FVector SlotWorldLocation = ZoneLocation + ZoneRight * SlotOffset;
		const int32 Occupancy = CountZombiesAssignedToSlot(SlotIndex);
		const float DistanceScore = FVector::Dist2D(ZombieLocation, SlotWorldLocation);
		const float SlotScore = Occupancy * OccupiedSlotScorePenalty + DistanceScore;
		if (SlotScore < BestSlotScore)
		{
			BestSlotScore = SlotScore;
			BestSlotIndex = SlotIndex;
		}
	}

	ZombieSlotAssignments.Add(Zombie, BestSlotIndex);
	return BestSlotIndex;
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
		GetOrAssignSlotIndex(Zombie, FMath::Max(ZoneVolume->GetScaledBoxExtent().Y - SlotLateralPadding, 0.0f), Zombie->GetActorLocation());
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
		ZombieSlotAssignments.Remove(Zombie);
	}
}
