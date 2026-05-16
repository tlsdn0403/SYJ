#include "Zombie/ZombieFallZone.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Zombie/BaseZombie.h"

namespace
{
	TArray<TWeakObjectPtr<AZombieFallZone>> GRegisteredFallZones;

	void CleanupInvalidRegisteredFallZones()
	{
		GRegisteredFallZones.RemoveAll([](const TWeakObjectPtr<AZombieFallZone>& ZonePtr)
			{
				return !ZonePtr.IsValid();
			});
	}

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
	// 매프레임 틱 호출 x
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ZoneVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneVolume"));
	ZoneVolume->SetupAttachment(SceneRoot);
	ZoneVolume->SetBoxExtent(FVector(140.0f, 320.0f, 120.0f));
	// 실제 충돌용이 아니라 Fall Zone의 범위를 잡는 기준 박스다.
	ZoneVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneVolume->SetGenerateOverlapEvents(false);

	// 낙하방향을 시각적으로 보여주는 화살표
	DropDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("DropDirection"));
	DropDirection->SetupAttachment(SceneRoot);
	DropDirection->ArrowLength = 200.0f;
	DropDirection->ArrowSize = 1.5f;
}

void AZombieFallZone::BeginPlay()
{
	Super::BeginPlay();

	// 전역 리스트에서 이미 사라진 Zone을 정리한 뒤 현재 Zone을 등록한다.
	CleanupInvalidRegisteredFallZones();
	// FallZone 배열에 현재 객체를 넣어줌(중복 방지)
	GRegisteredFallZones.AddUnique(this);
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

	// 이미 파괴된 Zone은 결과에 섞이지 않도록 먼저 제거한다.
	CleanupInvalidRegisteredFallZones();

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
// 좀비를 타겟 방향으로 보낼 수 있는지
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
	// 좀비 , 타겟 , 존 위치
	const FVector ZombieLocation = Zombie->GetActorLocation();
	const FVector TargetLocation = GetGuidanceTargetLocation(TargetActor, ZombieLocation);
	const FVector ZoneLocation = ZoneVolume->GetComponentLocation();
	const float ZombieToZoneDistance2D = FVector::Dist2D(ZombieLocation, ZoneLocation);
	const float DropHeight = ZombieLocation.Z - TargetLocation.Z;
	const float Distance2D = FVector::Dist2D(ZombieLocation, TargetLocation);

	// 좀비가 타겟보다 높은 위치에 있어야함
	if (DropHeight < MinTargetDropHeight || DropHeight > MaxTargetDropHeight)
	{
		return false;
	}
	// 목표와 좀비 사이의 2D 거리가 너무 가깝거나 멀면 안됨
	if (Distance2D < MinTargetDistance2D || Distance2D > MaxTargetDistance2D)
	{
		return false; 
	}

	//
	if (ZombieToZoneDistance2D > MaxZombieDistance2D)
	{
		return false;
	}

	const FVector ZoneExtent = ZoneVolume->GetScaledBoxExtent();
	// 화살표가 있으면 그 방향을 신뢰하고, 없으면 액터의 기본 전방/우측을 사용한다.
	const FVector ZoneForward = DropDirection ? DropDirection->GetForwardVector().GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
	const FVector ZoneRight = DropDirection ? DropDirection->GetRightVector().GetSafeNormal2D() : GetActorRightVector().GetSafeNormal2D();
	const float LateralHalfWidth = FMath::Max(ZoneExtent.Y - SlotLateralPadding, 0.0f);

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

	float TargetLateralOffset = 0.0f;
	if (!ZoneRight.IsNearlyZero())
	{
		// 슬롯을 따로 배정하지 않고, 현재 좀비 위치를 기준으로 접근 지점을 잡아 코드를 단순화한다.
		const FVector ZombieOffset = ZombieLocation - ZoneLocation;
		const float RawZombieLateral = FVector::DotProduct(ZombieOffset, ZoneRight);
		const float ClampedZombieLateral = FMath::Clamp(RawZombieLateral, -LateralHalfWidth, LateralHalfWidth);
		TargetLateralOffset = ClampedZombieLateral;

		const FVector TargetOffset = TargetLocation - ZoneLocation;
		const float RawTargetLateral = FVector::DotProduct(TargetOffset, ZoneRight);
		const float ClampedTargetLateral = FMath::Clamp(RawTargetLateral, -LateralHalfWidth, LateralHalfWidth);

		if (bBlendTowardDropDirection)
		{
			// 타겟 쪽으로 조금 더 붙여 주면 낙하 후의 진행 방향이 자연스러워진다.
			const float BlendAlpha = FMath::Clamp(DropDirectionBlendAlpha, 0.0f, 1.0f);
			TargetLateralOffset = FMath::Lerp(ClampedZombieLateral, ClampedTargetLateral, BlendAlpha);
		}
	}

	const FVector SlotOrigin = ZoneLocation + ZoneRight * TargetLateralOffset;
	// 박스 안쪽의 접근 지점과 박스를 살짝 넘어간 커밋 지점을 각각 계산한다.
	const float ApproachForwardDistance = FMath::Clamp(
		ZoneExtent.X - ApproachDistanceInsideZone,
		0.0f,
		ZoneExtent.X);
	OutApproachLocation = SlotOrigin + ZoneForward * ApproachForwardDistance;
	OutCommitLocation = SlotOrigin + ZoneForward * FMath::Max(ZoneExtent.X + FallTargetOvershootDistance, FallTargetOvershootDistance);

	const float ZoneDistancePenalty =
		FMath::Clamp(ZombieToZoneDistance2D / FMath::Max(FMath::Max(ZoneExtent.X, ZoneExtent.Y), 1.0f), 0.0f, 4.0f);
	// 방향이 잘 맞는지와 좀비가 Zone에 얼마나 가까운지로 점수 계산
	OutScore = AlignmentScore * 1000.0f - ZoneDistancePenalty * 250.0f;
	return true;
}

