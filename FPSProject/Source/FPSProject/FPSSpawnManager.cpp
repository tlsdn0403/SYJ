#include "FPSSpawnManager.h"

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

	if (Owner.NetworkZombieClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] NetworkZombieClass is not assigned."));
		return;
	}

	FVector ZombieLocation(ObjectInfo.pos_info().x(), ObjectInfo.pos_info().y(), ObjectInfo.pos_info().z());
	FPSStage2WorldUtils::TryProjectLocationToGround(World, ZombieLocation, 120.0f, ZombieLocation);
	const FRotator ZombieRotation(0.0f, ObjectInfo.pos_info().yaw(), 0.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABaseZombie* SpawnedZombie = World->SpawnActor<ABaseZombie>(Owner.NetworkZombieClass, ZombieLocation, ZombieRotation, SpawnParams);
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

	UE_LOG(LogTemp, Warning,
		TEXT("[TruckDebug] SpawnMine ObjectId=%llu MyPlayer=%s Local=%d Pawn=%s"),
		SpawnContext.ObjectId,
		*GetNameSafe(Owner.MyPlayer),
		Owner.MyPlayer->IsLocallyControlled() ? 1 : 0,
		*GetNameSafe(PlayerController->GetPawn()));
	UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] My player spawned. PlayerId=%llu"), SpawnContext.ObjectId);
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
	UE_LOG(LogTemp, Warning,
		TEXT("[TruckDebug] SpawnOther ObjectId=%llu OtherPlayer=%s Local=%d Location=%s Hidden=%d"),
		SpawnContext.ObjectId,
		*GetNameSafe(OtherPlayer),
		OtherPlayer->IsLocallyControlled() ? 1 : 0,
		*OtherPlayer->GetActorLocation().ToString(),
		OtherPlayer->IsHidden() ? 1 : 0);
	Owner.RetryPendingWeapon(SpawnContext.ObjectId);
}