#include "FPSSpawnManager.h"

#include "ClientPacketHandler.h"
#include "FPSProjectGameInstance.h"
#include "FPSStage2WorldUtils.h"
#include "FPSWorldObjectManager.h"
#include "AIController.h"
#include "Characters/FPSBaseCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Stage2/Stage2TileManager.h"
#include "Zombie/BaseZombie.h"

FFPSSpawnManager::FFPSSpawnManager(UFPSProjectGameInstance& InOwner)
	: Owner(InOwner)
{
}

void FFPSSpawnManager::ProcessSpawnObject(const Protocol::ObjectInfo& ObjectInfo, bool bIsMine)
{
	if (!Owner.IsConnectedToGameServer())
	{
		return;
	}

	UWorld* World = Owner.GetWorld();
	if (World == nullptr || Owner.WorldObjects == nullptr)
	{
		return;
	}

	Owner.CacheTruckActors();

	const uint64 ObjectId = ObjectInfo.object_id();
	if (ObjectId >= 1000000)
	{
		SpawnZombie(World, ObjectInfo);
		return;
	}

	if (Owner.WorldObjects->HasPlayer(ObjectId))
	{
		return;
	}

	FPlayerSpawnContext SpawnContext;
	if (!TryBuildPlayerSpawnContext(World, ObjectInfo, SpawnContext))
	{
		return;
	}

	if (bIsMine)
	{
		SpawnLocalPlayer(World, SpawnContext);
	}
	else
	{
		SpawnRemotePlayer(World, SpawnContext);
	}
}

void FFPSSpawnManager::Tick(float DeltaTime)
{
	ProcessPendingTileZombiePlacements();
}

bool FFPSSpawnManager::TryBuildPlayerSpawnContext(
	UWorld* World,
	const Protocol::ObjectInfo& ObjectInfo,
	FPlayerSpawnContext& OutContext) const
{
	if (World == nullptr)
	{
		return false;
	}

	OutContext.ObjectId = ObjectInfo.object_id();
	OutContext.Location = FVector(ObjectInfo.pos_info().x(), ObjectInfo.pos_info().y(), ObjectInfo.pos_info().z());
	OutContext.Rotation = FRotator(0.0f, ObjectInfo.pos_info().yaw(), 0.0f);
	OutContext.PosInfo.CopyFrom(ObjectInfo.pos_info());

	FTransform Stage2SpawnTransform;
	OutContext.bUsedStage2SpawnTransform =
		FPSStage2WorldUtils::TryGetPlayerSpawnTransform(World, OutContext.ObjectId, Stage2SpawnTransform);
	if (OutContext.bUsedStage2SpawnTransform)
	{
		OutContext.Location = Stage2SpawnTransform.GetLocation();
		OutContext.Rotation = Stage2SpawnTransform.Rotator();
		OutContext.PosInfo.set_x(OutContext.Location.X);
		OutContext.PosInfo.set_y(OutContext.Location.Y);
		OutContext.PosInfo.set_z(OutContext.Location.Z);
		OutContext.PosInfo.set_yaw(OutContext.Rotation.Yaw);
		UE_LOG(LogTemp, Warning, TEXT("[Stage2Spawn] Override player spawn. ObjectId=%llu Location=%s"),
			OutContext.ObjectId,
			*OutContext.Location.ToString());
	}
	else
	{
		FPSStage2WorldUtils::TryProjectLocationToGround(World, OutContext.Location, 120.0f, OutContext.Location);
		OutContext.PosInfo.set_x(OutContext.Location.X);
		OutContext.PosInfo.set_y(OutContext.Location.Y);
		OutContext.PosInfo.set_z(OutContext.Location.Z);
	}

	return true;
}

bool FFPSSpawnManager::TryResolveTileZombieTransform(
	UWorld* World,
	int32 TileTypeCode,
	int32 TileOccurrenceIndex,
	const FVector& LocalLocation,
	float LocalYaw,
	FTransform& OutTransform) const
{
	if (World == nullptr || TileTypeCode == 0)
	{
		return false;
	}

	EStage2TileType TileType = EStage2TileType::Straight;
	switch (TileTypeCode)
	{
	case 1:
		TileType = EStage2TileType::Straight;
		break;
	case 2:
		TileType = EStage2TileType::Left;
		break;
	case 3:
		TileType = EStage2TileType::Right;
		break;
	default:
		return false;
	}

	AStage2TileManager* TileManager = FPSStage2WorldUtils::FindStage2TileManager(World);
	return TileManager && TileManager->TryBuildWorldTransformForTileLocalPoint(TileType, LocalLocation, LocalYaw, TileOccurrenceIndex, OutTransform);
}

void FFPSSpawnManager::QueuePendingTileZombiePlacement(
	uint64 ObjectId,
	ABaseZombie* Zombie,
	const FVector& LocalLocation,
	float LocalYaw,
	int32 TileTypeCode,
	int32 TileOccurrenceIndex)
{
	if (Zombie == nullptr)
	{
		return;
	}

	FPendingTileZombiePlacement& PendingPlacement = PendingTileZombiePlacements.AddDefaulted_GetRef();
	PendingPlacement.ObjectId = ObjectId;
	PendingPlacement.Zombie = Zombie;
	PendingPlacement.LocalLocation = LocalLocation;
	PendingPlacement.LocalYaw = LocalYaw;
	PendingPlacement.TileTypeCode = TileTypeCode;
	PendingPlacement.TileOccurrenceIndex = TileOccurrenceIndex;
	Zombie->SetActorHiddenInGame(true);
	Zombie->SetActorEnableCollision(false);
}

void FFPSSpawnManager::ProcessPendingTileZombiePlacements()
{
	UWorld* World = Owner.GetWorld();
	if (World == nullptr || PendingTileZombiePlacements.Num() == 0)
	{
		return;
	}

	for (int32 Index = PendingTileZombiePlacements.Num() - 1; Index >= 0; --Index)
	{
		FPendingTileZombiePlacement& PendingPlacement = PendingTileZombiePlacements[Index];
		ABaseZombie* Zombie = PendingPlacement.Zombie.Get();
		if (Zombie == nullptr)
		{
			PendingTileZombiePlacements.RemoveAtSwap(Index);
			continue;
		}

		FTransform TileWorldTransform;
		if (TryResolveTileZombieTransform(
			World,
			PendingPlacement.TileTypeCode,
			PendingPlacement.TileOccurrenceIndex,
			PendingPlacement.LocalLocation,
			PendingPlacement.LocalYaw,
			TileWorldTransform))
		{
			Zombie->SetActorLocationAndRotation(TileWorldTransform.GetLocation(), TileWorldTransform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
			FPSStage2WorldUtils::SnapActorToGround(Zombie);
			Zombie->SetActorHiddenInGame(false);
			Zombie->SetActorEnableCollision(true);
			SendZombiePlacementCorrection(PendingPlacement.ObjectId, Zombie->GetActorLocation(), Zombie->GetActorRotation());
			PendingTileZombiePlacements.RemoveAtSwap(Index);
			continue;
		}
	}
}

void FFPSSpawnManager::SendZombiePlacementCorrection(uint64 ObjectId, const FVector& WorldLocation, const FRotator& WorldRotation)
{
	if (!Owner.IsConnectedToGameServer() || ObjectId == 0)
	{
		return;
	}

	Protocol::C_MOVE MovePkt;
	Protocol::PosInfo* Info = MovePkt.mutable_info();
	Info->set_object_id(ObjectId);
	Info->set_x(WorldLocation.X);
	Info->set_y(WorldLocation.Y);
	Info->set_z(WorldLocation.Z);
	Info->set_yaw(WorldRotation.Yaw);
	Info->set_state(Protocol::MOVE_STATE_IDLE);

	Owner.SendPacket(ClientPacketHandler::MakeSendBuffer(MovePkt));
}

void FFPSSpawnManager::SpawnZombie(UWorld* World, const Protocol::ObjectInfo& ObjectInfo)
{
	if (World == nullptr || Owner.WorldObjects == nullptr)
	{
		return;
	}

	const uint64 ObjectId = ObjectInfo.object_id();
	if (Owner.WorldObjects->FindZombie(ObjectId) != nullptr)
	{
		return;
	}

	const int32 EncodedSpawnType = ObjectInfo.weapon_type();
	int32 TileTypeCode = 0;
	int32 TileOccurrenceIndex = 0;
	int32 ZombieTypeValue = EncodedSpawnType;
	if (EncodedSpawnType >= 100)
	{
		TileTypeCode = EncodedSpawnType / 100;
		TileOccurrenceIndex = (EncodedSpawnType % 100) / 10;
		ZombieTypeValue = EncodedSpawnType % 10;
	}
	else if (EncodedSpawnType >= 10)
	{
		TileTypeCode = EncodedSpawnType / 10;
		ZombieTypeValue = EncodedSpawnType % 10;
	}

	TSubclassOf<ABaseZombie> ZombieClass = Owner.NetworkZombieClass;
	if (ZombieTypeValue > static_cast<int32>(Protocol::ZOMBIE_TYPE_NONE))
	{
		const int32 ZombieClassIndex = ZombieTypeValue - static_cast<int32>(Protocol::ZOMBIE_TYPE_MELEE);
		if (Owner.NetworkZombieClasses.IsValidIndex(ZombieClassIndex) && Owner.NetworkZombieClasses[ZombieClassIndex])
		{
			ZombieClass = Owner.NetworkZombieClasses[ZombieClassIndex];
		}
	}

	if (ZombieClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] NetworkZombieClass is not assigned."));
		return;
	}

	FVector ZombieLocation(ObjectInfo.pos_info().x(), ObjectInfo.pos_info().y(), ObjectInfo.pos_info().z());
	const FVector RequestedZombieLocation = ZombieLocation;
	FRotator ZombieRotation(0.0f, ObjectInfo.pos_info().yaw(), 0.0f);
	bool bResolvedTileTransform = TileTypeCode == 0;
	if (TileTypeCode != 0)
	{
		FTransform TileWorldTransform;
		if (TryResolveTileZombieTransform(
			World,
			TileTypeCode,
			TileOccurrenceIndex,
			RequestedZombieLocation,
			ObjectInfo.pos_info().yaw(),
			TileWorldTransform))
		{
			ZombieLocation = TileWorldTransform.GetLocation();
			ZombieRotation = TileWorldTransform.Rotator();
			bResolvedTileTransform = true;
		}
	}

	FPSStage2WorldUtils::TryProjectLocationToGround(World, ZombieLocation, 120.0f, ZombieLocation);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABaseZombie* SpawnedZombie = World->SpawnActor<ABaseZombie>(ZombieClass, ZombieLocation, ZombieRotation, SpawnParams);
	if (SpawnedZombie == nullptr)
	{
		return;
	}

	FPSStage2WorldUtils::SnapActorToGround(SpawnedZombie);
	SpawnedZombie->SetNetworkObjectId(ObjectId);
	SpawnedZombie->SetActorTickEnabled(false);
	if (UCharacterMovementComponent* MoveComp = SpawnedZombie->GetCharacterMovement())
	{
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}
	if (AAIController* AIController = Cast<AAIController>(SpawnedZombie->GetController()))
	{
		AIController->Destroy();
	}

	Owner.WorldObjects->RegisterZombie(ObjectId, SpawnedZombie);
	if (TileTypeCode != 0 && !bResolvedTileTransform)
	{
		QueuePendingTileZombiePlacement(ObjectId, SpawnedZombie, RequestedZombieLocation, ObjectInfo.pos_info().yaw(), TileTypeCode, TileOccurrenceIndex);
	}
	else if (TileTypeCode != 0)
	{
		SendZombiePlacementCorrection(ObjectId, SpawnedZombie->GetActorLocation(), SpawnedZombie->GetActorRotation());
	}
}

void FFPSSpawnManager::SpawnLocalPlayer(UWorld* World, const FPlayerSpawnContext& SpawnContext)
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(&Owner, 0);
	if (World == nullptr || PlayerController == nullptr || Owner.WorldObjects == nullptr)
	{
		return;
	}

	const TSubclassOf<AFPSBaseCharacter> DesiredPlayerClass = Owner.ResolvePlayerCharacterClass(SpawnContext.ObjectId);
	AFPSBaseCharacter* CurrentPlayerPawn = Cast<AFPSBaseCharacter>(PlayerController->GetPawn());
	if (DesiredPlayerClass && (!CurrentPlayerPawn || CurrentPlayerPawn->GetClass() != DesiredPlayerClass.Get()))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFPSBaseCharacter* ReplacementPawn = World->SpawnActor<AFPSBaseCharacter>(
			DesiredPlayerClass,
			SpawnContext.Location,
			SpawnContext.Rotation,
			SpawnParams);
		if (ReplacementPawn)
		{
			APawn* PreviousPawn = PlayerController->GetPawn();
			PlayerController->Possess(ReplacementPawn);
			if (IsValid(PreviousPawn) && PreviousPawn != ReplacementPawn)
			{
				PreviousPawn->Destroy();
			}
			CurrentPlayerPawn = ReplacementPawn;
		}
	}

	Owner.MyPlayer = CurrentPlayerPawn ? CurrentPlayerPawn : Cast<AFPSBaseCharacter>(PlayerController->GetPawn());
	if (Owner.MyPlayer == nullptr)
	{
		return;
	}

	Protocol::PosInfo AppliedPosInfo;
	AppliedPosInfo.CopyFrom(SpawnContext.PosInfo);
	Owner.MyPlayer->SetPlayerInfo(AppliedPosInfo);
	Owner.MyPlayer->SetActorLocationAndRotation(
		SpawnContext.Location,
		SpawnContext.Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	FPSStage2WorldUtils::SnapActorToGround(Owner.MyPlayer);

	const FVector SnappedLocation = Owner.MyPlayer->GetActorLocation();
	AppliedPosInfo.set_x(SnappedLocation.X);
	AppliedPosInfo.set_y(SnappedLocation.Y);
	AppliedPosInfo.set_z(SnappedLocation.Z);
	Owner.MyPlayer->SetPlayerInfo(AppliedPosInfo);
	Owner.MyPlayer->SetActorHiddenInGame(false);
	Owner.MyPlayer->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* MoveComp = Owner.MyPlayer->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	Owner.WorldObjects->RegisterPlayer(SpawnContext.ObjectId, Owner.MyPlayer);
	Owner.RetryPendingWeapon(SpawnContext.ObjectId);
	Owner.ApplyStageTimerToLocalUI();
	if (SpawnContext.bUsedStage2SpawnTransform)
	{
		Owner.MyPlayer->SyncMovementToServer();
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("[TruckDebug] SpawnMine ObjectId=%llu MyPlayer=%s Local=%d Pawn=%s"),
		SpawnContext.ObjectId,
		*GetNameSafe(Owner.MyPlayer),
		Owner.MyPlayer->IsLocallyControlled() ? 1 : 0,
		*GetNameSafe(PlayerController->GetPawn()));
	UE_LOG(LogTemp, Verbose, TEXT("[ZombieSync] My player spawned. PlayerId=%llu"), SpawnContext.ObjectId);
}

void FFPSSpawnManager::SpawnRemotePlayer(UWorld* World, const FPlayerSpawnContext& SpawnContext)
{
	if (World == nullptr || Owner.WorldObjects == nullptr)
	{
		return;
	}

	const TSubclassOf<AFPSBaseCharacter> SpawnPlayerClass = Owner.ResolvePlayerCharacterClass(SpawnContext.ObjectId);
	if (SpawnPlayerClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Stage2Spawn] Player class is null. ObjectId=%llu"), SpawnContext.ObjectId);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFPSBaseCharacter* OtherPlayer = World->SpawnActor<AFPSBaseCharacter>(
		SpawnPlayerClass,
		SpawnContext.Location,
		SpawnContext.Rotation,
		SpawnParams);
	if (OtherPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Stage2Spawn] Failed to spawn other player. ObjectId=%llu Class=%s Location=%s"),
			SpawnContext.ObjectId,
			*GetNameSafe(SpawnPlayerClass.Get()),
			*SpawnContext.Location.ToString());
		return;
	}

	Protocol::PosInfo AppliedPosInfo;
	AppliedPosInfo.CopyFrom(SpawnContext.PosInfo);
	OtherPlayer->SetPlayerInfo(AppliedPosInfo);
	OtherPlayer->SetActorLocationAndRotation(
		SpawnContext.Location,
		SpawnContext.Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	FPSStage2WorldUtils::SnapActorToGround(OtherPlayer);

	const FVector SnappedLocation = OtherPlayer->GetActorLocation();
	AppliedPosInfo.set_x(SnappedLocation.X);
	AppliedPosInfo.set_y(SnappedLocation.Y);
	AppliedPosInfo.set_z(SnappedLocation.Z);
	OtherPlayer->SetPlayerInfo(AppliedPosInfo);
	FPSStage2WorldUtils::RestoreNetworkCharacterVisibility(OtherPlayer);
	if (UCharacterMovementComponent* MoveComp = OtherPlayer->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	Owner.WorldObjects->RegisterPlayer(SpawnContext.ObjectId, OtherPlayer);
	UE_LOG(LogTemp, Verbose,
		TEXT("[TruckDebug] SpawnOther ObjectId=%llu OtherPlayer=%s Local=%d Location=%s Hidden=%d"),
		SpawnContext.ObjectId,
		*GetNameSafe(OtherPlayer),
		OtherPlayer->IsLocallyControlled() ? 1 : 0,
		*OtherPlayer->GetActorLocation().ToString(),
		OtherPlayer->IsHidden() ? 1 : 0);
	Owner.RetryPendingWeapon(SpawnContext.ObjectId);
}