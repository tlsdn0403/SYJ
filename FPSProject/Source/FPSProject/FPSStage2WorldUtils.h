#pragma once

#include "CoreMinimal.h"

class AFPSBaseCharacter;
class AActor;
class AStage2TileManager;
class ATruck;
class UWorld;

namespace FPSStage2WorldUtils
{
	void RestoreNetworkCharacterVisibility(AFPSBaseCharacter* Character);
	bool IsStage2LevelName(const FString& LevelName);
	bool IsStage2World(const UWorld* World);
	AStage2TileManager* FindStage2TileManager(UWorld* World);
	bool TryGetPlayerSpawnTransform(UWorld* World, uint64 ObjectId, FTransform& OutTransform);
	bool TryProjectLocationToGround(
		UWorld* World,
		const FVector& InLocation,
		float GroundOffset,
		FVector& OutLocation,
		const AActor* IgnoredActor = nullptr);
	bool TryPlaceTruckOnGround(
		ATruck* Truck,
		const FVector& InLocation,
		float AdditionalGroundOffset,
		FVector& OutLocation);
	void SnapActorToGround(AActor* Actor, float AdditionalGroundOffset = 2.0f);
	void ApplyInitialTruckPlacement(ATruck* Truck);
}