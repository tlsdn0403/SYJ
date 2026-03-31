#include "Truck/Truck.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "HUD/InteractUIClass.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Weapon/MountedMachineGun.h"
#include "UObject/ConstructorHelpers.h"

ATruck::ATruck()
{
	DriverSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("DriverSeatInteractTrigger"));
	DriverSeatInteractTrigger->SetupAttachment(RootComponent);
	DriverSeatInteractTrigger->InitSphereRadius(200.0f);

	CargoSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("CargoSeatInteractTrigger"));
	CargoSeatInteractTrigger->SetupAttachment(RootComponent);
	CargoSeatInteractTrigger->InitSphereRadius(200.0f);

	TurretSeatInteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("TurretSeatInteractTrigger"));
	TurretSeatInteractTrigger->SetupAttachment(RootComponent);
	TurretSeatInteractTrigger->InitSphereRadius(200.0f);
	TurretSeatInteractTrigger->InteractType = ETruckInteractType::TurretSeat;
	TurretSeatInteractTrigger->SetRelativeLocation(FVector(-80.0f, 0.0f, 140.0f));

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
		AmmoSlots.Add(NewSlot);
	}

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("FuelSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		FuelSlots.Add(NewSlot);
	}

	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("MedKitSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
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

	if (IsPlayerControlled())
	{
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

FVector ATruck::GetCargoRideLocation() const
{
	return CargoRidePoint ? CargoRidePoint->GetComponentLocation() : GetActorLocation();
}

FRotator ATruck::GetCargoRideRotation() const
{
	return CargoRidePoint ? CargoRidePoint->GetComponentRotation() : GetActorRotation();
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
	PlayerInputComponent->BindAxis("Throttle", this, &ATruck::MoveForward);
	PlayerInputComponent->BindAxis("Steer", this, &ATruck::MoveRight);
	PlayerInputComponent->BindAxis("Brake", this, &ATruck::Brake);
}

void ATruck::MoveForward(float Value)
{
	UE_LOG(LogTemp, Warning, TEXT("Throttle Input: %f"), Value);
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
		if (Value > 0.5f && Speed > 300.0f && !bIsBrakingSoundPlaying)
		{
			if (BrakeSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, BrakeSound, GetActorLocation());
				bIsBrakingSoundPlaying = true;

				FTimerHandle Handle;
				GetWorld()->GetTimerManager().SetTimer(Handle, [this]() {
					bIsBrakingSoundPlaying = false;
				}, 1.0f, false);
			}
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
	if (!Character)
	{
		return;
	}

	if (Character->GetCurrentTruckInteractType() == ETruckInteractType::TurretSeat)
	{
		if (!MountedWeapon)
		{
			UE_LOG(LogTemp, Warning, TEXT("Mounted weapon is not configured on truck %s"), *GetName());
			return;
		}

		if (MountedWeaponUser && MountedWeaponUser != Character)
		{
			UE_LOG(LogTemp, Warning, TEXT("Mounted weapon already in use by %s"), *GetNameSafe(MountedWeaponUser));
			return;
		}

		MountedWeaponUser = Character;
		Character->EnterMountedWeapon(this, MountedWeapon);
		MountedWeapon->SetWeaponUser(Character);
		return;
	}

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

	if (Character->GetCurrentTruckInteractType() == ETruckInteractType::DriverSeat)
	{
		AController* PlayerController = Character->GetController();
		if (PlayerController)
		{
			PlayerController->Possess(this);
			UE_LOG(LogTemp, Log, TEXT("Driver Seat!"));
		}
	}
	else if (Character->GetCurrentTruckInteractType() == ETruckInteractType::CargoSeat)
	{
		UE_LOG(LogTemp, Log, TEXT("Cargo Seat!"));
		if (!Character->IsOnTruckCargo())
		{
			Character->EnterTruckCargo(this);
		}
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
		UE_LOG(LogTemp, Warning, TEXT("CurrentRPM: %f"), CurrentRPM);
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
