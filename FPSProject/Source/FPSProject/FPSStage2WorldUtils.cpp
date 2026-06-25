#include "FPSStage2WorldUtils.h"

#include "Characters/FPSBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Stage2/Stage2TileManager.h"
#include "Truck/Truck.h"

namespace FPSStage2WorldUtils
{
	void RestoreNetworkCharacterVisibility(AFPSBaseCharacter* Character)
	{
		if (!IsValid(Character))
		{
			return;
		}

		Character->SetActorHiddenInGame(false);
		Character->SetActorEnableCollision(true);

		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		Character->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			if (!IsValid(SkeletalMeshComponent))
			{
				continue;
			}

			SkeletalMeshComponent->SetHiddenInGame(false, true);
			SkeletalMeshComponent->SetVisibility(true, true);
			SkeletalMeshComponent->SetOwnerNoSee(false);
			SkeletalMeshComponent->SetOnlyOwnerSee(false);
		}

		TArray<UWidgetComponent*> WidgetComponents;
		Character->GetComponents<UWidgetComponent>(WidgetComponents);
		for (UWidgetComponent* WidgetComponent : WidgetComponents)
		{
			if (!IsValid(WidgetComponent))
			{
				continue;
			}

			WidgetComponent->SetHiddenInGame(false, true);
			WidgetComponent->SetVisibility(true, true);
		}
	}

	bool IsStage2LevelName(const FString& LevelName)
	{
		return LevelName.Contains(TEXT("map_level2"), ESearchCase::IgnoreCase) ||
			LevelName.Contains(TEXT("level2"), ESearchCase::IgnoreCase) ||
			LevelName.Contains(TEXT("stage2"), ESearchCase::IgnoreCase);
	}

	bool IsStage2World(const UWorld* World)
	{
		return World && IsStage2LevelName(World->GetMapName());
	}

	AStage2TileManager* FindStage2TileManager(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AStage2TileManager> It(World); It; ++It)
		{
			return *It;
		}

		return nullptr;
	}

	bool TryGetPlayerSpawnTransform(UWorld* World, uint64 ObjectId, FTransform& OutTransform)
	{
		if (!IsStage2World(World))
		{
			return false;
		}

		const AStage2TileManager* Stage2TileManager = FindStage2TileManager(World);
		if (!Stage2TileManager || !Stage2TileManager->AreInitialTilesReady())
		{
			return false;
		}

		FTransform InitialSpawnTransform;
		if (!Stage2TileManager->TryGetInitialPlayerSpawnTransform(InitialSpawnTransform))
		{
			return false;
		}

		static constexpr float PlayerSpawnSpacing = 90.0f;
		static constexpr float PlayerSpawnForwardOffset = 100.0f;
		const int32 SpawnSlot = static_cast<int32>(ObjectId % 3);
		const float LateralOffset = SpawnSlot == 0 ? 0.0f : (SpawnSlot == 1 ? -PlayerSpawnSpacing : PlayerSpawnSpacing);
		const FVector SpawnForwardVector = -InitialSpawnTransform.GetRotation().GetRightVector();
		const FVector SpawnRightVector = InitialSpawnTransform.GetRotation().GetForwardVector();

		FVector SpawnLocation =
			InitialSpawnTransform.GetLocation() +
			SpawnRightVector * LateralOffset +
			SpawnForwardVector * PlayerSpawnForwardOffset +
			FVector(0.0f, 0.0f, 100.0f);

		FHitResult GroundHit;
		const FVector TraceStart = SpawnLocation + FVector(0.0f, 0.0f, 500.0f);
		const FVector TraceEnd = SpawnLocation - FVector(0.0f, 0.0f, 2500.0f);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility))
		{
			SpawnLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 120.0f);
		}
		else
		{
			const FVector CenterSpawnLocation =
				InitialSpawnTransform.GetLocation() +
				SpawnForwardVector * PlayerSpawnForwardOffset +
				FVector(0.0f, 0.0f, 100.0f);
			const FVector CenterTraceStart = CenterSpawnLocation + FVector(0.0f, 0.0f, 500.0f);
			const FVector CenterTraceEnd = CenterSpawnLocation - FVector(0.0f, 0.0f, 2500.0f);
			if (World->LineTraceSingleByChannel(GroundHit, CenterTraceStart, CenterTraceEnd, ECC_Visibility))
			{
				SpawnLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 120.0f);
			}
			else
			{
				SpawnLocation = InitialSpawnTransform.GetLocation() + FVector(0.0f, 0.0f, 120.0f);
			}
		}

		OutTransform = InitialSpawnTransform;
		OutTransform.SetLocation(SpawnLocation);
		return true;
	}

	bool TryGetWeaponSpawnTransform(UWorld* World, uint64 ItemObjectId, const FVector& LocalOffset, FTransform& OutTransform)
	{
		if (!TryGetPlayerSpawnTransform(World, ItemObjectId, OutTransform))
		{
			return false;
		}

		const FQuat SpawnRotation = OutTransform.GetRotation();
		const FVector SpawnForwardVector = -SpawnRotation.GetRightVector();
		const FVector SpawnRightVector = SpawnRotation.GetForwardVector();
		FVector SpawnLocation =
			OutTransform.GetLocation() +
			SpawnRightVector * LocalOffset.Y +
			SpawnForwardVector * LocalOffset.X +
			FVector(0.0f, 0.0f, LocalOffset.Z);

		TryProjectLocationToGround(World, SpawnLocation, 18.0f, SpawnLocation);
		OutTransform.SetLocation(SpawnLocation);
		return true;
	}

	bool TryProjectLocationToGround(
		UWorld* World,
		const FVector& InLocation,
		float GroundOffset,
		FVector& OutLocation,
		const AActor* IgnoredActor)
	{
		if (!IsStage2World(World))
		{
			return false;
		}

		FHitResult GroundHit;
		const FVector TraceStart = InLocation + FVector(0.0f, 0.0f, 2000.0f);
		const FVector TraceEnd = InLocation - FVector(0.0f, 0.0f, 5000.0f);
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = false;
		if (IgnoredActor)
		{
			QueryParams.AddIgnoredActor(IgnoredActor);
		}
		if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			return false;
		}

		OutLocation = FVector(InLocation.X, InLocation.Y, GroundHit.ImpactPoint.Z + GroundOffset);
		return true;
	}

	bool TryPlaceTruckOnGround(
		ATruck* Truck,
		const FVector& InLocation,
		float AdditionalGroundOffset,
		FVector& OutLocation)
	{
		if (!IsValid(Truck) || !Truck->GetMesh())
		{
			return false;
		}

		UWorld* World = Truck->GetWorld();
		if (!IsStage2World(World))
		{
			return false;
		}

		FHitResult GroundHit;
		const FVector TraceStart = InLocation + FVector(0.0f, 0.0f, 500.0f);
		const FVector TraceEnd = InLocation - FVector(0.0f, 0.0f, 2500.0f);
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = false;
		QueryParams.AddIgnoredActor(Truck);
		if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			return false;
		}

		const FBoxSphereBounds MeshBounds = Truck->GetMesh()->Bounds;
		const float CurrentMeshBottomZ = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
		const float ActorOriginToMeshBottom = Truck->GetActorLocation().Z - CurrentMeshBottomZ;

		OutLocation = FVector(
			InLocation.X,
			InLocation.Y,
			GroundHit.ImpactPoint.Z + AdditionalGroundOffset + ActorOriginToMeshBottom);
		return true;
	}

	void SnapActorToGround(AActor* Actor, float AdditionalGroundOffset)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		UWorld* World = Actor->GetWorld();
		if (!IsStage2World(World))
		{
			return;
		}

		FHitResult GroundHit;
		const FVector ActorLocation = Actor->GetActorLocation();
		const FVector TraceStart = ActorLocation + FVector(0.0f, 0.0f, 2000.0f);
		const FVector TraceEnd = ActorLocation - FVector(0.0f, 0.0f, 5000.0f);
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = false;
		QueryParams.AddIgnoredActor(Actor);
		if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			return;
		}

		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent);

		const float CurrentBottomZ = BoundsOrigin.Z - BoundsExtent.Z;
		const float DeltaZ = GroundHit.ImpactPoint.Z + AdditionalGroundOffset - CurrentBottomZ;
		if (FMath::Abs(DeltaZ) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		Actor->SetActorLocation(
			ActorLocation + FVector(0.0f, 0.0f, DeltaZ),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	void ApplyInitialTruckPlacement(ATruck* Truck)
	{
		if (!IsValid(Truck))
		{
			return;
		}

		UWorld* World = Truck->GetWorld();
		if (!IsStage2World(World))
		{
			return;
		}

		static const FName Stage2InitialTruckPlacementTag(TEXT("Stage2InitialTruckPlacementApplied"));
		if (Truck->Tags.Contains(Stage2InitialTruckPlacementTag))
		{
			return;
		}

		static constexpr float TruckSpawnForwardOffset = 700.0f;
		static constexpr float TruckGroundClearance = 8.0f;

		FTransform InitialSpawnTransform;
		const AStage2TileManager* Stage2TileManager = FindStage2TileManager(World);
		if (!Stage2TileManager ||
			!Stage2TileManager->AreInitialTilesReady() ||
			!Stage2TileManager->TryGetInitialPlayerSpawnTransform(InitialSpawnTransform))
		{
			return;
		}

		const FVector TruckForwardVector = -InitialSpawnTransform.GetRotation().GetRightVector();
		FVector TruckSpawnLocation =
			InitialSpawnTransform.GetLocation() +
			TruckForwardVector * TruckSpawnForwardOffset;
		TryPlaceTruckOnGround(Truck, TruckSpawnLocation, TruckGroundClearance, TruckSpawnLocation);
		FRotator TruckRotation = TruckForwardVector.Rotation();
		TruckRotation.Yaw += 90.0f;
		Truck->SetActorLocationAndRotation(
			TruckSpawnLocation,
			TruckRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Truck->Tags.Add(Stage2InitialTruckPlacementTag);
	}
}