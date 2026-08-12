#include "FPSStage2WorldUtils.h"

#include "Characters/FPSBaseCharacter.h"
#include "Algo/Sort.h"
#include "Camera/CameraComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/DrawFrustumComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerStart.h"
#include "Stage2/Stage2TileManager.h"
#include "Truck/Truck.h"

namespace FPSStage2WorldUtils
{
	static bool TryGetStaticPlayerStartTransform(UWorld* World, FTransform& OutTransform);

	static void HideTruckHelperVisuals(ATruck* Truck)
	{
		TArray<UCameraComponent*> CameraComponents;
		Truck->GetComponents<UCameraComponent>(CameraComponents);
		for (UCameraComponent* CameraComponent : CameraComponents)
		{
			if (IsValid(CameraComponent))
			{
				CameraComponent->SetHiddenInGame(true, true);
#if WITH_EDITORONLY_DATA
				CameraComponent->bDrawFrustumAllowed = false;
				CameraComponent->bCameraMeshHiddenInGame = true;
#endif
			}
		}

		TArray<UBillboardComponent*> BillboardComponents;
		Truck->GetComponents<UBillboardComponent>(BillboardComponents);
		for (UBillboardComponent* BillboardComponent : BillboardComponents)
		{
			if (IsValid(BillboardComponent))
			{
				BillboardComponent->SetHiddenInGame(true, true);
				BillboardComponent->SetVisibility(false, true);
			}
		}

		TArray<UDrawFrustumComponent*> FrustumComponents;
		Truck->GetComponents<UDrawFrustumComponent>(FrustumComponents);
		for (UDrawFrustumComponent* FrustumComponent : FrustumComponents)
		{
			if (IsValid(FrustumComponent))
			{
				FrustumComponent->SetHiddenInGame(true, true);
				FrustumComponent->SetVisibility(false, true);
			}
		}

		TArray<UShapeComponent*> ShapeComponents;
		Truck->GetComponents<UShapeComponent>(ShapeComponents);
		for (UShapeComponent* ShapeComponent : ShapeComponents)
		{
			if (IsValid(ShapeComponent))
			{
				ShapeComponent->SetHiddenInGame(true, true);
				ShapeComponent->SetVisibility(false, true);
			}
		}

		TArray<USceneComponent*> SceneComponents;
		Truck->GetComponents<USceneComponent>(SceneComponents);
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (!IsValid(SceneComponent))
			{
				continue;
			}

			const FString ComponentName = SceneComponent->GetName();
			if (!ComponentName.Contains(TEXT("Camera"), ESearchCase::IgnoreCase) &&
				!ComponentName.Contains(TEXT("SpringArm"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			SceneComponent->SetHiddenInGame(true, true);
			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent))
			{
				PrimitiveComponent->SetVisibility(false, true);
			}
		}
	}

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
			LevelName.Contains(TEXT("stage2"), ESearchCase::IgnoreCase) ||
			IsStaticStage2LevelName(LevelName);
	}

	bool IsStaticStage2LevelName(const FString& LevelName)
	{
		return LevelName.Contains(TEXT("0812_NEWMAP_Ba"), ESearchCase::IgnoreCase);
	}

	bool IsStage2World(const UWorld* World)
	{
		return World && IsStage2LevelName(World->GetMapName());
	}

	bool IsStaticStage2World(const UWorld* World)
	{
		return World && IsStaticStage2LevelName(World->GetMapName());
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

	ATruck* EnsureStaticStage2Truck(UWorld* World)
	{
		if (!IsStaticStage2World(World))
		{
			return nullptr;
		}

		for (TActorIterator<ATruck> It(World); It; ++It)
		{
			if (ATruck* ExistingTruck = *It; IsValid(ExistingTruck))
			{
				return ExistingTruck;
			}
		}

		UClass* TruckClass = LoadClass<ATruck>(nullptr, TEXT("/Game/Truck/BP_Truck.BP_Truck_C"));
		if (!TruckClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[Stage2Truck] Failed to load BP_Truck class."));
			return nullptr;
		}

		FTransform PlayerStartTransform;
		if (!TryGetStaticPlayerStartTransform(World, PlayerStartTransform))
		{
			UE_LOG(LogTemp, Error, TEXT("[Stage2Truck] Static map has no PlayerStart."));
			return nullptr;
		}

		static constexpr float TruckSpawnSideOffset = 500.0f;
		FVector TruckSpawnLocation =
			PlayerStartTransform.GetLocation() +
			PlayerStartTransform.GetRotation().GetRightVector() * TruckSpawnSideOffset +
			FVector(0.0f, 0.0f, 300.0f);
		FRotator TruckSpawnRotation = PlayerStartTransform.Rotator();
		TruckSpawnRotation.Yaw -= 90.0f;
		FTransform SpawnTransform(TruckSpawnRotation, TruckSpawnLocation);
		ATruck* Truck = World->SpawnActorDeferred<ATruck>(
			TruckClass,
			SpawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Truck)
		{
			UE_LOG(LogTemp, Error, TEXT("[Stage2Truck] Failed to spawn BP_Truck."));
			return nullptr;
		}

		Truck->NetworkTruckId = 3;
		Truck->FinishSpawning(SpawnTransform);

		FVector GroundedLocation;
		const bool bPlacedOnGround = TryPlaceTruckOnGround(Truck, TruckSpawnLocation, 8.0f, GroundedLocation);
		if (bPlacedOnGround)
		{
			Truck->SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Truck->SetActorHiddenInGame(false);
		Truck->SetActorEnableCollision(true);
		HideTruckHelperVisuals(Truck);
		if (USkeletalMeshComponent* TruckMesh = Truck->GetMesh())
		{
			TruckMesh->SetHiddenInGame(false, true);
			TruckMesh->SetVisibility(true, true);
			TruckMesh->SetEnableGravity(true);
		}
		Truck->ResetVehiclePhysicsState(true);

		UE_LOG(LogTemp, Log,
			TEXT("[Stage2Truck] Spawned near PlayerStart. PlayerStart=%s Truck=%s Grounded=%d"),
			*PlayerStartTransform.GetLocation().ToString(),
			*Truck->GetActorLocation().ToString(),
			bPlacedOnGround ? 1 : 0);
		return Truck;
	}

	void PlaceStaticStage2TruckNearPlayer(ATruck* Truck, const AActor* PlayerActor)
	{
		if (!IsValid(Truck) || !IsValid(PlayerActor) || !IsStaticStage2World(Truck->GetWorld()))
		{
			return;
		}

		static const FName PlayerPlacementTag(TEXT("Stage2PlayerTruckPlacementApplied"));
		if (Truck->Tags.Contains(PlayerPlacementTag))
		{
			return;
		}

		FTransform PlayerStartTransform;
		if (!TryGetStaticPlayerStartTransform(Truck->GetWorld(), PlayerStartTransform))
		{
			return;
		}

		static constexpr float PlayerSpawnForwardOffset = 100.0f;
		static constexpr float TruckSpawnForwardOffset = 500.0f;
		static constexpr float TruckSpawnLeftOffset = 500.0f;
		const FVector SpawnForwardVector = -PlayerStartTransform.GetRotation().GetRightVector();
		const FVector SpawnRightVector = PlayerStartTransform.GetRotation().GetForwardVector();
		FVector TargetLocation =
			PlayerStartTransform.GetLocation() +
			SpawnForwardVector * (PlayerSpawnForwardOffset + TruckSpawnForwardOffset) +
			SpawnRightVector * -TruckSpawnLeftOffset +
			FVector(0.0f, 0.0f, 300.0f);
		FVector GroundedLocation;
		const bool bPlacedOnGround = TryPlaceTruckOnGround(Truck, TargetLocation, 8.0f, GroundedLocation);
		if (bPlacedOnGround)
		{
			TargetLocation = GroundedLocation;
		}

		FRotator TargetRotation = PlayerStartTransform.Rotator();
		TargetRotation.Yaw -= 90.0f;
		TargetRotation.Pitch = 0.0f;
		TargetRotation.Roll = 0.0f;
		Truck->SetActorLocationAndRotation(
			TargetLocation,
			TargetRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Truck->SetActorHiddenInGame(false);
		Truck->SetActorEnableCollision(true);
		HideTruckHelperVisuals(Truck);
		if (USkeletalMeshComponent* TruckMesh = Truck->GetMesh())
		{
			TruckMesh->SetHiddenInGame(false, true);
			TruckMesh->SetVisibility(true, true);
			TruckMesh->SetEnableGravity(true);
		}
		Truck->ResetVehiclePhysicsState(true);
		Truck->Tags.Add(PlayerPlacementTag);

		UE_LOG(LogTemp, Log,
			TEXT("[Stage2Truck] Placed near local player. Player=%s Truck=%s Grounded=%d"),
			*PlayerActor->GetActorLocation().ToString(),
			*Truck->GetActorLocation().ToString(),
			bPlacedOnGround ? 1 : 0);
	}

	static bool TryGetStaticPlayerStartTransform(UWorld* World, FTransform& OutTransform)
	{
		if (!World)
		{
			return false;
		}

		TArray<APlayerStart*> PlayerStarts;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			if (APlayerStart* PlayerStart = *It)
			{
				PlayerStarts.Add(PlayerStart);
			}
		}

		Algo::SortBy(PlayerStarts, [](const APlayerStart* PlayerStart)
		{
			return GetPathNameSafe(PlayerStart);
		});

		for (APlayerStart* PlayerStart : PlayerStarts)
		{
			if (!IsValid(PlayerStart))
			{
				continue;
			}

			OutTransform = PlayerStart->GetActorTransform();
			return true;
		}

		return false;
	}

	bool TryGetPlayerSpawnTransform(UWorld* World, uint64 ObjectId, FTransform& OutTransform)
	{
		if (!IsStage2World(World))
		{
			return false;
		}

		FTransform InitialSpawnTransform;
		bool bHasInitialSpawnTransform = false;

		const AStage2TileManager* Stage2TileManager = FindStage2TileManager(World);
		if (Stage2TileManager && Stage2TileManager->AreInitialTilesReady())
		{
			bHasInitialSpawnTransform = Stage2TileManager->TryGetInitialPlayerSpawnTransform(InitialSpawnTransform);
		}

		if (!bHasInitialSpawnTransform)
		{
			bHasInitialSpawnTransform = TryGetStaticPlayerStartTransform(World, InitialSpawnTransform);
		}

		if (!bHasInitialSpawnTransform)
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
		if (IsStaticStage2World(World))
		{
			FRotator WeaponRotation = OutTransform.Rotator();
			WeaponRotation.Yaw -= 90.0f;
			OutTransform.SetRotation(WeaponRotation.Quaternion());
		}
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

		if (IsStaticStage2World(World))
		{
			Truck->SetActorHiddenInGame(false);
			Truck->SetActorEnableCollision(true);
			HideTruckHelperVisuals(Truck);
			if (USkeletalMeshComponent* TruckMesh = Truck->GetMesh())
			{
				TruckMesh->SetHiddenInGame(false, true);
				TruckMesh->SetVisibility(true, true);
				TruckMesh->SetEnableGravity(true);
			}
			Truck->ResetVehiclePhysicsState(true);
			Truck->Tags.Add(Stage2InitialTruckPlacementTag);
			return;
		}

		static constexpr float TruckSpawnForwardOffset = 700.0f;
		static constexpr float TruckGroundClearance = 8.0f;

		const AStage2TileManager* Stage2TileManager = FindStage2TileManager(World);
		FTransform InitialSpawnTransform;
		bool bHasInitialSpawnTransform = false;

		if (Stage2TileManager && Stage2TileManager->AreInitialTilesReady())
		{
			bHasInitialSpawnTransform = Stage2TileManager->TryGetInitialPlayerSpawnTransform(InitialSpawnTransform);
		}

		if (!bHasInitialSpawnTransform)
		{
			bHasInitialSpawnTransform = TryGetStaticPlayerStartTransform(World, InitialSpawnTransform);
		}

		if (!bHasInitialSpawnTransform)
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
		Truck->ResetVehiclePhysicsState(true);
		Truck->Tags.Add(Stage2InitialTruckPlacementTag);
	}
}