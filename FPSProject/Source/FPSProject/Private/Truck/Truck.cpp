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

ATruck::ATruck()
{
	DriverSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("DriverSeatInteractTrigger"));
	DriverSeatInteractTrigger->SetupAttachment(RootComponent);
	DriverSeatInteractTrigger->InitSphereRadius(200.0f);
	DriverSeatInteractTrigger->InteractType = ETruckInteractType::DriverSeat;

	CargoSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("CargoSeatInteractTrigger"));
	CargoSeatInteractTrigger->SetupAttachment(RootComponent);
	CargoSeatInteractTrigger->InitSphereRadius(200.0f);
	CargoSeatInteractTrigger->InteractType = ETruckInteractType::CargoSeat;

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

	auto SetupCargoCollision = [](UBoxComponent* Box)
		{
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
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

	static ConstructorHelpers::FClassFinder<AMountedMachineGun> MountedWeaponBP(TEXT("/Game/Truck/BP_MountedMachineGun"));
	UE_LOG(LogTemp, Warning, TEXT("MountedWeaponBP success: %d, class: %s"),
		MountedWeaponBP.Succeeded(),
		*GetNameSafe(MountedWeaponBP.Class));
	if (MountedWeaponBP.Succeeded())
	{
		MountedWeaponClass = MountedWeaponBP.Class;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> TurretWidgetBP(TEXT("/Game/Item/WBP_Interact"));
	if (TurretInteractWidget && TurretWidgetBP.Succeeded())
	{
		TurretInteractWidget->SetWidgetClass(TurretWidgetBP.Class);
	}
}

void ATruck::BeginPlay()
{
	Super::BeginPlay();

	if (ZombieStimuliSource)
	{
		ZombieStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
		ZombieStimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
		ZombieStimuliSource->RegisterWithPerceptionSystem();
	}

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		TruckMesh->SetGenerateOverlapEvents(true);
		TruckMesh->SetNotifyRigidBodyCollision(true);
		TruckMesh->OnComponentHit.AddDynamic(this, &ATruck::OnTruckMeshHit);
	}

	if (EngineSoundCue)
	{
		EngineAudioComponent->SetSound(EngineSoundCue);
	}

	if (TurretInteractWidget)
	{
		TurretInteractWidget->InitWidget();
	}

	if (TurretSeatInteractTrigger)
	{
		TurretSeatInteractTrigger->OnEnter.AddDynamic(this, &ATruck::OnTurretInteractEnter);
		TurretSeatInteractTrigger->OnExit.AddDynamic(this, &ATruck::OnTurretInteractExit);
	}

	for (UStaticMeshComponent* Slot : AmmoSlots) { if (Slot) Slot->SetVisibility(false); }
	for (UStaticMeshComponent* Slot : FuelSlots) { if (Slot) Slot->SetVisibility(false); }
	for (UStaticMeshComponent* Slot : MedKitSlots) { if (Slot) Slot->SetVisibility(false); }
	for (UStaticMeshComponent* Slot : AmmoSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	for (UStaticMeshComponent* Slot : FuelSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	for (UStaticMeshComponent* Slot : MedKitSlots) { if (Slot) { Slot->SetSimulatePhysics(false); Slot->SetEnableGravity(false); Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }

	if (MountedWeaponClass && TurretMountPoint)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UE_LOG(LogTemp, Warning, TEXT("Spawn Mounted Weapon"));

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
		}
	}
}

void ATruck::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckZombieImpactSweep();
	ReportZombieAwarenessNoise(DeltaTime);

	if (bIsLocallyDriven)
	{
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

			UE_LOG(LogTemp, Warning,
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
		if (EngineAudioComponent->IsPlaying())
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

void ATruck::SendTruckMovePacket()
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
	Info->set_yaw(GetActorRotation().Yaw);
	Info->set_state(GetVelocity().SizeSquared() > KINDA_SMALL_NUMBER ? Protocol::MOVE_STATE_RUN : Protocol::MOVE_STATE_IDLE);

	SEND_PACKET(MovePkt);
}

void ATruck::SetLocallyDriven(bool bLocallyDriven)
{
	bIsLocallyDriven = bLocallyDriven;

	UE_LOG(LogTemp, Warning,
		TEXT("[TruckDebug] SetLocallyDriven Truck=%s bLocallyDriven=%d Controller=%s IsPlayerControlled=%d"),
		*GetNameSafe(this),
		bLocallyDriven ? 1 : 0,
		*GetNameSafe(GetController()),
		IsPlayerControlled() ? 1 : 0);

	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetComponentTickEnabled(bLocallyDriven);

		if (!bLocallyDriven)
		{
			MoveComp->SetThrottleInput(0.0f);
			MoveComp->SetSteeringInput(0.0f);
			MoveComp->SetBrakeInput(1.0f);
		}
	}

	if (USkeletalMeshComponent* TruckMesh = GetMesh())
	{
		// Remote trucks are driven by replicated transforms, not local vehicle physics.
		if (!bLocallyDriven && TruckMesh->IsSimulatingPhysics())
		{
			TruckMesh->SetSimulatePhysics(false);
		}
	}
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
	return DriverExitPoint ? DriverExitPoint->GetComponentLocation() : GetActorLocation() - GetActorRightVector() * 200.f;
}

FVector ATruck::GetCargoExitLocation() const
{
	return CargoExitPoint ? CargoExitPoint->GetComponentLocation() : GetActorLocation() + GetActorRightVector() * 200.f;
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

void ATruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("Throttle", this, &ATruck::MoveForward);
	PlayerInputComponent->BindAxis("Steer", this, &ATruck::MoveRight);
	PlayerInputComponent->BindAxis("Brake", this, &ATruck::Brake);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ATruck::ExitDriverSeat);
}

void ATruck::MoveForward(float Value)
{
	/*UE_LOG(LogTemp, Warning, TEXT("Throttle Input: %f"), Value);*/
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(Value);
	}
}

void ATruck::MoveRight(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetSteeringInput(Value);
	}
}

void ATruck::Brake(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetBrakeInput(Value);

		const float Speed = GetVelocity().Size();
		const bool bBrakePressed = Value > 0.5f;
		if (bBrakePressed && !bBrakePressedLastFrame && Speed > BrakeSoundMinSpeed)
		{
			if (BrakeSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, BrakeSound, GetActorLocation());
			}
		}

		bBrakePressedLastFrame = bBrakePressed;
	}
}

void ATruck::CheckZombieImpactSweep()
{
	USkeletalMeshComponent* TruckMesh = GetMesh();
	if (!TruckMesh)
	{
		return;
	}

	const FVector VehicleVelocity = GetVelocity();
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

	const FBoxSphereBounds Bounds = TruckMesh->Bounds;
	const FVector SweepCenter = Bounds.Origin;
	const FVector SweepExtent(
		Bounds.BoxExtent.X * 1.12f,
		Bounds.BoxExtent.Y * 1.35f,
		Bounds.BoxExtent.Z * 1.18f);

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TruckZombieImpactSweep), false, this);

	if (!GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		SweepCenter,
		GetActorQuat(),
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
			if (TruckMesh->GetClosestPointOnCollision(Zombie->GetActorLocation(), ClosestPoint) >= 0.0f)
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
	UStaticMeshComponent* TargetSlot = nullptr;

	switch (ItemType)
	{
	case EItemType::Ammo:
		if (CurrentAmmoCount < AmmoSlots.Num())
		{
			TargetSlot = AmmoSlots[CurrentAmmoCount];
			CurrentAmmoCount++;
		}
		break;

	case EItemType::Fuel:
		if (CurrentFuelCount < FuelSlots.Num())
		{
			TargetSlot = FuelSlots[CurrentFuelCount];
			CurrentFuelCount++;
		}
		break;

	case EItemType::MedicalKit:
		if (CurrentMedKitCount < MedKitSlots.Num())
		{
			TargetSlot = MedKitSlots[CurrentMedKitCount];
			CurrentMedKitCount++;
		}
		break;
	}

	if (TargetSlot)
	{
		TargetSlot->SetVisibility(true);
		UE_LOG(LogTemp, Log, TEXT("Cargo loaded visually at slot. Type: %d"), (int32)ItemType);
	}
}

void ATruck::Interact_Implementation(AFPSBaseCharacter* Character)
{
	UE_LOG(LogTemp, Warning,
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

	if (TryEnterMountedWeapon(Character))
	{
		return;
	}
	// 아이템 파밍 라운드.
	if (bIsLoadingPhase)
	{
		if (Character->GetItemCount() > 0)
		{
			TArray<EItemType> ReceivedItems = Character->OffloadItems();

			for (EItemType Item : ReceivedItems)
			{
				TotalLoadedItems++;
				AddCargoVisual(Item);

				switch (Item)
				{
				case EItemType::Ammo:
					UE_LOG(LogTemp, Log, TEXT("Loaded Ammo box"));
					break;
				case EItemType::Fuel:
					UE_LOG(LogTemp, Log, TEXT("Loaded Fuel can"));
					break;
				case EItemType::MedicalKit:
					UE_LOG(LogTemp, Log, TEXT("Loaded Medical Kit"));
					break;
				default:
					break;
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
	// 운전석 탑승
	if (Character->GetCurrentTruckInteractType() == ETruckInteractType::DriverSeat)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TruckDebug] DriverSeatRequest Truck=%s Character=%s Local=%d"),
			*GetNameSafe(this),
			*GetNameSafe(Character),
			Character->IsLocallyControlled() ? 1 : 0);

		if (DriverCharacter && DriverCharacter != Character)
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
	if (!Character || !MountedWeapon)
	{
		return false;
	}

	const bool bRequestedTurretSeat =
		Character->GetCurrentTruckInteractType() == ETruckInteractType::TurretSeat;
	const bool bSwitchingFromCargo =
		Character->CurrentTruck == this &&
		Character->IsOnTruckCargo() &&
		FVector::Dist(Character->GetActorLocation(), GetTurretSeatLocation()) <= MountedWeaponUseDistance;

	if (!bRequestedTurretSeat && !bSwitchingFromCargo)
	{
		return false;
	}

	if (MountedWeaponUser && MountedWeaponUser != Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mounted weapon already in use by %s"), *GetNameSafe(MountedWeaponUser));
		return true;
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

	if (CharacterToRestore->IsLocallyControlled())
	{
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


	// 좀비의 위치를 트럭의 로컬 좌표로 변경
	const FVector LocalZombieLocation = GetActorTransform().InverseTransformPosition(Zombie->GetActorLocation());
	// 트럭 메쉬 크기를 가져옴
	const FVector MeshExtent = GetMesh() ? GetMesh()->Bounds.BoxExtent : FVector(150.0f, 100.0f, 100.0f);
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
		: GetActorTransform().TransformVectorNoScale(LocalOutwardDirection).GetSafeNormal();
	// 좀비를 날려버릴 방향 결정
	const FVector FinalImpactDirection = (SafeImpactDirection * 0.7f + WorldOutwardDirection * 0.9f).GetSafeNormal();
	const FVector ImpactFlingDirection = FinalImpactDirection.IsNearlyZero() ? SafeImpactDirection : FinalImpactDirection;

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
	// 좀비 즉사 조건
	const bool bCheatFlingImpact = bTruckBodyImpact && ImpactSpeed >= ZombiePinnedImpactFatalSpeed;

	if (Zombie->IsAlive() &&
		(ImpactSpeed >= ZombieImpactFatalSpeed ||
			bCheatFlingImpact))
	{
		Zombie->Die();
	}
	// 살아있다면 캐릭터를 넉백
	if (Zombie->IsAlive())
	{
		const FVector LaunchVelocity =
			ImpactFlingDirection * (ZombieImpactKnockback * KnockbackScale * 1.2f) +
			FVector::UpVector * (ZombieImpactUpwardKnockback * KnockbackScale);
		Zombie->LaunchCharacter(LaunchVelocity, true, true);
		return;
	}
	// 죽은 경우 레그돌 하여 넉백
	if (USkeletalMeshComponent* ZombieMesh = Zombie->GetMesh())
	{
		const FVector WorldImpulse =
			ImpactFlingDirection * (ZombieImpactImpulse * KnockbackScale * 1.2f) +
			FVector::UpVector * (ZombieImpactImpulse * 0.2f * KnockbackScale);
		ZombieMesh->AddImpulseAtLocation(WorldImpulse, ImpactPoint, FName(TEXT("pelvis")));
	}
}

void ATruck::EndMountedWeaponUse(AFPSBaseCharacter* Character)
{
	if (MountedWeaponUser == Character)
	{
		if (MountedWeapon)
		{
			MountedWeapon->SetWeaponUser(nullptr);
		}

		MountedWeaponUser = nullptr;
	}
}

void ATruck::OnTurretInteractEnter(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor) || !TurretInteractWidget)
	{
		return;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(TurretInteractWidget->GetUserWidgetObject()))
	{
		UI->SetInteractText(FText::FromString(TEXT("Use Machine Gun")));
		UI->PlayAni_PopUp(false);
	}
}

void ATruck::OnTurretInteractExit(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor) || !TurretInteractWidget)
	{
		return;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(TurretInteractWidget->GetUserWidgetObject()))
	{
		UI->RePlayAni_PopUp();
	}
}

void ATruck::UpdateEngineSound()
{
	auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (MoveComp)
	{
		float CurrentRPM = MoveComp->GetEngineRotationSpeed();
		/*UE_LOG(LogTemp, Warning, TEXT("CurrentRPM: %f"), CurrentRPM);*/
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
