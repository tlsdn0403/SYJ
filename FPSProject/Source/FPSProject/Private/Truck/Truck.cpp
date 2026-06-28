#include "Truck/Truck.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "HUD/InteractUIClass.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Weapon/MountedMachineGun.h"
#include "Zombie/BaseZombie.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/OverlapResult.h"
#include "CollisionShape.h"
#include "ClientPacketHandler.h"
#include "FPSProject.h"
#include "FPSProjectGameInstance.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "HUD/InventoryWidget.h"
#include "HUD/EffectUI.h"
#include "Characters/FPSPlayerController.h"
#include "FPSStage2WorldUtils.h"
#include "Stage2/Stage2TileManager.h"
#include "EngineUtils.h"

namespace
{
bool AreCargoSlotsConfigured(const TArray<UStaticMeshComponent*>& Slots)
{
	for (const UStaticMeshComponent* Slot : Slots)
	{
		if (Slot && Slot->GetStaticMesh() != nullptr)
		{
			return true;
		}
	}

	return false;
}

void SetCargoSlotShown(UStaticMeshComponent* Slot, bool bShown)
{
	if (!Slot)
	{
		return;
	}

	Slot->SetVisibility(bShown, true);
	Slot->SetHiddenInGame(!bShown, true);
	Slot->MarkRenderStateDirty();
}

void EnsureVehicleMeshPhysicsReady(USkeletalMeshComponent* TruckMesh, bool bWakeBodies)
{
	if (!TruckMesh)
	{
		return;
	}

	TruckMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (!TruckMesh->IsSimulatingPhysics())
	{
		TruckMesh->SetSimulatePhysics(true);
		TruckMesh->RecreatePhysicsState();
	}

	if (bWakeBodies)
	{
		TruckMesh->WakeAllRigidBodies();
	}
	else
	{
		TruckMesh->PutAllRigidBodiesToSleep();
	}

	TruckMesh->RefreshBoneTransforms();
	TruckMesh->MarkRenderTransformDirty();
	TruckMesh->MarkRenderDynamicDataDirty();
}

void RefreshVehicleMeshRenderState(USkeletalMeshComponent* TruckMesh)
{
	if (!TruckMesh)
	{
		return;
	}

	TruckMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	TruckMesh->RefreshBoneTransforms();
	TruckMesh->MarkRenderTransformDirty();
	TruckMesh->MarkRenderDynamicDataDirty();
}

void MakeVehicleMeshKinematic(USkeletalMeshComponent* TruckMesh)
{
	if (!TruckMesh)
	{
		return;
	}

	RefreshVehicleMeshRenderState(TruckMesh);

	if (TruckMesh->IsSimulatingPhysics())
	{
		TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		TruckMesh->PutAllRigidBodiesToSleep();
		TruckMesh->SetSimulatePhysics(false);
	}
}

FVector GetBoxSidePointForLocation(const UBoxComponent* Box, const FVector& FromLocation, FVector* OutSideNormal)
{
	if (!Box)
	{
		if (OutSideNormal)
		{
			*OutSideNormal = FVector::ForwardVector;
		}
		return FromLocation;
	}

	const FTransform BoxTransform = Box->GetComponentTransform();
	const FVector Extent = Box->GetUnscaledBoxExtent();
	const FVector LocalFrom = BoxTransform.InverseTransformPosition(FromLocation);
	FVector LocalPoint(
		FMath::Clamp(LocalFrom.X, -Extent.X, Extent.X),
		FMath::Clamp(LocalFrom.Y, -Extent.Y, Extent.Y),
		FMath::Clamp(LocalFrom.Z, -Extent.Z, Extent.Z));

	const bool bOutsideX = FMath::Abs(LocalFrom.X) > Extent.X;
	const bool bOutsideY = FMath::Abs(LocalFrom.Y) > Extent.Y;
	FVector LocalNormal = FVector::ZeroVector;

	if (bOutsideX || bOutsideY)
	{
		LocalNormal = FVector(LocalFrom.X - LocalPoint.X, LocalFrom.Y - LocalPoint.Y, 0.0f).GetSafeNormal();
	}

	if (LocalNormal.IsNearlyZero())
	{
		const float DistanceToXFace = Extent.X - FMath::Abs(LocalFrom.X);
		const float DistanceToYFace = Extent.Y - FMath::Abs(LocalFrom.Y);
		if (DistanceToXFace < DistanceToYFace)
		{
			const float Sign = LocalFrom.X >= 0.0f ? 1.0f : -1.0f;
			LocalNormal = FVector(Sign, 0.0f, 0.0f);
			LocalPoint.X = Sign * Extent.X;
		}
		else
		{
			const float Sign = LocalFrom.Y >= 0.0f ? 1.0f : -1.0f;
			LocalNormal = FVector(0.0f, Sign, 0.0f);
			LocalPoint.Y = Sign * Extent.Y;
		}
	}

	FVector WorldNormal = BoxTransform.TransformVectorNoScale(LocalNormal);
	WorldNormal.Z = 0.0f;
	WorldNormal.Normalize();
	if (WorldNormal.IsNearlyZero())
	{
		WorldNormal = Box->GetForwardVector().GetSafeNormal2D();
	}

	if (OutSideNormal)
	{
		*OutSideNormal = WorldNormal;
	}

	return BoxTransform.TransformPosition(LocalPoint);
}

bool ComponentNameMatches(const UActorComponent* Component, const FName& ComponentName)
{
	return Component &&
		!ComponentName.IsNone() &&
		(Component->GetFName() == ComponentName ||
			Component->GetName().Contains(ComponentName.ToString(), ESearchCase::IgnoreCase));
}

bool NiagaraIdentityContains(const UNiagaraComponent* Component, const TCHAR* Needle)
{
	if (!Component || !Needle)
	{
		return false;
	}

	FString Identity = Component->GetName();
	if (const UNiagaraSystem* Asset = Component->GetAsset())
	{
		Identity += TEXT(" ");
		Identity += Asset->GetName();
		Identity += TEXT(" ");
		Identity += Asset->GetPathName();
	}

	return Identity.Contains(Needle, ESearchCase::IgnoreCase);
}
}

ATruck::ATruck()
{
	DriverSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("DriverSeatInteractTrigger"));
	DriverSeatInteractTrigger->SetupAttachment(RootComponent);
	DriverSeatInteractTrigger->InitSphereRadius(200.0f);
	DriverSeatInteractTrigger->InteractType = ETruckInteractType::DriverSeat;

	DriverSeatInteractWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("DriverSeatInteractWidget"));
	DriverSeatInteractWidget->SetupAttachment(DriverSeatInteractTrigger);
	DriverSeatInteractWidget->SetTwoSided(true);
	DriverSeatInteractWidget->SetWidgetSpace(EWidgetSpace::Screen);
	DriverSeatInteractWidget->SetHiddenInGame(true);
	DriverSeatInteractWidget->SetVisibility(false);

	CargoSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("CargoSeatInteractTrigger"));
	CargoSeatInteractTrigger->SetupAttachment(RootComponent);
	CargoSeatInteractTrigger->InitSphereRadius(200.0f);
	CargoSeatInteractTrigger->InteractType = ETruckInteractType::CargoSeat;

	CargoSeatInteractWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("CargoSeatInteractWidget"));
	CargoSeatInteractWidget->SetupAttachment(CargoSeatInteractTrigger);
	CargoSeatInteractWidget->SetTwoSided(true);
	CargoSeatInteractWidget->SetWidgetSpace(EWidgetSpace::Screen);
	CargoSeatInteractWidget->SetHiddenInGame(true);
	CargoSeatInteractWidget->SetVisibility(false);

	DriverSeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeatPoint"));
	DriverSeatPoint->SetupAttachment(RootComponent);
	DriverSeatPoint->SetRelativeLocation(FVector(110.0f, -10.0f, 120.0f));
	DriverSeatPoint->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	DriverExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DriverExitPoint"));
	DriverExitPoint->SetupAttachment(RootComponent);
	DriverExitPoint->SetRelativeLocation(FVector(130.0f, -170.0f, 20.0f));

	TurretSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("TurretSeatInteractTrigger"));
	TurretSeatInteractTrigger->SetupAttachment(RootComponent);
	TurretSeatInteractTrigger->InitSphereRadius(200.0f);
	TurretSeatInteractTrigger->InteractType = ETruckInteractType::TurretSeat;
	TurretSeatInteractTrigger->bRequiresTruckCargo = true;
	TurretSeatInteractTrigger->SetRelativeLocation(FVector(-80.0f, 0.0f, 140.0f));

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	CargoRidePoint = CreateDefaultSubobject<USceneComponent>(TEXT("CargoRidePoint"));
	CargoRidePoint->SetupAttachment(RootComponent);

	CargoExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CargoExitPoint"));
	CargoExitPoint->SetupAttachment(RootComponent);

	TurretSeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TurretSeatPoint"));
	TurretSeatPoint->SetupAttachment(RootComponent);
	TurretSeatPoint->SetRelativeLocation(FVector(-60.0f, 0.0f, 140.0f));

	TurretMountPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TurretMountPoint"));
	TurretMountPoint->SetupAttachment(RootComponent);
	TurretMountPoint->SetRelativeLocation(FVector(-120.0f, 0.0f, 200.0f));

	TurretCameraPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TurretCameraPoint"));
	TurretCameraPoint->SetupAttachment(RootComponent);
	TurretCameraPoint->SetRelativeLocation(FVector(-110.0f, 0.0f, 210.0f));

	TurretInteractWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("TurretInteractWidget"));
	TurretInteractWidget->SetupAttachment(TurretSeatInteractTrigger);
	TurretInteractWidget->SetTwoSided(true);
	TurretInteractWidget->SetWidgetSpace(EWidgetSpace::Screen);
	TurretInteractWidget->SetHiddenInGame(true);
	TurretInteractWidget->SetVisibility(false);

	EngineAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudioComponent->SetupAttachment(RootComponent);
	EngineAudioComponent->bAutoActivate = false;

	ZombieStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("ZombieStimuliSource"));

	CargoOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("CargoOrigin"));
	CargoOrigin->SetupAttachment(RootComponent);
	CargoOrigin->SetRelativeLocation(FVector(-120.0f, 0.0f, 80.0f));

	CargoMoveBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoMoveBounds"));
	CargoMoveBounds->SetupAttachment(RootComponent);
	CargoMoveBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CargoMoveBounds->SetBoxExtent(FVector(120.f, 80.f, 100.f));

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("AmmoSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		NewSlot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewSlot->SetSimulatePhysics(false);
		NewSlot->SetEnableGravity(false);
		AmmoSlots.Add(NewSlot);
	}

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("MountedAmmoSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		NewSlot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewSlot->SetSimulatePhysics(false);
		NewSlot->SetEnableGravity(false);
		MountedAmmoSlots.Add(NewSlot);
	}

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("FuelSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		NewSlot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewSlot->SetSimulatePhysics(false);
		NewSlot->SetEnableGravity(false);
		FuelSlots.Add(NewSlot);
	}

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("RepairKitSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		NewSlot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewSlot->SetSimulatePhysics(false);
		NewSlot->SetEnableGravity(false);
		RepairKitSlots.Add(NewSlot);
	}

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("MedKitSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		NewSlot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewSlot->SetSimulatePhysics(false);
		NewSlot->SetEnableGravity(false);
		MedKitSlots.Add(NewSlot);
	}

	CargoFloorCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoFloorCollision"));
	CargoFloorCollision->SetupAttachment(RootComponent);

	CargoLeftWallCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoLeftWallCollision"));
	CargoLeftWallCollision->SetupAttachment(RootComponent);

	CargoRightWallCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoRightWallCollision"));
	CargoRightWallCollision->SetupAttachment(RootComponent);

	CargoFrontWallCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoFrontWallCollision"));
	CargoFrontWallCollision->SetupAttachment(RootComponent);

	CargoBackWallCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoBackWallCollision"));
	CargoBackWallCollision->SetupAttachment(RootComponent);

	VehiclePawnCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehiclePawnCollision"));
	VehiclePawnCollision->SetupAttachment(GetMesh());
	VehiclePawnCollision->SetBoxExtent(FVector(260.0f, 120.0f, 100.0f));
	VehiclePawnCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VehiclePawnCollision->SetCollisionObjectType(ECC_Vehicle);
	VehiclePawnCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	VehiclePawnCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	VehiclePawnCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	VehiclePawnCollision->SetGenerateOverlapEvents(true);
	VehiclePawnCollision->SetNotifyRigidBodyCollision(true);

	auto SetupCargoCollision = [](UBoxComponent* Box)
		{
			// Cargo bounds should constrain character movement, but must not feed impulses back into the vehicle body.
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Box->SetGenerateOverlapEvents(false);
		};

	SetupCargoCollision(CargoFloorCollision);
	SetupCargoCollision(CargoLeftWallCollision);
	SetupCargoCollision(CargoRightWallCollision);
	SetupCargoCollision(CargoFrontWallCollision);
	SetupCargoCollision(CargoBackWallCollision);

	MountedWeaponRelativeTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.0f));
	MountedWeaponClass = AMountedMachineGun::StaticClass();

	static ConstructorHelpers::FClassFinder<AMountedMachineGun> MountedWeaponBP(TEXT("/Game/Truck/MachineGun/BP_MountedMachineGun"));
	UE_LOG(LogTemp, Verbose, TEXT("MountedWeaponBP success: %d, class: %s"),
		MountedWeaponBP.Succeeded(),
		*GetNameSafe(MountedWeaponBP.Class));
	if (MountedWeaponBP.Succeeded())
	{
		MountedWeaponClass = MountedWeaponBP.Class;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> TurretWidgetBP(TEXT("/Game/Item/WBP_Interact"));
	if (TurretWidgetBP.Succeeded())
	{
		if (DriverSeatInteractWidget)
		{
			DriverSeatInteractWidget->SetWidgetClass(TurretWidgetBP.Class);
		}
		if (CargoSeatInteractWidget)
		{
			CargoSeatInteractWidget->SetWidgetClass(TurretWidgetBP.Class);
		}
		if (TurretInteractWidget)
		{
			TurretInteractWidget->SetWidgetClass(TurretWidgetBP.Class);
		}
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> ZombieCrashSoundAsset(
		TEXT("/Game/Sound/crashZombie.crashZombie"));
	if (ZombieCrashSoundAsset.Succeeded())
	{
		ZombieCrashSound = ZombieCrashSoundAsset.Object;
	}
}

void ATruck::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureVehiclePawnCollision();
}

void ATruck::ConfigureVehiclePawnCollision()
{
	USkeletalMeshComponent* TruckMesh = GetMesh();
	if (!bAutoFitVehiclePawnCollision || !VehiclePawnCollision || !TruckMesh || !TruckMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	// CalcBounds with an identity transform gives mesh-local bounds. The blocker is
	// attached directly to the mesh, so it stays aligned even while Chaos moves it.
	const FBoxSphereBounds LocalBounds = TruckMesh->CalcBounds(FTransform::Identity);
	const FVector SafePadding(
		FMath::Max(0.0f, VehiclePawnCollisionPadding.X),
		FMath::Max(0.0f, VehiclePawnCollisionPadding.Y),
		FMath::Max(0.0f, VehiclePawnCollisionPadding.Z));

	VehiclePawnCollision->SetRelativeLocation(LocalBounds.Origin);
	VehiclePawnCollision->SetRelativeRotation(FRotator::ZeroRotator);
	VehiclePawnCollision->SetBoxExtent(LocalBounds.BoxExtent + SafePadding);
}

FVector ATruck::GetClosestZombieInteractionPoint(const FVector& FromLocation) const
{
	if (VehiclePawnCollision)
	{
		return GetBoxSidePointForLocation(VehiclePawnCollision, FromLocation, nullptr);
	}

	if (const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		FVector ClosestPoint = GetActorLocation();
		if (PrimitiveComponent->GetClosestPointOnCollision(FromLocation, ClosestPoint) >= 0.0f)
		{
			return ClosestPoint;
		}
	}

	return GetActorLocation();
}

FVector ATruck::GetZombieApproachLocation(const FVector& FromLocation, float StandOffDistance) const
{
	const float SafeStandOffDistance = FMath::Max(0.0f, StandOffDistance);
	FVector SideNormal = FVector::ZeroVector;
	FVector SurfacePoint = GetActorLocation();

	if (VehiclePawnCollision)
	{
		SurfacePoint = GetBoxSidePointForLocation(VehiclePawnCollision, FromLocation, &SideNormal);
	}
	else if (const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		if (PrimitiveComponent->GetClosestPointOnCollision(FromLocation, SurfacePoint) < 0.0f)
		{
			SurfacePoint = GetActorLocation();
		}

		SideNormal = FromLocation - SurfacePoint;
		SideNormal.Z = 0.0f;
		SideNormal.Normalize();
	}

	if (SideNormal.IsNearlyZero())
	{
		SideNormal = (FromLocation - GetActorLocation()).GetSafeNormal2D();
	}

	if (SideNormal.IsNearlyZero())
	{
		SideNormal = GetActorForwardVector().GetSafeNormal2D();
	}

	FVector ApproachLocation = SurfacePoint + SideNormal * SafeStandOffDistance;
	ApproachLocation.Z = FromLocation.Z;
	return ApproachLocation;
}

void ATruck::BeginPlay()
{
	Super::BeginPlay();
	const FTransform PlacedTransform = GetActorTransform();
	ConfigureVehiclePawnCollision();
	ResolveTruckSmokeComponents();

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &ATruck::HandleTruckHealthChanged);
		HealthComponent->SetMaxHealth(TruckMaxHealth, true);
	}
	ApplyStageVehicleTuning();

	TruckMaxFuel = FMath::Max(1.0f, TruckMaxFuel);
	CurrentTruckFuel = bUseFuel
		? FMath::Clamp(TruckStartingFuel, 0.0f, TruckMaxFuel)
		: TruckMaxFuel;
	OnTruckFuelChanged.Broadcast(CurrentTruckFuel, TruckMaxFuel);

	if (ZombieStimuliSource)
	{
		ZombieStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
		ZombieStimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
		ZombieStimuliSource->RegisterWithPerceptionSystem();
	}

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		// Characters are network-controlled pawns. Letting them block a simulated
		// Chaos vehicle makes every client produce a different contact impulse.
		// Zombie impacts are handled by CheckZombieImpactSweep instead.
		TruckMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		TruckMesh->SetNotifyRigidBodyCollision(true);
		TruckMesh->OnComponentHit.AddDynamic(this, &ATruck::OnTruckMeshHit);
	}

	if (VehiclePawnCollision)
	{
		VehiclePawnCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		VehiclePawnCollision->SetCollisionObjectType(ECC_Vehicle);
		VehiclePawnCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
		VehiclePawnCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		VehiclePawnCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		VehiclePawnCollision->SetGenerateOverlapEvents(true);
		VehiclePawnCollision->OnComponentHit.AddDynamic(this, &ATruck::OnTruckMeshHit);
	}

	// The driver's client is the only physics authority for the truck.
	// Until a local driver is assigned, keep the vehicle kinematic on every client.
	SetLocallyDriven(false);
	SetActorTransform(PlacedTransform, false, nullptr, ETeleportType::TeleportPhysics);
	RefreshVehicleMeshRenderState(GetMesh());

	if (EngineSoundCue)
	{
		EngineAudioComponent->SetSound(EngineSoundCue);
	}

	if (TurretInteractWidget)
	{
		TurretInteractWidget->InitWidget();
	}
	if (CargoSeatInteractWidget)
	{
		CargoSeatInteractWidget->InitWidget();
	}
	if (DriverSeatInteractWidget)
	{
		DriverSeatInteractWidget->InitWidget();
	}

	if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
	{
		GameInstance->RefreshStage2StartupActorHold();
	}

	if (TurretSeatInteractTrigger)
	{
		TurretSeatInteractTrigger->OnEnter.AddDynamic(this, &ATruck::OnTurretInteractEnter);
		TurretSeatInteractTrigger->OnExit.AddDynamic(this, &ATruck::OnTurretInteractExit);
	}
	if (CargoSeatInteractTrigger)
	{
		CargoSeatInteractTrigger->OnEnter.AddDynamic(this, &ATruck::OnCargoInteractEnter);
		CargoSeatInteractTrigger->OnExit.AddDynamic(this, &ATruck::OnCargoInteractExit);
	}
	if (DriverSeatInteractTrigger)
	{
		DriverSeatInteractTrigger->OnEnter.AddDynamic(this, &ATruck::OnDriverInteractEnter);
		DriverSeatInteractTrigger->OnExit.AddDynamic(this, &ATruck::OnDriverInteractExit);
	}

	for (UStaticMeshComponent* Slot : AmmoSlots) { SetCargoSlotShown(Slot, false); }
	for (UStaticMeshComponent* Slot : MountedAmmoSlots) { SetCargoSlotShown(Slot, false); }
	for (UStaticMeshComponent* Slot : FuelSlots) { SetCargoSlotShown(Slot, false); }
	for (UStaticMeshComponent* Slot : RepairKitSlots) { SetCargoSlotShown(Slot, false); }
	for (UStaticMeshComponent* Slot : MedKitSlots) { SetCargoSlotShown(Slot, false); }
	for (UStaticMeshComponent* Slot : AmmoSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	for (UStaticMeshComponent* Slot : MountedAmmoSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	for (UStaticMeshComponent* Slot : FuelSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	for (UStaticMeshComponent* Slot : RepairKitSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	for (UStaticMeshComponent* Slot : MedKitSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }

	if (MountedWeaponClass && TurretMountPoint)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UE_LOG(LogTemp, Verbose, TEXT("Spawn Mounted Weapon"));

		MountedWeapon = GetWorld()->SpawnActor<AMountedMachineGun>(
			MountedWeaponClass,
			TurretMountPoint->GetComponentTransform(),
			SpawnParams);

		if (MountedWeapon)
		{
			MountedWeapon->AttachToComponent(TurretMountPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			MountedWeapon->SetActorRelativeLocation(MountedWeaponRelativeTransform.GetLocation());
			MountedWeapon->SetActorRelativeRotation(MountedWeaponRelativeTransform.Rotator());
			MountedWeapon->SetActorRelativeScale3D(MountedWeaponRelativeTransform.GetScale3D());
			MountedWeapon->ConfigureOperatorSeat(TurretSeatPoint->GetComponentTransform());
		}
	}

	RefreshTruckSmokeEffects();
}

void ATruck::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckZombieImpactSweep();
	RefreshTruckSmokeEffects();

	if (bIsLocallyDriven || NetworkTruckId == 0)
	{
		ReportZombieAwarenessNoise(DeltaTime);
	}

	if (bIsLocallyDriven)
	{
		if (bCinematicControlLocked)
		{
			ClearDrivingInput(true);
			TruckMovePacketSendTimer = 0.0f;
			DebugTransformLogTimer = 0.0f;

			if (EngineAudioComponent && EngineAudioComponent->IsPlaying())
			{
				EngineAudioComponent->Stop();
			}
			return;
		}

		UpdateFuelConsumption(DeltaTime);

		DebugTransformLogTimer += DeltaTime;
		if (DebugTransformLogTimer >= 1.0f)
		{
			DebugTransformLogTimer = 0.0f;

			const FVector ActorLocation = GetActorLocation();
			const FRotator ActorRotation = GetActorRotation();
			const USkeletalMeshComponent* TruckMesh = GetMesh();
			const FVector MeshLocation = TruckMesh ? TruckMesh->GetComponentLocation() : FVector::ZeroVector;
			const FRotator MeshRotation = TruckMesh ? TruckMesh->GetComponentRotation() : FRotator::ZeroRotator;

			AController* CurrentController = GetController();
			AActor* ViewTarget = CurrentController ? CurrentController->GetViewTarget() : nullptr;
			const FVector ViewLocation = ViewTarget ? ViewTarget->GetActorLocation() : FVector::ZeroVector;
			const FRotator ViewRotation = ViewTarget ? ViewTarget->GetActorRotation() : FRotator::ZeroRotator;

			UE_LOG(LogTemp, Verbose,
				TEXT("[TruckDebug] Truck=%s ActorLoc=%s ActorRot=%s MeshLoc=%s MeshRot=%s ViewTarget=%s ViewLoc=%s ViewRot=%s Driver=%s"),
				*GetName(),
				*ActorLocation.ToString(),
				*ActorRotation.ToString(),
				*MeshLocation.ToString(),
				*MeshRotation.ToString(),
				*GetNameSafe(ViewTarget),
				*ViewLocation.ToString(),
				*ViewRotation.ToString(),
				*GetNameSafe(DriverCharacter));
		}

		TruckMovePacketSendTimer += DeltaTime;
		if (TruckMovePacketSendTimer >= TRUCK_MOVE_PACKET_SEND_DELAY)
		{
			TruckMovePacketSendTimer = 0.0f;
			SendTruckMovePacket();
		}

		if (!EngineAudioComponent->IsPlaying())
		{
			EngineAudioComponent->Play();
		}

		UpdateEngineSound();
		UpdateBrakeSound();
	}
	else
	{
		if (EngineAudioComponent && EngineAudioComponent->IsPlaying())
		{
			EngineAudioComponent->Stop();
		}
	}
}

void ATruck::ReportZombieAwarenessNoise(float DeltaTime)
{
	ZombieNoiseTimer += DeltaTime;

	const float Speed = GetVelocity().Size();
	if (Speed < ZombieNoiseMinSpeed)
	{
		ZombieNoiseTimer = FMath::Min(ZombieNoiseTimer, ZombieNoiseInterval);
		return;
	}

	if (ZombieNoiseTimer < ZombieNoiseInterval)
	{
		return;
	}

	ZombieNoiseTimer = 0.0f;

	const float Loudness = FMath::GetMappedRangeValueClamped(
		FVector2D(ZombieNoiseMinSpeed, ZombieNoiseMaxSpeed),
		FVector2D(0.6f, 1.35f),
		Speed);

	UAISense_Hearing::ReportNoiseEvent(
		GetWorld(),
		GetActorLocation(),
		Loudness,
		this,
		ZombieNoiseRange,
		FName(TEXT("TruckNoise")));
}

void ATruck::SendTruckMovePacket(bool bAllowHealthIncrease)
{
	if (NetworkTruckId == 0)
	{
		return;
	}

	Protocol::C_TRUCK_MOVE MovePkt;
	Protocol::PosInfo* Info = MovePkt.mutable_info();
	Info->set_object_id(NetworkTruckId);
	Info->set_x(GetActorLocation().X);
	Info->set_y(GetActorLocation().Y);
	Info->set_z(GetActorLocation().Z);
	const FRotator TruckRotation = GetActorRotation();
	Info->set_yaw(TruckRotation.Yaw);
	Info->set_pitch(TruckRotation.Pitch);
	Info->set_roll(TruckRotation.Roll);
	Info->set_state(GetVelocity().SizeSquared() > KINDA_SMALL_NUMBER ? Protocol::MOVE_STATE_RUN : Protocol::MOVE_STATE_IDLE);
	MovePkt.set_has_truck_fuel(true);
	MovePkt.set_fuel(CurrentTruckFuel);
	MovePkt.set_has_truck_health(true);
	MovePkt.set_truck_hp(GetTruckHealth());
	MovePkt.set_truck_max_hp(GetTruckMaxHealth());
	if (bAllowHealthIncrease)
	{
		MovePkt.set_has_truck_health_repair(true);
	}

	SEND_PACKET(MovePkt);
}

void ATruck::SyncTruckStateToServer(bool bAllowHealthIncrease)
{
	SendTruckMovePacket(bAllowHealthIncrease);
}

void ATruck::SetLocallyDriven(bool bLocallyDriven)
{
	bIsLocallyDriven = bLocallyDriven;
	ApplyStageVehicleTuning();
	if (bLocallyDriven)
	{
		bHasNetworkSmokeSample = false;
		NetworkSmokeSpeed = 0.0f;
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("[TruckDebug] SetLocallyDriven Truck=%s bLocallyDriven=%d Controller=%s IsPlayerControlled=%d"),
		*GetNameSafe(this),
		bLocallyDriven ? 1 : 0,
		*GetNameSafe(GetController()),
		IsPlayerControlled() ? 1 : 0);

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetComponentTickEnabled(bLocallyDriven && !bCinematicControlLocked);
	}

	ClearDrivingInput(!bLocallyDriven || bCinematicControlLocked);

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		if (bLocallyDriven && !bCinematicControlLocked)
		{
			EnsureVehicleMeshPhysicsReady(TruckMesh, true);
		}
		else
		{
			MakeVehicleMeshKinematic(TruckMesh);
		}
	}
}

void ATruck::SetCinematicControlLocked(bool bLocked)
{
	if (bCinematicControlLocked == bLocked)
	{
		return;
	}

	bCinematicControlLocked = bLocked;
	TruckMovePacketSendTimer = 0.0f;
	DebugTransformLogTimer = 0.0f;
	ClearDrivingInput(bCinematicControlLocked || !bIsLocallyDriven);

	if (UChaosWheeledVehicleMovementComponent* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetComponentTickEnabled(bIsLocallyDriven && !bCinematicControlLocked);
	}

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		if (bCinematicControlLocked)
		{
			MakeVehicleMeshKinematic(TruckMesh);
		}
		else if (bIsLocallyDriven)
		{
			EnsureVehicleMeshPhysicsReady(TruckMesh, true);
			TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
		else
		{
			MakeVehicleMeshKinematic(TruckMesh);
		}
	}

	SetInteractionWidgetsHidden(bCinematicControlLocked);

	if (!bCinematicControlLocked)
	{
		RefreshLocalInteractionWidgets();
	}
}

void ATruck::SetDriverCharacter(AFPSBaseCharacter* Character)
{
	DriverCharacter = Character;
	RefreshLocalInteractionWidgets();
}

void ATruck::SetMountedWeaponUser(AFPSBaseCharacter* Character)
{
	MountedWeaponUser = Character;
	RefreshLocalInteractionWidgets();
}

void ATruck::ApplyNetworkTransform(const FVector& TargetLocation, const FRotator& TargetRotation, bool bForceCorrection)
{
	if (bIsLocallyDriven)
	{
		// Accepted packets are echoed to everyone, including the driver. Only an
		// explicit server correction is allowed to override local physics authority.
		if (!bForceCorrection)
		{
			return;
		}

		if (USkeletalMeshComponent* TruckMesh = GetMesh())
		{
			EnsureVehicleMeshPhysicsReady(TruckMesh, true);
			TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}

		SetActorLocationAndRotation(
			TargetLocation,
			TargetRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		if (USkeletalMeshComponent* TruckMesh = GetMesh())
		{
			TruckMesh->WakeAllRigidBodies();
			TruckMesh->RefreshBoneTransforms();
			TruckMesh->MarkRenderTransformDirty();
			TruckMesh->MarkRenderDynamicDataDirty();
		}
		return;
	}

	if (NetworkTruckId != 0)
	{
		UpdateNetworkSmokeSpeedFromTransform(TargetLocation);
	}

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		MakeVehicleMeshKinematic(TruckMesh);
	}

	SetActorLocationAndRotation(
		TargetLocation,
		TargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		if (TruckMesh->IsSimulatingPhysics())
		{
			TruckMesh->PutAllRigidBodiesToSleep();
		}
		RefreshVehicleMeshRenderState(TruckMesh);
	}

	RefreshTruckSmokeEffects();
}

void ATruck::SetLoadingPhase(bool bLoadingPhase)
{
	if (bIsLoadingPhase == bLoadingPhase)
	{
		return;
	}

	bIsLoadingPhase = bLoadingPhase;
	UE_LOG(LogTemp, Log, TEXT("[Truck] Loading phase changed. Truck=%s bIsLoadingPhase=%d"),
		*GetNameSafe(this),
		bIsLoadingPhase ? 1 : 0);
	RefreshTruckSmokeEffects();
	RefreshLocalInteractionWidgets();
}

FVector ATruck::GetCargoRideLocation() const
{
	return CargoRidePoint ? CargoRidePoint->GetComponentLocation() : GetActorLocation();
}

FRotator ATruck::GetCargoRideRotation() const
{
	return CargoRidePoint ? CargoRidePoint->GetComponentRotation() : GetActorRotation();
}

FVector ATruck::GetDriverSeatLocation() const
{
	return DriverSeatPoint ? DriverSeatPoint->GetComponentLocation() : GetActorLocation();
}

FRotator ATruck::GetDriverSeatRotation() const
{
	return DriverSeatPoint ? DriverSeatPoint->GetComponentRotation() : GetActorRotation();
}

FVector ATruck::GetDriverExitLocation() const
{
	if (DriverExitPoint)
	{
		const FTransform UprightTruckTransform(GetUprightExitRotation(), GetActorLocation());
		return UprightTruckTransform.TransformPosition(DriverExitPoint->GetRelativeLocation());
	}

	return GetActorLocation() - GetUprightExitRotation().RotateVector(FVector::RightVector) * 200.0f;
}

FRotator ATruck::GetUprightExitRotation() const
{
	return FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
}

FVector ATruck::GetCargoExitLocation() const
{
	if (CargoExitPoint)
	{
		const FTransform UprightTruckTransform(GetUprightExitRotation(), GetActorLocation());
		return UprightTruckTransform.TransformPosition(CargoExitPoint->GetRelativeLocation());
	}

	return GetActorLocation() + GetUprightExitRotation().RotateVector(FVector::RightVector) * 200.0f;
}

FVector ATruck::GetTurretSeatLocation() const
{
	return TurretSeatPoint ? TurretSeatPoint->GetComponentLocation() : GetActorLocation();
}

FRotator ATruck::GetTurretSeatRotation() const
{
	return TurretSeatPoint ? TurretSeatPoint->GetComponentRotation() : GetActorRotation();
}

FVector ATruck::GetTurretCameraLocation() const
{
	return TurretCameraPoint ? TurretCameraPoint->GetComponentLocation() : GetActorLocation();
}

FRotator ATruck::GetTurretCameraRotation() const
{
	return TurretCameraPoint ? TurretCameraPoint->GetComponentRotation() : GetActorRotation();
}

FBox ATruck::GetCargoWorldBounds() const
{
	if (!CargoMoveBounds)
	{
		return FBox(EForceInit::ForceInitToZero);
	}

	const FVector Center = CargoMoveBounds->GetComponentLocation();
	const FVector Extent = CargoMoveBounds->GetScaledBoxExtent();

	return FBox(Center - Extent, Center + Extent);
}

UPrimitiveComponent* ATruck::GetCargoMovementBase() const
{
	return CargoFloorCollision ? CargoFloorCollision : Cast<UPrimitiveComponent>(RootComponent);
}

void ATruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("Throttle", this, &ATruck::MoveForward);
	PlayerInputComponent->BindAxis("Steer", this, &ATruck::MoveRight);
	PlayerInputComponent->BindAxis("Brake", this, &ATruck::Brake);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ATruck::ExitDriverSeat);
	PlayerInputComponent->BindAction("UseHealPack", IE_Pressed, this, &ATruck::UseDriverHealPack);
	PlayerInputComponent->BindAction("RecoverTruckUpright", IE_Pressed, this, &ATruck::RecoverTruckUpright);
}

float ATruck::GetTruckHealth() const
{
	return HealthComponent ? HealthComponent->GetHealth() : 0.0f;
}

float ATruck::GetTruckMaxHealth() const
{
	return HealthComponent ? HealthComponent->MaxGetHealth() : TruckMaxHealth;
}

void ATruck::RepairTruck(float RepairAmount)
{
	if (HealthComponent && RepairAmount > 0.0f && !bTruckDestroyed)
	{
		HealthComponent->Heal(RepairAmount);
	}
}

void ATruck::ApplyNetworkHealth(float CurrentHealth, float MaxHealth)
{
	if (!HealthComponent || MaxHealth <= 0.0f)
	{
		return;
	}

	const float ClampedMaxHealth = FMath::Max(MaxHealth, 1.0f);
	const float ClampedCurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, ClampedMaxHealth);
	bApplyingNetworkHealth = true;
	if (!FMath::IsNearlyEqual(HealthComponent->MaxGetHealth(), ClampedMaxHealth))
	{
		HealthComponent->SetMaxHealth(ClampedMaxHealth, false);
	}
	if (!FMath::IsNearlyEqual(HealthComponent->GetHealth(), ClampedCurrentHealth))
	{
		HealthComponent->SetCurrentHealth(ClampedCurrentHealth);
	}
	bApplyingNetworkHealth = false;
	RefreshTruckSmokeEffects();
}

void ATruck::ResetVehiclePhysicsState(bool bReleaseBrake)
{
	CurrentThrottleInput = 0.0f;
	bBrakePressedLastFrame = false;

	if (UChaosWheeledVehicleMovementComponent* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(0.0f);
		MoveComp->SetSteeringInput(0.0f);
		MoveComp->SetBrakeInput(bReleaseBrake ? 0.0f : 1.0f);
	}

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		EnsureVehicleMeshPhysicsReady(TruckMesh, true);
		TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		TruckMesh->WakeAllRigidBodies();
		RefreshVehicleMeshRenderState(TruckMesh);
	}

	ApplyStageVehicleTuning();
}

void ATruck::ApplyStageVehicleTuning()
{
	UChaosWheeledVehicleMovementComponent* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!MoveComp)
	{
		return;
	}

	if (!bHasOriginalEngineMaxTorque)
	{
		OriginalEngineMaxTorque = MoveComp->EngineSetup.MaxTorque;
		bHasOriginalEngineMaxTorque = true;
	}

	const bool bStage2World = FPSStage2WorldUtils::IsStage2World(GetWorld());
	const float TargetTorque = bStage2World
		? OriginalEngineMaxTorque * FMath::Max(1.0f, Stage2EngineTorqueMultiplier)
		: OriginalEngineMaxTorque;

	MoveComp->SetMaxEngineTorque(TargetTorque);
}

void ATruck::ResolveTruckSmokeComponents()
{
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!IsValid(NiagaraComponent))
		{
			continue;
		}

		NiagaraComponent->SetAutoActivate(false);
		NiagaraComponent->SetVisibility(false, true);
		NiagaraComponent->SetHiddenInGame(true, true);
		if (NiagaraComponent->IsActive())
		{
			NiagaraComponent->Deactivate();
		}

		if (!WhiteSmokeComponent && ComponentNameMatches(NiagaraComponent, WhiteSmokeComponentName))
		{
			WhiteSmokeComponent = NiagaraComponent;
		}
		else if (!BlackSmokeComponent && ComponentNameMatches(NiagaraComponent, BlackSmokeComponentName))
		{
			BlackSmokeComponent = NiagaraComponent;
		}
	}

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!IsValid(NiagaraComponent))
		{
			continue;
		}

		if (!BlackSmokeComponent &&
			(NiagaraIdentityContains(NiagaraComponent, TEXT("truck_attack")) ||
				NiagaraIdentityContains(NiagaraComponent, TEXT("black")) ||
				NiagaraIdentityContains(NiagaraComponent, TEXT("attack"))))
		{
			BlackSmokeComponent = NiagaraComponent;
			continue;
		}

		if (!WhiteSmokeComponent &&
			(NiagaraIdentityContains(NiagaraComponent, TEXT("white")) ||
				NiagaraIdentityContains(NiagaraComponent, TEXT("NS_Smoke")) ||
				NiagaraIdentityContains(NiagaraComponent, TEXT("smoke"))))
		{
			WhiteSmokeComponent = NiagaraComponent;
		}
	}

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!IsValid(NiagaraComponent))
		{
			continue;
		}

		if (!WhiteSmokeComponent && NiagaraComponent != BlackSmokeComponent)
		{
			WhiteSmokeComponent = NiagaraComponent;
		}
		else if (!BlackSmokeComponent && NiagaraComponent != WhiteSmokeComponent)
		{
			BlackSmokeComponent = NiagaraComponent;
		}
	}
}

void ATruck::RefreshTruckSmokeEffects()
{
	if (!WhiteSmokeComponent && !BlackSmokeComponent)
	{
		ResolveTruckSmokeComponents();
	}

	const bool bSmokeAllowed = FPSStage2WorldUtils::IsStage2World(GetWorld()) && !bIsLoadingPhase;
	const float CurrentSpeed = GetTruckSmokeEvaluationSpeed();
	const float MaxHealth = FMath::Max(GetTruckMaxHealth(), 1.0f);
	const float HealthRatio = GetTruckHealth() / MaxHealth;

	const bool bShouldShowWhiteSmoke =
		bSmokeAllowed &&
		!bTruckDestroyed &&
		CurrentSpeed >= FMath::Max(0.0f, WhiteSmokeMinSpeed);
	const bool bShouldShowBlackSmoke =
		bSmokeAllowed &&
		HealthRatio <= FMath::Clamp(BlackSmokeHealthRatioThreshold, 0.0f, 1.0f);

	if (bWhiteSmokeActive != bShouldShowWhiteSmoke || !bShouldShowWhiteSmoke)
	{
		bWhiteSmokeActive = bShouldShowWhiteSmoke;
		SetSmokeComponentActive(WhiteSmokeComponent.Get(), bWhiteSmokeActive);
	}

	if (bBlackSmokeActive != bShouldShowBlackSmoke || !bShouldShowBlackSmoke)
	{
		bBlackSmokeActive = bShouldShowBlackSmoke;
		SetSmokeComponentActive(BlackSmokeComponent.Get(), bBlackSmokeActive);
	}
}

void ATruck::SetSmokeComponentActive(UNiagaraComponent* SmokeComponent, bool bShouldBeActive)
{
	if (!IsValid(SmokeComponent))
	{
		return;
	}

	SmokeComponent->SetAutoActivate(false);
	SmokeComponent->SetVisibility(bShouldBeActive, true);
	SmokeComponent->SetHiddenInGame(!bShouldBeActive, true);

	if (bShouldBeActive)
	{
		if (!SmokeComponent->IsActive())
		{
			SmokeComponent->Activate(true);
		}
		return;
	}

	if (SmokeComponent->IsActive())
	{
		SmokeComponent->Deactivate();
	}
}

void ATruck::UpdateNetworkSmokeSpeedFromTransform(const FVector& TargetLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (bHasNetworkSmokeSample)
	{
		const float DeltaTime = CurrentTime - LastNetworkSmokeSampleTime;
		if (DeltaTime > KINDA_SMALL_NUMBER)
		{
			NetworkSmokeSpeed = FVector::Dist(TargetLocation, LastNetworkSmokeLocation) / DeltaTime;
			LastNetworkSmokeUpdateTime = CurrentTime;
		}
	}
	else
	{
		LastNetworkSmokeUpdateTime = CurrentTime;
		bHasNetworkSmokeSample = true;
	}

	LastNetworkSmokeLocation = TargetLocation;
	LastNetworkSmokeSampleTime = CurrentTime;
}

float ATruck::GetTruckSmokeEvaluationSpeed() const
{
	if (bIsLocallyDriven || NetworkTruckId == 0)
	{
		return GetVelocity().Size();
	}

	const UWorld* World = GetWorld();
	if (!World || !bHasNetworkSmokeSample)
	{
		return 0.0f;
	}

	const float TimeSinceLastServerUpdate = World->GetTimeSeconds() - LastNetworkSmokeUpdateTime;
	return TimeSinceLastServerUpdate <= FMath::Max(0.0f, NetworkSmokeSpeedTimeout)
		? NetworkSmokeSpeed
		: 0.0f;
}

void ATruck::UseDriverHealPack()
{
	if (DriverCharacter)
	{
		DriverCharacter->UseHealPack();
	}
}

void ATruck::HandleTruckHealthChanged(float NewHealth, float Damage)
{
	OnTruckHealthChanged.Broadcast(NewHealth, GetTruckMaxHealth());
	RefreshTruckSmokeEffects();
	if (!bApplyingNetworkHealth && NetworkTruckId != 0)
	{
		SyncTruckStateToServer(Damage < 0.0f);
	}

	if (NewHealth > 0.0f)
	{
		bTruckDestroyed = false;
		return;
	}

	if (bTruckDestroyed)
	{
		return;
	}

	bTruckDestroyed = true;
	if (UChaosWheeledVehicleMovementComponent* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		CurrentThrottleInput = 0.0f;
		MoveComp->SetThrottleInput(0.0f);
		MoveComp->SetSteeringInput(0.0f);
		MoveComp->SetBrakeInput(1.0f);
	}

	OnTruckDestroyed.Broadcast();
	RefreshTruckSmokeEffects();
	UE_LOG(LogTemp, Warning, TEXT("Truck %s was destroyed."), *GetName());
}

void ATruck::MoveForward(float Value)
{
	/*UE_LOG(LogTemp, Warning, TEXT("Throttle Input: %f"), Value);*/
	if (bCinematicControlLocked)
	{
		ClearDrivingInput(true);
		return;
	}

	const bool bCanAccelerate = !bTruckDestroyed && HasTruckFuel();
	CurrentThrottleInput = bCanAccelerate ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(CurrentThrottleInput);
	}
}

void ATruck::SetTruckFuel(float NewFuel)
{
	const float SafeMaxFuel = FMath::Max(1.0f, TruckMaxFuel);
	const float ClampedFuel = FMath::Clamp(NewFuel, 0.0f, SafeMaxFuel);
	if (FMath::IsNearlyEqual(CurrentTruckFuel, ClampedFuel))
	{
		return;
	}

	CurrentTruckFuel = ClampedFuel;
	OnTruckFuelChanged.Broadcast(CurrentTruckFuel, SafeMaxFuel);

	if (bUseFuel && CurrentTruckFuel <= KINDA_SMALL_NUMBER)
	{
		CurrentThrottleInput = 0.0f;
		if (UChaosWheeledVehicleMovementComponent* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
		{
			MoveComp->SetThrottleInput(0.0f);
		}
	}
}

void ATruck::RefuelTruck(float FuelAmount)
{
	if (FuelAmount > 0.0f)
	{
		SetTruckFuel(CurrentTruckFuel + FuelAmount);
		SyncTruckStateToServer();
	}
}

void ATruck::UpdateFuelConsumption(float DeltaTime)
{
	if (!bUseFuel || bTruckDestroyed || DeltaTime <= 0.0f ||
		FuelConsumptionPerSecond <= 0.0f || FMath::IsNearlyZero(CurrentThrottleInput))
	{
		return;
	}

	const float FuelUsed = FuelConsumptionPerSecond * FMath::Abs(CurrentThrottleInput) * DeltaTime;
	SetTruckFuel(CurrentTruckFuel - FuelUsed);
}

void ATruck::ClearDrivingInput(bool bHoldBrake)
{
	CurrentThrottleInput = 0.0f;
	bBrakePressedLastFrame = false;

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(0.0f);
		MoveComp->SetSteeringInput(0.0f);
		MoveComp->SetBrakeInput(bHoldBrake ? 1.0f : 0.0f);
	}
}

void ATruck::MoveRight(float Value)
{
	if (bCinematicControlLocked)
	{
		ClearDrivingInput(true);
		return;
	}

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetSteeringInput(bTruckDestroyed ? 0.0f : Value);
	}
}

void ATruck::Brake(float Value)
{
	if (bCinematicControlLocked)
	{
		ClearDrivingInput(true);
		return;
	}

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetBrakeInput(bTruckDestroyed ? 1.0f : Value);

		const float Speed = GetVelocity().Size();
		const bool bBrakePressed = Value > 0.5f;
		if (bBrakePressed && !bBrakePressedLastFrame && Speed > BrakeSoundMinSpeed)
		{
			if (BrakeSound)
			{
				//UGameplayStatics::PlaySoundAtLocation(this, BrakeSound, GetActorLocation());
			}
		}

		bBrakePressedLastFrame = bBrakePressed;
	}
}

void ATruck::RecoverTruckUpright()
{
	if (!CanRecoverTruckUpright())
	{
		return;
	}

	FVector RecoveryLocation = GetActorLocation();
	if (!TryGetUprightRecoveryLocation(RecoveryLocation))
	{
		RecoveryLocation += FVector(0.0f, 0.0f, UprightRecoveryGroundClearance);
	}

	FRotator RecoveryRotation(0.0f, GetActorRotation().Yaw, 0.0f);
	RecoveryRotation.Normalize();

	ClearDrivingInput(false);
	TruckMovePacketSendTimer = 0.0f;
	DebugTransformLogTimer = 0.0f;

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		EnsureVehicleMeshPhysicsReady(TruckMesh, true);
		TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	SetActorLocationAndRotation(
		RecoveryLocation,
		RecoveryRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		TruckMesh->WakeAllRigidBodies();
		RefreshVehicleMeshRenderState(TruckMesh);
	}

	if (AController* TruckController = GetController())
	{
		TruckController->SetControlRotation(RecoveryRotation);
	}

	TeleportOccupantsAfterUprightRecovery();
	SendTruckMovePacket();

	UE_LOG(LogTemp, Log,
		TEXT("[TruckRecovery] Recovered upright. Truck=%s Location=%s Rotation=%s Driver=%s"),
		*GetNameSafe(this),
		*RecoveryLocation.ToString(),
		*RecoveryRotation.ToString(),
		*GetNameSafe(DriverCharacter));
}

bool ATruck::CanRecoverTruckUpright() const
{
	if (!bIsLocallyDriven || bCinematicControlLocked)
	{
		return false;
	}

	if (!IsValid(DriverCharacter) ||
		!DriverCharacter->IsDrivingTruck() ||
		DriverCharacter->CurrentTruck != this)
	{
		return false;
	}

	return GetActorUpVector().Z <= FMath::Clamp(UprightRecoveryMaxUpDot, -1.0f, 1.0f);
}

bool ATruck::TryGetUprightRecoveryLocation(FVector& OutRecoveryLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TraceStart = CurrentLocation + FVector(0.0f, 0.0f, FMath::Max(0.0f, UprightRecoveryGroundTraceUpDistance));
	const FVector TraceEnd = CurrentLocation - FVector(0.0f, 0.0f, FMath::Max(0.0f, UprightRecoveryGroundTraceDownDistance));

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TruckUprightRecoveryGroundTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	if (DriverCharacter)
	{
		QueryParams.AddIgnoredActor(DriverCharacter);
	}

	if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	float ActorOriginToBottom = 120.0f;
	if (const USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		const FBoxSphereBounds MeshBounds = TruckMesh->Bounds;
		const float CurrentMeshBottomZ = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
		ActorOriginToBottom = FMath::Max(ActorOriginToBottom, CurrentLocation.Z - CurrentMeshBottomZ);
	}

	OutRecoveryLocation = FVector(
		CurrentLocation.X,
		CurrentLocation.Y,
		GroundHit.ImpactPoint.Z + ActorOriginToBottom + FMath::Max(0.0f, UprightRecoveryGroundClearance));
	return true;
}

void ATruck::TeleportOccupantsAfterUprightRecovery()
{
	if (IsValid(DriverCharacter) && DriverCharacter->IsDrivingTruck() && DriverCharacter->CurrentTruck == this)
	{
		DriverCharacter->AttachToComponent(DriverSeatPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		DriverCharacter->SetActorRelativeLocation(FVector::ZeroVector);
		DriverCharacter->SetActorRelativeRotation(FRotator::ZeroRotator);
	}

	if (IsValid(MountedWeaponUser) &&
		MountedWeaponUser->IsUsingMountedWeapon() &&
		MountedWeaponUser->CurrentTruck == this &&
		TurretSeatPoint)
	{
		MountedWeaponUser->AttachToComponent(TurretSeatPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MountedWeaponUser->SetActorRelativeLocation(FVector::ZeroVector);
		MountedWeaponUser->SetActorRelativeRotation(FRotator::ZeroRotator);
	}

	if (MountedWeapon && TurretMountPoint)
	{
		MountedWeapon->AttachToComponent(TurretMountPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MountedWeapon->SetActorRelativeLocation(MountedWeaponRelativeTransform.GetLocation());
		MountedWeapon->SetActorRelativeRotation(MountedWeaponRelativeTransform.Rotator());
		MountedWeapon->SetActorRelativeScale3D(MountedWeaponRelativeTransform.GetScale3D());
		MountedWeapon->ConfigureOperatorSeat(TurretSeatPoint ? TurretSeatPoint->GetComponentTransform() : TurretMountPoint->GetComponentTransform());
	}
}

void ATruck::CheckZombieImpactSweep()
{
	UWorld* World = GetWorld();
	UBoxComponent* ImpactCollision = VehiclePawnCollision;
	if (!World || !ImpactCollision)
	{
		return;
	}

	if (!bIsLocallyDriven && NetworkTruckId != 0)
	{
		return;
	}

	FVector VehicleVelocity = GetVelocity();
	const float ImpactSpeed = VehicleVelocity.Size();
	if (ImpactSpeed < ZombieImpactMinSpeed)
	{
		return;
	}

	const FVector ImpactDirection = VehicleVelocity.GetSafeNormal();
	if (ImpactDirection.IsNearlyZero())
	{
		return;
	}

	const FVector SweepCenter = ImpactCollision->GetComponentLocation();
	const FVector SweepExtent = ImpactCollision->GetScaledBoxExtent() + FVector(ZombieImpactContactTolerance);

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TruckZombieImpactSweep), false, this);

	if (!World->OverlapMultiByObjectType(
		Overlaps,
		SweepCenter,
		ImpactCollision->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(SweepExtent),
		QueryParams))
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ABaseZombie* Zombie = Cast<ABaseZombie>(Overlap.GetActor()))
		{
			FVector ImpactPoint = Zombie->GetActorLocation();
			FVector ClosestPoint;
			float DistanceToTruck = TNumericLimits<float>::Max();
			if (ImpactCollision->GetClosestPointOnCollision(Zombie->GetActorLocation(), ClosestPoint) >= 0.0f)
			{
				ImpactPoint = ClosestPoint;
				DistanceToTruck = FVector::Dist(Zombie->GetActorLocation(), ClosestPoint);
			}

			float AllowedContactDistance = ZombieImpactContactTolerance;
			if (const UCapsuleComponent* ZombieCapsule = Zombie->GetCapsuleComponent())
			{
				AllowedContactDistance += ZombieCapsule->GetScaledCapsuleRadius();
			}

			if (DistanceToTruck > AllowedContactDistance)
			{
				continue;
			}

			ProcessZombieImpact(Zombie, ImpactPoint, ImpactDirection, ImpactSpeed);
		}
	}
}

void ATruck::AddCargoVisual(EItemType ItemType)
{
	auto TryUseNextSlot = [](TArray<UStaticMeshComponent*>& Slots, int32& CurrentCount) -> UStaticMeshComponent*
	{
		while (CurrentCount < Slots.Num())
		{
			UStaticMeshComponent* CandidateSlot = Slots[CurrentCount++];
			if (CandidateSlot == nullptr)
			{
				continue;
			}

			if (CandidateSlot->GetStaticMesh() == nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("Cargo slot '%s' has no StaticMesh assigned."), *CandidateSlot->GetName());
				continue;
			}

			return CandidateSlot;
		}

		return nullptr;
	};

	TArray<UStaticMeshComponent*>* TargetSlots = nullptr;
	int32* TargetCount = nullptr;

	switch (ItemType)
	{
	case EItemType::Ammo:
	case EItemType::CharacterAmmo:
		TargetSlots = &AmmoSlots;
		TargetCount = &CurrentAmmoCount;
		break;

	case EItemType::MountedGunAmmo:
		if (AreCargoSlotsConfigured(MountedAmmoSlots))
		{
			TargetSlots = &MountedAmmoSlots;
			TargetCount = &CurrentMountedAmmoCount;
		}
		else
		{
			TargetSlots = &AmmoSlots;
			TargetCount = &CurrentAmmoCount;
		}
		break;

	case EItemType::Fuel:
		TargetSlots = &FuelSlots;
		TargetCount = &CurrentFuelCount;
		break;

	case EItemType::TruckRepairKit:
		if (AreCargoSlotsConfigured(RepairKitSlots))
		{
			TargetSlots = &RepairKitSlots;
			TargetCount = &CurrentRepairKitCount;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Truck repair kit cargo visual skipped because RepairKitSlots are not configured on truck %s"), *GetName());
			return;
		}
		break;

	case EItemType::MedicalKit:
	case EItemType::HealPack:
		TargetSlots = &MedKitSlots;
		TargetCount = &CurrentMedKitCount;
		break;

	default:
		break;
	}

	if (TargetSlots == nullptr || TargetCount == nullptr)
	{
		return;
	}

	if (UStaticMeshComponent* TargetSlot = TryUseNextSlot(*TargetSlots, *TargetCount))
	{
		SetCargoSlotShown(TargetSlot, true);
		UE_LOG(LogTemp, Log, TEXT("Cargo loaded visually at slot. Type: %d"), static_cast<int32>(ItemType));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("No cargo visual slot available for item type %d on truck %s. Cargo data is still loaded."), static_cast<int32>(ItemType), *GetName());
}

void ATruck::ApplyLoadedCargoItem(EItemType ItemType)
{
	TotalLoadedItems++;
	AddCargoVisual(ItemType);
}

void ATruck::Interact_Implementation(AFPSBaseCharacter* Character)
{
	UE_LOG(LogTemp, Verbose,
		TEXT("[TruckDebug] Interact Truck=%s Character=%s InteractType=%d Local=%d Driver=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Character),
		Character ? static_cast<int32>(Character->GetCurrentTruckInteractType()) : -1,
		Character && Character->IsLocallyControlled() ? 1 : 0,
		*GetNameSafe(DriverCharacter));

	if (!Character)
	{
		return;
	}

	if (bIsLoadingPhase)
	{
		if (Character->GetItemCount() > 0)
		{
			TArray<EItemType> ReceivedItems = Character->OffloadItems();

			if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
			{
				GameInstance->RecordStage1CargoItems(ReceivedItems);
			}

			for (EItemType Item : ReceivedItems)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[Stage1Cargo] Offload item type=%d"), static_cast<int32>(Item));
				ApplyLoadedCargoItem(Item);

				switch (Item)
				{
				case EItemType::Ammo:
				case EItemType::CharacterAmmo:
					UE_LOG(LogTemp, Log, TEXT("Loaded Character Ammo"));
					break;
				case EItemType::MountedGunAmmo:
					UE_LOG(LogTemp, Log, TEXT("Loaded Mounted Gun Ammo"));
					break;
				case EItemType::Fuel:
					UE_LOG(LogTemp, Log, TEXT("Loaded Fuel"));
					break;
				case EItemType::TruckRepairKit:
					UE_LOG(LogTemp, Log, TEXT("Loaded Truck Repair Kit"));
					break;
				case EItemType::MedicalKit:
				case EItemType::HealPack:
					UE_LOG(LogTemp, Log, TEXT("Loaded Heal Pack"));
					break;
				default:
					break;
				}
				AFPSPlayerController* PC = Cast<AFPSPlayerController>(Character->GetController());
				////플레이어 인벤토리 비우기
				//AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController());
				PC->InventoryW->ClearItem();
			}

			if (NetworkTruckId != 0)
			{
				if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
				{
					if (GameInstance->IsConnectedToGameServer())
					{
						Protocol::C_LOAD_TRUCK_ITEM LoadPkt;
						LoadPkt.set_truck_id(NetworkTruckId);
						for (EItemType Item : ReceivedItems)
						{
							UE_LOG(LogTemp, Verbose, TEXT("[Stage1Cargo] SendLoadTruckItem type=%d"), static_cast<int32>(Item));
							LoadPkt.add_item_types(static_cast<int32>(Item));
						}
						GameInstance->SendPacket(ClientPacketHandler::MakeSendBuffer(LoadPkt));
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Offloaded %d items to Truck!"), ReceivedItems.Num());
		}

		if (LoadItemSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, LoadItemSound, GetActorLocation());
		}

		return;
	}

	if (TryEnterMountedWeapon(Character))
	{
		return;
	}
	// 운전석 탑승
	if (Character->GetCurrentTruckInteractType() == ETruckInteractType::DriverSeat)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[TruckDebug] DriverSeatRequest Truck=%s Character=%s Local=%d"),
			*GetNameSafe(this),
			*GetNameSafe(Character),
			Character->IsLocallyControlled() ? 1 : 0);

		if (IsValid(DriverCharacter) && DriverCharacter != Character)
		{
			UE_LOG(LogTemp, Warning, TEXT("Driver seat already occupied by %s"), *GetNameSafe(DriverCharacter));
			return;
		}

		if (Character->IsLocallyControlled())
		{
			if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(Character->GetGameInstance()))
			{
				if (GameInstance->TryEnterTruckLocally(Character, this, Protocol::TRUCK_SEAT_DRIVER))
				{
					return;
				}
			}

			Protocol::C_ENTER_TRUCK EnterPkt;
			EnterPkt.set_truck_id(NetworkTruckId);
			EnterPkt.set_seat_type(Protocol::TRUCK_SEAT_DRIVER);
			SEND_PACKET(EnterPkt);
		}
	}
	else if (Character->GetCurrentTruckInteractType() == ETruckInteractType::CargoSeat)
	{
		UE_LOG(LogTemp, Log, TEXT("Cargo Seat!"));
		if (!Character->IsOnTruckCargo() && Character->IsLocallyControlled())
		{
			if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(Character->GetGameInstance()))
			{
				if (GameInstance->TryEnterTruckLocally(Character, this, Protocol::TRUCK_SEAT_CARGO))
				{
					return;
				}
			}

			Protocol::C_ENTER_TRUCK EnterPkt;
			EnterPkt.set_truck_id(NetworkTruckId);
			EnterPkt.set_seat_type(Protocol::TRUCK_SEAT_CARGO);
			SEND_PACKET(EnterPkt);
		}
	}
}

bool ATruck::TryEnterMountedWeapon(AFPSBaseCharacter* Character)
{
	/*if (!FPSStage2WorldUtils::IsStage2World(GetWorld()))
	{
		return false;
	}*/

	if (!Character || !MountedWeapon || bIsLoadingPhase ||
		!Character->IsOnTruckCargo() || Character->CurrentTruck != this)
	{
		return false;
	}

	const bool bOverlappingTurretSeat =
		TurretSeatInteractTrigger &&
		TurretSeatInteractTrigger->IsOverlappingActor(Character);
	const bool bRequestedTurretSeat =
		Character->GetCurrentTruckInteractType() == ETruckInteractType::TurretSeat;
	const bool bSwitchingFromCargo =
		bOverlappingTurretSeat ||
		FVector::Dist(Character->GetActorLocation(), GetTurretSeatLocation()) <= MountedWeaponUseDistance;

	if (!bRequestedTurretSeat && !bSwitchingFromCargo)
	{
		return false;
	}

	if (IsValid(MountedWeaponUser) && MountedWeaponUser != Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mounted weapon already in use by %s"), *GetNameSafe(MountedWeaponUser));
		return false;
	}

	if (Character->IsLocallyControlled())
	{
		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(Character->GetGameInstance()))
		{
			if (GameInstance->TryEnterTruckLocally(Character, this, Protocol::TRUCK_SEAT_TURRET))
			{
				return true;
			}
		}

		Protocol::C_ENTER_TRUCK EnterPkt;
		EnterPkt.set_truck_id(NetworkTruckId);
		EnterPkt.set_seat_type(Protocol::TRUCK_SEAT_TURRET);
		SEND_PACKET(EnterPkt);
	}
	return Character->IsLocallyControlled();
}

void ATruck::RefreshMachineGunAmmoFromCargo()
{
	SyncMachineGunReserveFromCargo();

	if (!bMachineGunMagazineInitialized && MachineGunCurrentAmmo <= 0 && MachineGunTotalAmmo > 0)
	{
		ReloadMachineGun();
	}
}

void ATruck::SyncMachineGunReserveFromCargo()
{
	const int32 SafeMaxAmmo = FMath::Max(MachineGunMaxAmmo, 0);
	const int32 SafeMountedAmmoCount = FMath::Max(CurrentMountedAmmoCount, 0);
	const int32 AmmoCountDelta = SafeMountedAmmoCount - LastSyncedMountedAmmoCount;

	if (AmmoCountDelta != 0)
	{
		MachineGunTotalAmmo = FMath::Max(MachineGunTotalAmmo + (AmmoCountDelta * SafeMaxAmmo), 0);
		LastSyncedMountedAmmoCount = SafeMountedAmmoCount;
	}
	else if (LastSyncedMountedAmmoCount == 0 && MachineGunTotalAmmo == 0 && SafeMountedAmmoCount > 0)
	{
		MachineGunTotalAmmo = SafeMountedAmmoCount * SafeMaxAmmo;
		LastSyncedMountedAmmoCount = SafeMountedAmmoCount;
	}

	MachineGunCurrentAmmo = FMath::Clamp(MachineGunCurrentAmmo, 0, SafeMaxAmmo);
}

void ATruck::ApplyNetworkMachineGunAmmo(int32 TotalAmmo, int32 CurrentAmmo, int32 MaxAmmo)
{
	MachineGunMaxAmmo = FMath::Max(MaxAmmo, 0);
	MachineGunTotalAmmo = FMath::Max(TotalAmmo, 0);
	MachineGunCurrentAmmo = FMath::Clamp(CurrentAmmo, 0, MachineGunMaxAmmo);
	LastSyncedMountedAmmoCount = FMath::Max(CurrentMountedAmmoCount, 0);
	bMachineGunMagazineInitialized = true;

	if (MountedWeapon)
	{
		MountedWeapon->SetMagazineAmmo(MachineGunCurrentAmmo, MachineGunMaxAmmo);
	}
}

bool ATruck::ConsumeMachineGunBullet()
{
	RefreshMachineGunAmmoFromCargo();

	if (MachineGunCurrentAmmo <= 0)
	{
		MachineGunCurrentAmmo = 0;
		return false;
	}

	--MachineGunCurrentAmmo;
	bMachineGunMagazineInitialized = true;

	return true;
}

bool ATruck::ReloadMachineGun()
{
	SyncMachineGunReserveFromCargo();

	MachineGunMaxAmmo = FMath::Max(MachineGunMaxAmmo, 0);
	MachineGunCurrentAmmo = FMath::Clamp(MachineGunCurrentAmmo, 0, MachineGunMaxAmmo);

	const int32 MissingAmmo = MachineGunMaxAmmo - MachineGunCurrentAmmo;
	if (MachineGunMaxAmmo <= 0 || MissingAmmo <= 0 || MachineGunTotalAmmo <= 0)
	{
		bMachineGunMagazineInitialized = MachineGunCurrentAmmo > 0;
		return false;
	}

	const int32 ReloadAmount = FMath::Min(MissingAmmo, MachineGunTotalAmmo);
	MachineGunCurrentAmmo += ReloadAmount;
	MachineGunTotalAmmo -= ReloadAmount;
	bMachineGunMagazineInitialized = true;

	if (MountedWeapon)
	{
		MountedWeapon->SetMagazineAmmo(MachineGunCurrentAmmo, MachineGunMaxAmmo);
	}

	return true;
}

void ATruck::ExitDriverSeat()
{
	if (!DriverCharacter)
	{
		return;
	}

	AFPSBaseCharacter* CharacterToRestore = DriverCharacter;

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(0.0f);
		MoveComp->SetSteeringInput(0.0f);
		MoveComp->SetBrakeInput(1.0f);
	}

	const bool bShouldHandleLocalExit =
		bIsLocallyDriven ||
		(GetController() && GetController()->IsLocalController());

	UE_LOG(LogTemp, Verbose,
		TEXT("[TruckDebug] ExitDriverSeat Truck=%s Driver=%s bShouldHandleLocalExit=%d bIsLocallyDriven=%d Controller=%s CharacterLocal=%d"),
		*GetNameSafe(this),
		*GetNameSafe(CharacterToRestore),
		bShouldHandleLocalExit ? 1 : 0,
		bIsLocallyDriven ? 1 : 0,
		*GetNameSafe(GetController()),
		CharacterToRestore->IsLocallyControlled() ? 1 : 0);

	if (bShouldHandleLocalExit)
	{
		// TCP preserves packet order, so the server receives this final transform
		// before the following exit request and freezes every client at the same pose.
		SendTruckMovePacket();

		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(CharacterToRestore->GetGameInstance()))
		{
			if (GameInstance->TryExitTruckLocally(CharacterToRestore))
			{
				return;
			}
		}

		Protocol::C_EXIT_TRUCK ExitPkt;
		SEND_PACKET(ExitPkt);
	}
}

void ATruck::OnTruckMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 유효한 엑터인지 검사
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	// 좀비가 아니거나 죽었으면 무시
	ABaseZombie* Zombie = Cast<ABaseZombie>(OtherActor);
	if (!Zombie || !Zombie->IsAlive())
	{
		return;
	}

	ProcessZombieImpact(Zombie, Hit.ImpactPoint, GetVelocity().GetSafeNormal(), GetVelocity().Size());
}

void ATruck::ProcessZombieImpact(ABaseZombie* Zombie, const FVector& ImpactPoint, const FVector& ImpactDirection, float ImpactSpeed)
{
	if (!bIsLocallyDriven && NetworkTruckId != 0)
	{
		return;
	}

	// 속도가 느리거나 좀비가 죽으면 무시
	if (!Zombie || !Zombie->IsAlive() || ImpactSpeed < ZombieImpactMinSpeed)
	{
		return;
	}

	// 좀비를 여러 번 쳐버리는 것을 방지
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (const float* LastImpactTime = LastZombieImpactTimes.Find(Zombie))
	{
		if (CurrentTime - *LastImpactTime < ZombieImpactCooldown)
		{
			return;
		}
	}
	// LastZombieImpactTimes 맵에 좀비의 마지막 충돌 시간을 업데이트
	LastZombieImpactTimes.Add(Zombie, CurrentTime);
	ApplyZombieImpactSpeedPenalty(ImpactSpeed);
	PlayLocalDriverZombieImpactBloodEffect();

	if (ZombieCrashSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ZombieCrashSound, ImpactPoint);
	}

	// 전달받은 충돌 방향이 거의 0 이면 트럭 전방으로 간주
	const FVector SafeImpactDirection = ImpactDirection.IsNearlyZero() ? GetActorForwardVector() : ImpactDirection.GetSafeNormal();

	// 좀비가 받을 데미지 속도에 따라 계산.
	const float Damage = FMath::GetMappedRangeValueClamped(
		FVector2D(ZombieImpactMinSpeed, ZombieImpactFatalSpeed),
		FVector2D(ZombieImpactMinDamage, ZombieImpactMaxDamage),
		ImpactSpeed);

	// 좀비의 넉백을 속도에 따라 조절
	const float KnockbackScale = FMath::GetMappedRangeValueClamped(
		FVector2D(ZombieImpactMinSpeed, ZombieImpactFatalSpeed),
		FVector2D(1.0f, 1.6f),
		ImpactSpeed);


	// Dedicated pawn blocker is also the single source of truth for zombie body contact.
	const FTransform CollisionTransform = VehiclePawnCollision
		? VehiclePawnCollision->GetComponentTransform()
		: GetActorTransform();
	const FVector LocalZombieLocation = CollisionTransform.InverseTransformPosition(Zombie->GetActorLocation());
	const FVector MeshExtent = VehiclePawnCollision
		? VehiclePawnCollision->GetUnscaledBoxExtent()
		: FVector(150.0f, 100.0f, 100.0f);
	// 좀비가 트럭 몸체 범위 안에 있는지 검사
	const bool bWithinTruckLength =
		LocalZombieLocation.X > -MeshExtent.X * 1.15f &&
		LocalZombieLocation.X < MeshExtent.X * 1.15f;
	const bool bWithinTruckWidth = FMath::Abs(LocalZombieLocation.Y) < MeshExtent.Y * 1.35f;
	const bool bWithinTruckHeight = FMath::Abs(LocalZombieLocation.Z) < MeshExtent.Z * 1.5f;
	const bool bTruckBodyImpact = bWithinTruckLength && bWithinTruckWidth && bWithinTruckHeight;

	// 트럭 중심에서 좀비가 어느 위치에 있는지
	const FVector LocalOutwardDirection = FVector(LocalZombieLocation.X, LocalZombieLocation.Y, 0.0f).GetSafeNormal();
	// 월드 좌표계로 트럭 중심에서 좀비가 있는 방향을 변환
	const FVector WorldOutwardDirection = LocalOutwardDirection.IsNearlyZero()
		? SafeImpactDirection
		: CollisionTransform.TransformVectorNoScale(LocalOutwardDirection).GetSafeNormal();
	// 좀비를 날려버릴 방향 결정
	const FVector FinalImpactDirection = (SafeImpactDirection * 0.7f + WorldOutwardDirection * 0.9f).GetSafeNormal();
	const FVector ImpactFlingDirection = FinalImpactDirection.IsNearlyZero() ? SafeImpactDirection : FinalImpactDirection;

	// 좀비 즉사 조건
	const bool bCheatFlingImpact = bTruckBodyImpact && ImpactSpeed >= ZombiePinnedImpactFatalSpeed;
	const bool bForceImpactRagdoll = ImpactSpeed >= ZombieImpactFatalSpeed || bCheatFlingImpact;
	const FVector LaunchVelocity =
		ImpactFlingDirection * (ZombieImpactKnockback * KnockbackScale * 1.2f) +
		FVector::UpVector * (ZombieImpactUpwardKnockback * KnockbackScale);
	const FVector WorldImpulse =
		ImpactFlingDirection * (ZombieImpactImpulse * KnockbackScale * 1.2f) +
		FVector::UpVector * (ZombieImpactImpulse * 0.2f * KnockbackScale);

	const bool bNetworkZombie = Zombie->GetNetworkObjectId() != 0;
	const bool bCanReportServerHit = bIsLocallyDriven || NetworkTruckId == 0;
	Zombie->PlayBloodHitEffect(ImpactPoint, ImpactFlingDirection);
	const bool bNetworkHitSent = bCanReportServerHit &&
		UFPSProjectGameInstance::SendZombieHitPacket(DriverCharacter, Zombie, Damage, ImpactPoint, NAME_None, ImpactFlingDirection);
	if (bNetworkZombie && (bNetworkHitSent || NetworkTruckId != 0))
	{
		Zombie->ApplyTruckImpactKnockback(
			LaunchVelocity,
			WorldImpulse,
			ImpactPoint,
			bForceImpactRagdoll,
			ZombieImpactCooldown + 0.35f);
		return;
	}

	// 좀비에게 데미지 줄 HitResult 생성
	FHitResult DamageHit;
	DamageHit.ImpactPoint = ImpactPoint;
	DamageHit.Location = ImpactPoint;

	// 좀비에게 데미지를 줌
	UGameplayStatics::ApplyPointDamage(
		Zombie,
		Damage,
		ImpactFlingDirection,
		DamageHit,
		GetController(),
		this,
		nullptr);

	if (Zombie->IsAlive() &&
		bForceImpactRagdoll)
	{
		Zombie->Die();
	}
	// 살아있다면 캐릭터를 넉백
	if (Zombie->IsAlive())
	{
		Zombie->LaunchCharacter(LaunchVelocity, true, true);
		return;
	}
	// 죽은 경우 레그돌 하여 넉백
	if (USkeletalMeshComponent* ZombieMesh = Zombie->GetMesh())
	{
		ZombieMesh->AddImpulseAtLocation(WorldImpulse, ImpactPoint, FName(TEXT("pelvis")));
	}
}

void ATruck::ApplyZombieImpactSpeedPenalty(float ImpactSpeed)
{
	if ((!bIsLocallyDriven && NetworkTruckId != 0) || bCinematicControlLocked)
	{
		return;
	}

	USkeletalMeshComponent* TruckMesh = GetMesh();
	if (!TruckMesh || !TruckMesh->IsSimulatingPhysics() || ImpactSpeed < ZombieImpactMinSpeed)
	{
		return;
	}

	const float MinLossRatio = FMath::Clamp(ZombieImpactMinSpeedLossRatio, 0.0f, 0.95f);
	const float MaxLossRatio = FMath::Clamp(ZombieImpactMaxSpeedLossRatio, MinLossRatio, 0.95f);
	const float LossRatio = FMath::GetMappedRangeValueClamped(
		FVector2D(ZombieImpactMinSpeed, ZombieImpactFatalSpeed),
		FVector2D(MinLossRatio, MaxLossRatio),
		ImpactSpeed);
	if (LossRatio <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector CurrentVelocity = TruckMesh->GetPhysicsLinearVelocity();
	const FVector CurrentHorizontalVelocity(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
	if (CurrentHorizontalVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector NewHorizontalVelocity = CurrentHorizontalVelocity * (1.0f - LossRatio);
	const FVector NewVelocity(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, CurrentVelocity.Z);
	TruckMesh->SetPhysicsLinearVelocity(NewVelocity, false);
	TruckMesh->SetPhysicsAngularVelocityInDegrees(
		TruckMesh->GetPhysicsAngularVelocityInDegrees() * (1.0f - LossRatio * 0.35f),
		false);
	TruckMesh->WakeAllRigidBodies();

	if (bIsLocallyDriven && NetworkTruckId != 0)
	{
		TruckMovePacketSendTimer = 0.0f;
		SendTruckMovePacket();
	}
}

void ATruck::PlayLocalDriverZombieImpactBloodEffect()
{
	if (!FPSStage2WorldUtils::IsStage2World(GetWorld()) || !IsValid(DriverCharacter))
	{
		return;
	}

	if (DriverCharacter->CurrentTruck != this || !DriverCharacter->IsDrivingTruck())
	{
		return;
	}

	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!LocalPlayerController)
	{
		return;
	}

	const bool bLocalControllerOwnsTruck = GetController() == LocalPlayerController;
	const bool bLocalDriver = bIsLocallyDriven || bLocalControllerOwnsTruck;
	if (!bLocalDriver)
	{
		return;
	}

	AFPSPlayerController* FPSPlayerController = Cast<AFPSPlayerController>(LocalPlayerController);
	if (!FPSPlayerController || !FPSPlayerController->EffectW)
	{
		return;
	}

	FPSPlayerController->EffectW->SpawnBloodEffects(0.45f);
}

void ATruck::EndMountedWeaponUse(AFPSBaseCharacter* Character)
{
	if (MountedWeaponUser == Character)
	{
		if (MountedWeapon)
		{
			MountedWeapon->SetWeaponUser(nullptr);
		}

		SetMountedWeaponUser(nullptr);
	}
}

void ATruck::RefreshInteractionWidgetsForCharacter(AFPSBaseCharacter* Character)
{
	if (Character)
	{
		const bool bIsTruckOccupant =
			Character->CurrentTruck == this &&
			(Character->IsDrivingTruck() || Character->IsOnTruckCargo() || Character->IsUsingMountedWeapon());

		if (bIsTruckOccupant)
		{
			Character->ShowTruckHealthOnHUD(this);
		}
	}

	if (Character && VehiclePawnCollision)
	{
		// Seat state is applied only after the server broadcasts S_ENTER_TRUCK / S_EXIT_TRUCK.
		// Mirror that authoritative state into the local movement-query ignore lists so
		// cargo riders can walk inside the blocker while every outside pawn is blocked.
		const bool bIsOccupant =
			Character->CurrentTruck == this &&
			(Character->IsDrivingTruck() || Character->IsOnTruckCargo() || Character->IsUsingMountedWeapon());
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->IgnoreComponentWhenMoving(VehiclePawnCollision, bIsOccupant);
			VehiclePawnCollision->IgnoreComponentWhenMoving(Capsule, bIsOccupant);
		}
	}

	if (Character && MountedWeapon && Character->CurrentTruck == this && Character->IsUsingMountedWeapon())
	{
		MountedWeapon->AttachUserToOperatorSeat(Character);
	}

	if (!IsLocalInteractionCharacter(Character))
	{
		return;
	}

	if (bCinematicControlLocked)
	{
		SetInteractionWidgetsHidden(true);
		if (Character->GetCurrentInteractableActor() == this)
		{
			Character->SetInteractableActor(nullptr);
			Character->SetCurrentTruckInteractType(ETruckInteractType::None);
		}
		return;
	}

	SetInteractionWidgetsHidden(false);

	const bool bCharacterIsAnyTruckOccupant =
		Character->CurrentTruck != nullptr ||
		Character->IsOnTruckCargo() ||
		Character->IsDrivingTruck() ||
		Character->IsUsingMountedWeapon();
	const bool bCharacterIsFree = !bCharacterIsAnyTruckOccupant;

	const bool bCanUseDriverSeat =
		!bIsLoadingPhase &&
		bCharacterIsFree &&
		!IsValid(DriverCharacter) &&
		DriverSeatInteractTrigger &&
		DriverSeatInteractTrigger->IsOverlappingActor(Character);

	const bool bCanUseCargoSeat =
		bCharacterIsFree &&
		CargoSeatInteractTrigger &&
		CargoSeatInteractTrigger->IsOverlappingActor(Character);

	const bool bCanUseTurret =
		!bIsLoadingPhase &&
		Character->CurrentTruck == this &&
		Character->IsOnTruckCargo() &&
		!Character->IsUsingMountedWeapon() &&
		!IsValid(MountedWeaponUser) &&
		TurretSeatInteractTrigger &&
		TurretSeatInteractTrigger->IsOverlappingActor(Character);

	// Driver and cargo trigger volumes can overlap. Pick the nearest valid one so
	// the prompt and the interaction performed by the character always agree.
	ETruckInteractType SelectedType = ETruckInteractType::None;
	const USceneComponent* SelectedPromptComponent = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	auto ConsiderPrompt = [&](bool bCanUse, ETruckInteractType Type, const USceneComponent* PromptComponent)
		{
			if (!bCanUse || !PromptComponent)
			{
				return;
			}

			const float DistanceSquared = FVector::DistSquared(
				Character->GetActorLocation(),
				PromptComponent->GetComponentLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				SelectedType = Type;
				SelectedPromptComponent = PromptComponent;
			}
		};

	ConsiderPrompt(bCanUseDriverSeat, ETruckInteractType::DriverSeat, DriverSeatInteractTrigger);
	ConsiderPrompt(bCanUseCargoSeat, ETruckInteractType::CargoSeat, CargoSeatInteractTrigger);
	ConsiderPrompt(bCanUseTurret, ETruckInteractType::TurretSeat, TurretSeatInteractTrigger);

	if (SelectedType != ETruckInteractType::None)
	{
		Character->SetInteractableActor(this);
		Character->SetCurrentTruckInteractType(SelectedType);
	}
	else if (Character->GetCurrentInteractableActor() == this)
	{
		Character->SetInteractableActor(nullptr);
		Character->SetCurrentTruckInteractType(ETruckInteractType::None);
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const ETruckInteractType PreviousVisiblePromptType = VisibleInteractPromptType;
	ETruckInteractType PromptTypeToShow = SelectedType;
	if (SelectedType != ETruckInteractType::None && SelectedPromptComponent)
	{
		const float SelectedDistanceSquared = FVector::DistSquared(
			Character->GetActorLocation(),
			SelectedPromptComponent->GetComponentLocation());
		const bool bCurrentlyVisiblePrompt = VisibleInteractPromptType == SelectedType;
		const float AllowedPromptDistance = bCurrentlyVisiblePrompt
			? FMath::Max(TruckInteractPromptShowDistance, TruckInteractPromptKeepDistance)
			: TruckInteractPromptShowDistance;
		if (AllowedPromptDistance > 0.0f && SelectedDistanceSquared > FMath::Square(AllowedPromptDistance))
		{
			PromptTypeToShow = ETruckInteractType::None;
		}
	}

	const bool bSuppressPromptReappear =
		PromptTypeToShow != ETruckInteractType::None &&
		PreviousVisiblePromptType == ETruckInteractType::None &&
		(CurrentTime - LastInteractPromptHiddenTime) < TruckInteractPromptReappearCooldown;

	PromptTypeToShow = bSuppressPromptReappear ? ETruckInteractType::None : PromptTypeToShow;
	const bool bPromptChanged = PromptTypeToShow != PreviousVisiblePromptType;
	if (bPromptChanged)
	{
		if (PromptTypeToShow == ETruckInteractType::None)
		{
			LastInteractPromptHiddenTime = CurrentTime;
		}

		VisibleInteractPromptType = PromptTypeToShow;
	}

	auto UpdatePrompt = [PromptTypeToShow, bPromptChanged](
		UWidgetComponent* WidgetComponent,
		ETruckInteractType PromptType,
		const FText& PromptText)
		{
			UInteractUIClass* UI = Cast<UInteractUIClass>(
				WidgetComponent ? WidgetComponent->GetUserWidgetObject() : nullptr);

			const bool bShouldShowPrompt = PromptTypeToShow == PromptType;
			if (WidgetComponent)
			{
				WidgetComponent->SetHiddenInGame(!bShouldShowPrompt, true);
				WidgetComponent->SetVisibility(bShouldShowPrompt, true);
			}

			if (!UI || !bShouldShowPrompt)
			{
				return;
			}

			UI->SetInteractText(PromptText);
			if (bPromptChanged)
			{
				UI->PlayAni_PopUp(false);
			}
		};

	UpdatePrompt(
		DriverSeatInteractWidget,
		ETruckInteractType::DriverSeat,
		FText::FromString(TEXT("트럭 운전하기")));
	UpdatePrompt(
		CargoSeatInteractWidget,
		ETruckInteractType::CargoSeat,
		FText::FromString(bIsLoadingPhase
			? TEXT("트럭에 아이템 적재하기")
			: TEXT("트럭 트렁크 탑승")));
	UpdatePrompt(
		TurretInteractWidget,
		ETruckInteractType::TurretSeat,
		FText::FromString(TEXT("기관총 사용하기")));
}

void ATruck::OnDriverInteractEnter(AActor* OtherActor)
{
	RefreshInteractionWidgetsForCharacter(Cast<AFPSBaseCharacter>(OtherActor));
}

void ATruck::OnDriverInteractExit(AActor* OtherActor)
{
	RefreshInteractionWidgetsForCharacter(Cast<AFPSBaseCharacter>(OtherActor));
}

void ATruck::OnCargoInteractEnter(AActor* OtherActor)
{
	RefreshInteractionWidgetsForCharacter(Cast<AFPSBaseCharacter>(OtherActor));
}

void ATruck::OnCargoInteractExit(AActor* OtherActor)
{
	RefreshInteractionWidgetsForCharacter(Cast<AFPSBaseCharacter>(OtherActor));
}

void ATruck::OnTurretInteractEnter(AActor* OtherActor)
{
	RefreshInteractionWidgetsForCharacter(Cast<AFPSBaseCharacter>(OtherActor));
}

void ATruck::OnTurretInteractExit(AActor* OtherActor)
{
	RefreshInteractionWidgetsForCharacter(Cast<AFPSBaseCharacter>(OtherActor));
}

void ATruck::RefreshLocalInteractionWidgets()
{
	AFPSBaseCharacter* LocalCharacter = nullptr;
	if (const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
	{
		LocalCharacter = GameInstance->MyPlayer;
	}

	if (!IsValid(LocalCharacter))
	{
		LocalCharacter = Cast<AFPSBaseCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	if (IsValid(LocalCharacter))
	{
		RefreshInteractionWidgetsForCharacter(LocalCharacter);
	}
}

void ATruck::SetInteractionWidgetsHidden(bool bShouldHide)
{
	if (bShouldHide && VisibleInteractPromptType != ETruckInteractType::None)
	{
		VisibleInteractPromptType = ETruckInteractType::None;
		LastInteractPromptHiddenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastInteractPromptHiddenTime;
	}

	auto ApplyHidden = [bShouldHide](UWidgetComponent* WidgetComponent)
		{
			if (!WidgetComponent)
			{
				return;
			}

			WidgetComponent->SetHiddenInGame(bShouldHide, true);
			WidgetComponent->SetVisibility(!bShouldHide, true);
		};

	ApplyHidden(DriverSeatInteractWidget);
	ApplyHidden(CargoSeatInteractWidget);
	ApplyHidden(TurretInteractWidget);
}

bool ATruck::IsLocalInteractionCharacter(const AFPSBaseCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}

	if (Character->IsLocallyControlled())
	{
		return true;
	}

	if (const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
	{
		if (GameInstance->MyPlayer == Character)
		{
			return true;
		}
	}

	return DriverCharacter == Character && GetController() && GetController()->IsLocalController();
}

void ATruck::UpdateEngineSound()
{
	auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (MoveComp)
	{
		float CurrentRPM = MoveComp->GetEngineRotationSpeed();
		EngineAudioComponent->SetFloatParameter(TEXT("RPM"), CurrentRPM);
	}
}

void ATruck::UpdateBrakeSound()
{
	auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!MoveComp) return;

	const bool bIsMoving = FMath::Abs(GetVelocity().Size()) > 100.0f;
	(void)bIsMoving;
}
