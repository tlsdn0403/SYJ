#pragma once

#include "CoreMinimal.h"
#include "Protocol.pb.h"

class UFPSProjectGameInstance;
class ABaseZombie;

class FFPSSpawnManager
{
public:
	explicit FFPSSpawnManager(UFPSProjectGameInstance& InOwner);

	void ProcessSpawnObject(const Protocol::ObjectInfo& ObjectInfo, bool bIsMine);
	void Tick(float DeltaTime);

private:
	struct FPlayerSpawnContext
	{
		uint64 ObjectId = 0;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		Protocol::PosInfo PosInfo;
		bool bUsedStage2SpawnTransform = false;
	};

	struct FPendingTileZombiePlacement
	{
		uint64 ObjectId = 0;
		TWeakObjectPtr<ABaseZombie> Zombie;
		FVector LocalLocation = FVector::ZeroVector;
		float LocalYaw = 0.0f;
		int32 TileTypeCode = 0;
		int32 TileOccurrenceIndex = 0;
	};

	bool TryBuildPlayerSpawnContext(UWorld* World, const Protocol::ObjectInfo& ObjectInfo, FPlayerSpawnContext& OutContext) const;
	bool TryResolveTileZombieTransform(UWorld* World, int32 TileTypeCode, int32 TileOccurrenceIndex, const FVector& LocalLocation, float LocalYaw, FTransform& OutTransform) const;
	void QueuePendingTileZombiePlacement(uint64 ObjectId, ABaseZombie* Zombie, const FVector& LocalLocation, float LocalYaw, int32 TileTypeCode, int32 TileOccurrenceIndex);
	void ProcessPendingTileZombiePlacements(int32 MaxPlacementAttempts);
	void SendZombiePlacementCorrection(uint64 ObjectId, const FVector& WorldLocation, const FRotator& WorldRotation);
	void SpawnZombie(UWorld* World, const Protocol::ObjectInfo& ObjectInfo);
	void SpawnLocalPlayer(UWorld* World, const FPlayerSpawnContext& SpawnContext);
	void SpawnRemotePlayer(UWorld* World, const FPlayerSpawnContext& SpawnContext);

	UFPSProjectGameInstance& Owner;
	TArray<FPendingTileZombiePlacement> PendingTileZombiePlacements;
};
