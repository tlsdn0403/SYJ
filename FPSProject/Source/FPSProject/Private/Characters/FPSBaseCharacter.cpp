#include "Characters/FPSBaseCharacter.h"
#include "Protocol.pb.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/MountedMachineGun.h"
#include "Projectiles/FPSProjectile.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/HealthComponent.h"
#include "HUD/InventoryWidget.h"
#include "HUD/BasicUI.h"
#include "HUD/EffectUI.h"
#include "HUD/BaseUI.h"
#include "HUD/L2BaseUI.h"
#include "Characters/FPSPlayerController.h"
#include "Interface/InteractInterface.h"
#include "Truck/Truck.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimationAsset.h"
#include "ClientPacketHandler.h"
#include "FPSStage2WorldUtils.h"
#include "FPSProjectGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "NavigationInvokerComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "EngineUtils.h"

namespace
{
constexpr int32 BulletsPerAmmoBox = 40;

EItemType NormalizeStageItemType(EItemType ItemType)
{
	return ItemType == EItemType::TT ? EItemType::HealPack : ItemType;
}
}

AFPSBaseCharacter::AFPSBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	ThirdPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCameraComponent->bUsePawnControlRotation = false;
	ThirdPersonCameraComponent->FieldOfView = DefaultThirdPersonFOV;

	FPSCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
	FPSCameraComponent->SetupAttachment(RootComponent);
	FPSCameraComponent->bUsePawnControlRotation = true;
	FPSCameraComponent->SetActive(false);

	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ThirdPersonMesh"));
	check(FPSMesh != nullptr);

	FPSMesh->bCastDynamicShadow = false;
	FPSMesh->CastShadow = false;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	ZombieStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("ZombieStimuliSource"));
	NavigationInvokerComponent = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvoker"));
	NavigationInvokerComponent->SetGenerationRadii(6000.0f, 8000.0f);

	PlayerInfo = new Protocol::PosInfo();
	DestInfo = new Protocol::PosInfo();

	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	DefaultCameraBoomSocketOffset = CameraBoom->SocketOffset;

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> DrivingAnimationRef(
		TEXT("/Game/Characters/Animations/Driving/Driving__2__Anim.Driving__2__Anim"));
	if (DrivingAnimationRef.Succeeded())
	{
		DrivingAnimationAsset = DrivingAnimationRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> DeathAnimationRef(
		TEXT("/Game/Characters/Animations/Death_1.Death_1"));
	if (DeathAnimationRef.Succeeded())
	{
		DeathAnimationAsset = DeathAnimationRef.Object;
	}
}

AFPSBaseCharacter::~AFPSBaseCharacter()
{
	delete PlayerInfo;
	delete DestInfo;
}

void AFPSBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (ZombieStimuliSource)
	{
		ZombieStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
		ZombieStimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
		ZombieStimuliSource->RegisterWithPerceptionSystem();
	}

	ConfigureNavigationRuntimeCost();

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &AFPSBaseCharacter::HandleHealthChanged);
	}


	AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController());

	if (IsLocallyControlled())
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		{
			GameInstance->RequestEnterGameWhenReady();
			GameInstance->RefreshStage2StartupActorHold();
		}

		// 마우스 커서 숨기기
		if (PC)
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}

	if (PC)
	{
		if (FPSStage2WorldUtils::IsStage2World(GetWorld()))
		{
			Delete_L1Widget(PC);
			Add_L2_Widget(PC);
		}
		else
		{
			Add_L1_Widget(PC);
		}
	}
}

void AFPSBaseCharacter::ConfigureNavigationRuntimeCost()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCanEverAffectNavigation(false);
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCanEverAffectNavigation(false);
	}

	if (FPSMesh)
	{
		FPSMesh->SetCanEverAffectNavigation(false);
	}

	const bool bClientOnlyProxy = GetNetMode() == NM_Client;
	if (bClientOnlyProxy && NavigationInvokerComponent)
	{
		NavigationInvokerComponent->Deactivate();
		NavigationInvokerComponent->SetComponentTickEnabled(false);
	}
}

void AFPSBaseCharacter::Delete_L1Widget(AFPSPlayerController* PC) {
	if (!PC) return;

	if (PC->InventoryW)
	{
		PC->InventoryW->RemoveFromParent();
	}
}

void AFPSBaseCharacter::Add_L1_Widget(AFPSPlayerController* PC) {
	if (!PC) return;

	if (PC->InventoryW)
	{
		PC->InventoryW->AddToViewport();
	}
	if (PC->TimerW)
	{
		PC->TimerW->AddToViewport();
	}
	if (PC->BasicW)
	{
		PC->BasicW->AddToViewport();
	}
	RefreshStage2ItemUI();
	SetHealth(100, 100); //이건 처음값 임의 세팅
}

void AFPSBaseCharacter::Add_L2_Widget(AFPSPlayerController* PC) {
	if (!PC) return;

	if (PC->BasicW)
	{
		PC->BasicW->AddToViewport();
	}
	if (PC->EffectW)
	{
		PC->EffectW->AddToViewport();
	}
	if (PC->L2BaseW)
	{
		PC->L2BaseW->AddToViewport();
	}
}

void AFPSBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const bool bShouldSkipRemoteMovementSync = bIsDrivingTruck || bIsUsingMountedWeapon;

	if (bIsOnTruckCargo && CurrentTruck)
	{
		ConstrainToTruckCargoBounds();
	}

	// 줌 했을 떄 FOV 확대
	const bool bUseIronSightCamera =
		IsLocallyControlled() &&
		bIsIronSightAiming &&
		!bIsHoldAiming &&
		!bIsDrivingTruck &&
		!bIsUsingMountedWeapon &&
		CurrentWeapon != nullptr;

	UpdateIronSightFirstPersonView(bUseIronSightCamera && bUseFirstPersonWeaponIronSight);

	if (bUseIronSightCamera && FPSCameraComponent)
	{
		if (!FPSCameraComponent->IsActive() && ThirdPersonCameraComponent)
		{
			FPSCameraComponent->SetWorldLocationAndRotation(
				ThirdPersonCameraComponent->GetComponentLocation(),
				ThirdPersonCameraComponent->GetComponentRotation());
			FPSCameraComponent->FieldOfView = ThirdPersonCameraComponent->FieldOfView;
		}

		if (ThirdPersonCameraComponent)
		{
			ThirdPersonCameraComponent->SetActive(false);
		}
		FPSCameraComponent->SetActive(true);

		FVector TargetAimLocation = FVector::ZeroVector;
		FRotator TargetAimRotation = FRotator::ZeroRotator;
		if (CurrentWeapon->GetAimCameraViewPoint(TargetAimLocation, TargetAimRotation))
		{
			if (Controller)
			{
				TargetAimRotation = Controller->GetControlRotation();
			}

			FPSCameraComponent->SetWorldLocationAndRotation(
				FMath::VInterpTo(FPSCameraComponent->GetComponentLocation(), TargetAimLocation, DeltaTime, IronSightInterpSpeed),
				FMath::RInterpTo(FPSCameraComponent->GetComponentRotation(), TargetAimRotation, DeltaTime, IronSightInterpSpeed));
			FPSCameraComponent->FieldOfView = FMath::FInterpTo(
				FPSCameraComponent->FieldOfView,
				IronSightFOV,
				DeltaTime,
				IronSightInterpSpeed);
		}
	}
	else if (!bIsUsingMountedWeapon)
	{
		if (FPSCameraComponent)
		{
			FPSCameraComponent->SetActive(false);
		}
		if (ThirdPersonCameraComponent)
		{
			ThirdPersonCameraComponent->SetActive(true);
		}
	}
	if (ThirdPersonCameraComponent && CameraBoom)
	{
		// 조준중이면 Aiming 카메라 , 아니면 기존
		const float TargetFOV = bIsHoldAiming ? AimingThirdPersonFOV : DefaultThirdPersonFOV;
		// 이것도 조준중이면 Aiming 카메라 붐
		const float TargetBoomLength = bIsHoldAiming ? AimingBoomLength : DefaultBoomLength;
		// 이것도 조준중이면 Aiming 카메라 붐 소켓 오프셋
		const FVector TargetSocketOffset = bIsHoldAiming ? AimingCameraBoomSocketOffset : DefaultCameraBoomSocketOffset;

		// 보간을 이용해서, 카메라가 부드럽게 이동하도록
		ThirdPersonCameraComponent->FieldOfView = FMath::FInterpTo(
			ThirdPersonCameraComponent->FieldOfView,
			TargetFOV,
			DeltaTime,
			AimInterpSpeed);

		CameraBoom->TargetArmLength = FMath::FInterpTo(
			CameraBoom->TargetArmLength,
			TargetBoomLength,
			DeltaTime,
			AimInterpSpeed);

		CameraBoom->SocketOffset = FMath::VInterpTo(
			CameraBoom->SocketOffset,
			TargetSocketOffset,
			DeltaTime,
			AimInterpSpeed);
	}

	if (IsLocallyControlled() && Controller && !RecoilRecoveryRemaining.IsNearlyZero())
	{
		const float RecoveryAlpha = FMath::Clamp(RecoilRecoverySpeed * DeltaTime, 0.0f, 1.0f);
		const FRotator RecoveryStep = RecoilRecoveryRemaining * RecoveryAlpha;

		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Pitch -= RecoveryStep.Pitch;
		ControlRotation.Yaw -= RecoveryStep.Yaw;
		Controller->SetControlRotation(ControlRotation);

		RecoilRecoveryRemaining -= RecoveryStep;
	}
	if (bIsUsingMountedWeapon && CurrentMountedWeapon && IsLocallyControlled())
	{
		CurrentMountedWeapon->UpdateAim(GetControlRotation());

		if (FPSCameraComponent)
		{
			FPSCameraComponent->SetWorldLocationAndRotation(
				CurrentMountedWeapon->GetCameraLocation(),
				CurrentMountedWeapon->GetCameraRotation()
			);
			FPSCameraComponent->FieldOfView = CurrentMountedWeapon->GetCameraFOV();
		}
	}

	if (IsLocallyControlled())
	{
		MovePacketSendTimer += DeltaTime;
		if (MovePacketSendTimer >= MOVE_PACKET_SEND_DELAY)
		{
			MovePacketSendTimer = 0.f;
			SendMovePacket();
		}
	}
	//(신우) 트럭을 타고 있을 때 위치보정 안하도록  (이거 안하니까 이상한곳에 앉아있음)
	else if (bShouldSkipRemoteMovementSync)
	{
		return;
	}
	// [신우] 탑승자 보정을 바꿈 idle일 때 빠르게 보정
	else if (bIsOnTruckCargo && CurrentTruck)
	{
		const FRotator TargetRot(0.f, DestInfo->yaw(), 0.f);
		const Protocol::MoveState CargoState = DestInfo->state();
		const bool bIsCargoMoving =
			CargoState == Protocol::MOVE_STATE_RUN ||
			CargoState == Protocol::MOVE_STATE_JUMP;

		SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 12.f));

		if (UBoxComponent* CargoBounds = CurrentTruck->GetCargoMoveBoundsComponent();
			CargoBounds && bHasReplicatedTruckCargoLocalLocation)
		{
			const FTransform BoundsTransform = CargoBounds->GetComponentTransform();
			const FVector CurrentLocalLocation =
				BoundsTransform.InverseTransformPosition(GetActorLocation());
			const float LocalOffsetErrorSq =
				FVector::DistSquaredXY(CurrentLocalLocation, ReplicatedTruckCargoLocalLocation);
			const FVector TargetLocalLocation = bIsCargoMoving
				? FMath::VInterpTo(CurrentLocalLocation, ReplicatedTruckCargoLocalLocation, DeltaTime, 12.f)
				: ReplicatedTruckCargoLocalLocation;
			const bool bShouldSnapToCargoOffset =
				LocalOffsetErrorSq > FMath::Square(bIsCargoMoving ? 90.0f : 20.0f);

			SetActorLocation(
				BoundsTransform.TransformPosition(bShouldSnapToCargoOffset
					? ReplicatedTruckCargoLocalLocation
					: TargetLocalLocation),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		else
		{
			const FVector CurrentLocation = GetActorLocation();
			const FVector TargetLocation(DestInfo->x(), DestInfo->y(), DestInfo->z());

			SetActorLocation(
				FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, 12.f),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		return;
	}
	else // 남의 캐릭터일 때
	{
		const Protocol::MoveState State = PlayerInfo->state();
		FVector CurrentLocation = GetActorLocation();
		FVector TargetLocation = FVector(DestInfo->x(), DestInfo->y(), DestInfo->z());

		FRotator TargetRot(0.f, DestInfo->yaw(), 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 15.f));

		if (State == Protocol::MOVE_STATE_JUMP && RemoteLastState != Protocol::MOVE_STATE_JUMP)
		{
			if (!GetCharacterMovement()->IsFalling()) Jump();
		}
		RemoteLastState = State;

		float DistToDest = FVector::Dist(CurrentLocation, TargetLocation);

		if (State == Protocol::MOVE_STATE_RUN || State == Protocol::MOVE_STATE_JUMP)
		{
			FVector Direction;
			if (DistToDest > 15.0f)
			{
				Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
			}
			else
			{
				Direction = GetActorForwardVector();
			}

			AddMovementInput(Direction, 1.0f);

			if (DistToDest > 200.0f)
			{
				SetActorLocation(TargetLocation);
			}
		}
		else
		{
			if (DistToDest > 2.0f)
			{
				SetActorLocation(FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, 10.0f));
			}
		}
	}
}

void AFPSBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AFPSBaseCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AFPSBaseCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &AFPSBaseCharacter::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &AFPSBaseCharacter::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AFPSBaseCharacter::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AFPSBaseCharacter::StopJump);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AFPSBaseCharacter::Fire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AFPSBaseCharacter::StopFire);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AFPSBaseCharacter::StartAim);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AFPSBaseCharacter::StopAim);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AFPSBaseCharacter::Interact);
	PlayerInputComponent->BindAction("LeaveGame", IE_Pressed, this, &AFPSBaseCharacter::LeaveGame);
	PlayerInputComponent->BindAction("TravelToStage2Map", IE_Pressed, this, &AFPSBaseCharacter::TravelToStage2Map);
	PlayerInputComponent->BindAction("UseHealPack", IE_Pressed, this, &AFPSBaseCharacter::HandleUseHealPackInput);
}

void AFPSBaseCharacter::HandleUseHealPackInput()
{
	UseHealPack();
}

void AFPSBaseCharacter::HandleHealthChanged(float NewHealth, float Damage)
{
	if (NewHealth <= 0.0f)
	{
		Die();
	}
}

void AFPSBaseCharacter::Die(bool bBroadcastDeath)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	StopFire();
	UpdateIronSightFirstPersonView(false);

	if (bBroadcastDeath && IsLocallyControlled())
	{
		SendDeathPacket();
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->SetWeaponHidden(true);
	}

	if (CurrentTruck)
	{
		if (bIsDrivingTruck)
		{
			ExitTruckDriverSeat();
		}
		else if (bIsUsingMountedWeapon)
		{
			ExitMountedWeapon(false);
		}
		else if (bIsOnTruckCargo)
		{
			ExitTruckCargo();
		}
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (DeathAnimationAsset && GetMesh())
	{
		GetMesh()->PlayAnimation(DeathAnimationAsset, false);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathCleanupTimerHandle);
		World->GetTimerManager().SetTimer(
			DeathCleanupTimerHandle,
			this,
			&AFPSBaseCharacter::RemoveDeadBody,
			DeathCleanupDelay,
			false);
	}

	if (IsLocallyControlled())
	{
		if (AFPSPlayerController* FPSPlayerController = Cast<AFPSPlayerController>(GetController()))
		{
			if (FPSPlayerController->EffectW)
			{
				FPSPlayerController->EffectW->PlayAni_Effect(false);
			}
		}

		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
			{
				if (AFPSBaseCharacter* SpectateTarget = GameInstance->GetSpectateTargetBySlot(0))
				{
					PlayerController->SetViewTargetWithBlend(SpectateTarget, 0.25f);
				}
			}
		}
	}
}
// 2스테이지 맵으로 이동
void AFPSBaseCharacter::TravelToStage2Map()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("map_level2_test")));
}

void AFPSBaseCharacter::SendEnterGamePacket()
{
	if (bEnterGamePacketSent)
	{
		return;
	}

	Protocol::C_ENTER_GAME EnterGamePkt;
	EnterGamePkt.set_playerindex(0); // 임시로 0번

	if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
	{
		SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(EnterGamePkt);
		GameInstance->SendPacket(SendBuffer);
		bEnterGamePacketSent = true;
		UE_LOG(LogTemp, Warning, TEXT("[Network] C_ENTER_GAME sent."));
	}
}

void AFPSBaseCharacter::EnterTruckDriverSeat(ATruck* Truck)
{
	if (!Truck || bIsDrivingTruck || bIsOnTruckCargo || bIsUsingMountedWeapon)
	{
		return;
	}

	ClearTruckInteractionState();
	CurrentTruck = Truck;
	ShowTruckFuelOnHUD(Truck);
	bIsDrivingTruck = true;
	bIsAiming = false;
	bIsHoldAiming = false;
	bIsIronSightAiming = false;
	bAimInputHeld = false;
	StopFire();
	SetHeldWeaponVehicleVisibility(true);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	AttachToComponent(Truck->DriverSeatPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector::ZeroVector);
	SetActorRelativeRotation(FRotator::ZeroRotator);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlayDrivingAnimation();
	Truck->RefreshInteractionWidgetsForCharacter(this);
}

void AFPSBaseCharacter::ExitTruckDriverSeat()
{
	if (!bIsDrivingTruck || !CurrentTruck)
	{
		return;
	}

	ATruck* Truck = CurrentTruck;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(
		Truck->GetDriverExitLocation(),
		Truck->GetUprightExitRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	bIsDrivingTruck = false;
	CurrentTruck = nullptr;
	StopShowingTruckFuelOnHUD();
	SetHeldWeaponVehicleVisibility(false);
	RefreshTruckInteractionState(Truck);
	Truck->RefreshInteractionWidgetsForCharacter(this);

	ApplyDefaultAnimationClass();
}

void AFPSBaseCharacter::EnterTruckCargo(ATruck* Truck)
{
	if (!Truck || bIsDrivingTruck)
	{
		return;
	}

	if (bIsUsingMountedWeapon && CurrentTruck == Truck)
	{
		ExitMountedWeapon();
		return;
	}

	if (bIsOnTruckCargo)
	{
		return;
	}

	StopFire();
	ClearTruckInteractionState();
	bHasSavedTruckCargoLocalLocation = false;
	bIsOnTruckCargo = true;
	bIsDrivingTruck = false;
	bIsAiming = false;
	bIsHoldAiming = false;
	bIsIronSightAiming = false;
	bAimInputHeld = false;
	CurrentTruck = Truck;
	ShowTruckFuelOnHUD(Truck);

	SetActorLocationAndRotation(
		Truck->GetCargoRideLocation(),
		Truck->GetCargoRideRotation()
	);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	BeginTruckCargoWalk(Truck);
	RefreshTruckInteractionState(Truck);
	Truck->RefreshInteractionWidgetsForCharacter(this);
}

void AFPSBaseCharacter::ExitTruckCargo()
{
	if (!bIsOnTruckCargo || !CurrentTruck)
	{
		return;
	}

	ATruck* Truck = CurrentTruck;
	bHasSavedTruckCargoLocalLocation = false;

	EndTruckCargoWalk();
	SetActorLocationAndRotation(
		Truck->GetCargoExitLocation(),
		Truck->GetUprightExitRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	bIsOnTruckCargo = false;
	CurrentTruck = nullptr;
	StopShowingTruckFuelOnHUD();
	bHasReplicatedTruckCargoLocalLocation = false;
	bHasLastTruckCargoLocalLocationForMoveState = false;
	RefreshTruckInteractionState(Truck);
	Truck->RefreshInteractionWidgetsForCharacter(this);

	if (IsLocallyControlled())
	{
		SendMovePacket();
	}
}

void AFPSBaseCharacter::EnterMountedWeapon(ATruck* Truck, AMountedMachineGun* MountedWeapon)
{
	if (!Truck || !MountedWeapon || bIsUsingMountedWeapon || bIsDrivingTruck ||
		!bIsOnTruckCargo || CurrentTruck != Truck)
	{
		return;
	}

	StopFire();
	RecoilRecoveryRemaining = FRotator::ZeroRotator;
	ClearTruckInteractionState();
	const bool bWasOnTruckCargo = bIsOnTruckCargo;
	bIsUsingMountedWeapon = true;
	bIsOnTruckCargo = false;
	bIsDrivingTruck = false;
	bIsAiming = false;
	bIsHoldAiming = false;
	bIsIronSightAiming = false;
	bAimInputHeld = false;
	CurrentTruck = Truck;
	ShowTruckFuelOnHUD(Truck);
	CurrentMountedWeapon = MountedWeapon;
	CurrentMountedWeapon->SetWeaponUser(this);
	SetHeldWeaponVehicleVisibility(true);

	if (bWasOnTruckCargo)
	{
		if (UBoxComponent* CargoBounds = Truck->GetCargoMoveBoundsComponent())
		{
			SavedTruckCargoLocalLocation =
				CargoBounds->GetComponentTransform().InverseTransformPosition(GetActorLocation());
			bHasSavedTruckCargoLocalLocation = true;
		}
		else
		{
			bHasSavedTruckCargoLocalLocation = false;
		}
	}

	bHasReplicatedTruckCargoLocalLocation = false;
	bHasLastTruckCargoLocalLocationForMoveState = false;

	EndTruckCargoWalk();

	SetActorLocationAndRotation(
		Truck->GetTurretSeatLocation(),
		Truck->GetTurretSeatRotation()
	);

	AttachToComponent(Truck->TurretSeatPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayDrivingAnimation();

	if (ThirdPersonCameraComponent)
	{
		ThirdPersonCameraComponent->SetActive(false);
	}
	if (FPSCameraComponent)
	{
		FPSCameraComponent->SetWorldLocationAndRotation(
			MountedWeapon->GetCameraLocation(),
			MountedWeapon->GetCameraRotation()
		);
		FPSCameraComponent->FieldOfView = MountedWeapon->GetCameraFOV();
		FPSCameraComponent->SetActive(true);
	}

	if (Controller)
	{
		Controller->SetControlRotation(MountedWeapon->GetCameraRotation());
	}

	Truck->RefreshInteractionWidgetsForCharacter(this);
}

void AFPSBaseCharacter::ExitMountedWeapon(bool bReturnToCargo)
{
	if (!bIsUsingMountedWeapon || !CurrentTruck)
	{
		return;
	}

	ATruck* Truck = CurrentTruck;
	FRotator ExitViewRotation = Controller
		? Controller->GetControlRotation()
		: GetActorRotation();
	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		FVector ViewLocation = FVector::ZeroVector;
		PlayerController->GetPlayerViewPoint(ViewLocation, ExitViewRotation);
	}
	ExitViewRotation.Roll = 0.0f;
	ExitViewRotation.Normalize();

	FVector RestoreCargoWorldLocation = Truck->GetCargoRideLocation();
	if (bHasSavedTruckCargoLocalLocation)
	{
		if (UBoxComponent* CargoBounds = Truck->GetCargoMoveBoundsComponent())
		{
			RestoreCargoWorldLocation =
				CargoBounds->GetComponentTransform().TransformPosition(SavedTruckCargoLocalLocation);
		}
	}

	if (CurrentMountedWeapon)
	{
		CurrentMountedWeapon->SetWeaponUser(nullptr);
	}

	StopFire();
	RecoilRecoveryRemaining = FRotator::ZeroRotator;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StopFire();

	if (FPSCameraComponent)
	{
		FPSCameraComponent->SetActive(false);
	}
	if (ThirdPersonCameraComponent)
	{
		ThirdPersonCameraComponent->SetActive(true);
	}

	Truck->EndMountedWeaponUse(this);
	CurrentMountedWeapon = nullptr;
	bIsUsingMountedWeapon = false;
	SetHeldWeaponVehicleVisibility(false);

	if (bReturnToCargo)
	{
		CurrentTruck = Truck;
		bIsOnTruckCargo = true;
		bHasReplicatedTruckCargoLocalLocation = false;
		bHasLastTruckCargoLocalLocationForMoveState = false;

		BeginTruckCargoWalk(Truck);
		SetActorLocationAndRotation(
			RestoreCargoWorldLocation,
			Truck->GetCargoRideRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		ConstrainToTruckCargoBounds();

		if (UBoxComponent* CargoBounds = Truck->GetCargoMoveBoundsComponent())
		{
			const FVector CargoLocalLocation =
				CargoBounds->GetComponentTransform().InverseTransformPosition(GetActorLocation());
			ReplicatedTruckCargoLocalLocation = CargoLocalLocation;
			bHasReplicatedTruckCargoLocalLocation = true;
			LastTruckCargoLocalLocationForMoveState = CargoLocalLocation;
			bHasLastTruckCargoLocalLocationForMoveState = true;
		}
	}
	else
	{
		bIsOnTruckCargo = false;
		CurrentTruck = nullptr;
		StopShowingTruckFuelOnHUD();
		bHasReplicatedTruckCargoLocalLocation = false;
		bHasLastTruckCargoLocalLocationForMoveState = false;

		SetActorLocationAndRotation(
			Truck->GetCargoExitLocation(),
			Truck->GetUprightExitRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	// Keep the regular controller and character aligned with the actual mounted
	// camera before movement resumes. Otherwise W can use a stale gun rotation.
	if (IsLocallyControlled() && Controller)
	{
		Controller->SetControlRotation(ExitViewRotation);
		SetActorRotation(
			FRotator(0.0f, ExitViewRotation.Yaw, 0.0f),
			ETeleportType::TeleportPhysics);
	}

	RefreshTruckInteractionState(Truck);
	Truck->RefreshInteractionWidgetsForCharacter(this);
	bHasSavedTruckCargoLocalLocation = false;

	ApplyDefaultAnimationClass();

	if (IsLocallyControlled())
	{
		SendMovePacket();
	}
}

void AFPSBaseCharacter::SyncMovementToServer()
{
	if (IsLocallyControlled())
	{
		SendMovePacket();
	}
}

bool AFPSBaseCharacter::CanInteractWithMountedWeapon() const
{
	return bIsOnTruckCargo &&
		CurrentTruck != nullptr &&
		CurrentInteractableActor == CurrentTruck &&
		CurrentTruckInteractType == ETruckInteractType::TurretSeat &&
		!IsValid(CurrentTruck->GetMountedWeaponUser());
}

void AFPSBaseCharacter::MoveForward(float Value)
{
	if (bIsDead || bIsUsingMountedWeapon)
	{
		return;
	}

	if (Controller != nullptr && Value != 0.0f)
	{
		const FVector Direction = FRotationMatrix(GetMovementViewRotation()).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AFPSBaseCharacter::MoveRight(float Value)
{
	if (bIsDead || bIsUsingMountedWeapon)
	{
		return;
	}

	if (Controller != nullptr && Value != 0.0f)
	{
		const FVector Direction = FRotationMatrix(GetMovementViewRotation()).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

FRotator AFPSBaseCharacter::GetMovementViewRotation() const
{
	FRotator ViewRotation = Controller
		? Controller->GetControlRotation()
		: GetActorRotation();

	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		FVector ViewLocation = FVector::ZeroVector;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	return FRotator(0.0f, ViewRotation.Yaw, 0.0f);
}

void AFPSBaseCharacter::StartJump()
{
	if (bIsUsingMountedWeapon)
	{
		return;
	}

	Jump();
	bPressedJump = true;
	SendMovePacket();
}

void AFPSBaseCharacter::StopJump()
{
	StopJumping();
	bPressedJump = false;
}

void AFPSBaseCharacter::StartAim()
{
	if (bIsDrivingTruck || bIsUsingMountedWeapon || !CurrentWeapon)
	{
		return;
	}

	bAimInputHeld = true;
	AimPressedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bIsHoldAiming = true;
	bIsAiming = true;
}

void AFPSBaseCharacter::StopAim()
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : AimPressedTime;
	const float HeldDuration = bAimInputHeld ? CurrentTime - AimPressedTime : AimTapToggleThreshold + 1.0f;

	bAimInputHeld = false;
	bIsHoldAiming = false;

	if (CurrentWeapon && HeldDuration <= AimTapToggleThreshold)
	{
		bIsIronSightAiming = !bIsIronSightAiming;
	}

	bIsAiming = bIsHoldAiming || bIsIronSightAiming;
}

void AFPSBaseCharacter::LeaveGame()
{
	if (IsLocallyControlled())
	{
		//// 내 게임 인스턴스를 찾아와서 접속 끊기 함수 실행!
		//if (UFPSProjectGameInstance* GI = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		//{
		//	GI->DisconnectFromGameServer();
		//	GI->QuitGame();
		//}
		AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController());
		PC->BasicW->Play_ESC();
	}
}

void AFPSBaseCharacter::SetCurrentWeapon(AWeaponBase* NewWeapon)
{
	CurrentWeapon = NewWeapon;

	if (bIsDrivingTruck || bIsUsingMountedWeapon)
	{
		SetHeldWeaponVehicleVisibility(true);
		return;
	}

	ApplyDefaultAnimationClass();
}

void AFPSBaseCharacter::ApplyDefaultAnimationClass()
{
	if (!GetMesh())
	{
		return;
	}

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	if (CurrentWeapon != nullptr)
	{
		if (ArmedAnimClass)
		{
			GetMesh()->SetAnimInstanceClass(ArmedAnimClass);
		}
	}
	else if (UnarmedAnimClass)
	{
		GetMesh()->SetAnimInstanceClass(UnarmedAnimClass);
	}
}

void AFPSBaseCharacter::PlayDrivingAnimation()
{
	if (!GetMesh() || !DrivingAnimationAsset)
	{
		return;
	}

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->PlayAnimation(DrivingAnimationAsset, true);
}

void AFPSBaseCharacter::ClearCurrentWeapon()
{
	UpdateIronSightFirstPersonView(false);
	SetCurrentWeapon(nullptr);
	bIsAiming = false;
	bIsHoldAiming = false;
	bIsIronSightAiming = false;
	bAimInputHeld = false;
}

void AFPSBaseCharacter::Fire()
{
	if (bIsUsingMountedWeapon && CurrentMountedWeapon)
	{
		CurrentMountedWeapon->SetWeaponUser(this);
		CurrentMountedWeapon->Fire();

		if (!GetWorldTimerManager().IsTimerActive(MountedWeaponAutoFireTimerHandle))
		{
			GetWorldTimerManager().SetTimer(
				MountedWeaponAutoFireTimerHandle,
				this,
				&AFPSBaseCharacter::HandleMountedWeaponAutoFire,
				CurrentMountedWeapon->GetFireInterval(),
				true,
				CurrentMountedWeapon->GetFireInterval());
		}

		if (IsLocallyControlled())
		{
			Protocol::C_FIRE FirePkt;
			SEND_PACKET(FirePkt);
			UE_LOG(LogTemp, Verbose, TEXT("[Network] 1. C_FIRE sent to server."));
		}

		return;
	}

	if (GetCurrentWeapon())
	{
		if (IsLocallyControlled() && !ConsumeStage2AmmoBullet())
		{
			return;
		}

		GetCurrentWeapon()->Fire();

		if (IsLocallyControlled())
		{
			Protocol::C_FIRE FirePkt;
			SEND_PACKET(FirePkt);
		}

		return;
	}
}

void AFPSBaseCharacter::StopFire()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(MountedWeaponAutoFireTimerHandle);
	}
}

void AFPSBaseCharacter::GetWeaponAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	// 실제 플레이어가 조종줄일 때
	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		// 뷰포트 크기 가져옴
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		// 중앙 좌표 계산
		if (ViewportSizeX > 0 && ViewportSizeY > 0)
		{
			FVector WorldDirection;
			// 화면 중앙을 월드 방향으로 변환
			// 2D 화면 좌표를 3D 월드 좌표와 방향으로 바꾸어 주는 함수다
			if (PlayerController->DeprojectScreenPositionToWorld(
				ViewportSizeX * 0.5f,
				ViewportSizeY * 0.5f,
				OutLocation,
				WorldDirection))
			{
				OutRotation = WorldDirection.Rotation();
				return;
			}
		}
		// OutLocation -> 월드 공간에서 화면 중앙에 해당하는 위치
		// OutRotation -> 월드 공간에서 화면 중앙을 향하는 방향의 회전값
	}

	if (FPSCameraComponent && FPSCameraComponent->IsActive())
	{
		OutLocation = FPSCameraComponent->GetComponentLocation();
		OutRotation = FPSCameraComponent->GetComponentRotation();
		return;
	}

	if (ThirdPersonCameraComponent && ThirdPersonCameraComponent->IsActive())
	{
		OutLocation = ThirdPersonCameraComponent->GetComponentLocation();
		OutRotation = ThirdPersonCameraComponent->GetComponentRotation();
		return;
	}

	GetActorEyesViewPoint(OutLocation, OutRotation);
}

// 총을 쏠 때 총기 반동을 주기.
void AFPSBaseCharacter::ApplyWeaponRecoil(float PitchKick, float YawKick)
{
	if (!IsLocallyControlled() || !Controller || bIsDrivingTruck)
	{
		return;
	}

	FRotator ControlRotation = Controller->GetControlRotation();
	ControlRotation.Pitch += PitchKick;
	ControlRotation.Yaw += YawKick;
	Controller->SetControlRotation(ControlRotation);

	RecoilRecoveryRemaining.Pitch += PitchKick;
	RecoilRecoveryRemaining.Yaw += YawKick;
}

void AFPSBaseCharacter::HandleMountedWeaponAutoFire()
{
	if (!bIsUsingMountedWeapon || !CurrentMountedWeapon)
	{
		StopFire();
		return;
	}

	CurrentMountedWeapon->SetWeaponUser(this);
	CurrentMountedWeapon->Fire();

	if (IsLocallyControlled())
	{
		Protocol::C_FIRE FirePkt;
		SEND_PACKET(FirePkt);
	}
}

void AFPSBaseCharacter::BeginTruckCargoWalk(ATruck* Truck)
{
	if (!Truck)
	{
		return;
	}

	EndTruckCargoWalk();

	// 트렁크 위치로 캐릭터 이동
	SetActorLocationAndRotation(
		Truck->GetCargoRideLocation(),
		Truck->GetCargoRideRotation()
	);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 캐릭터 매시가 트럭 매시와 충돌하지 않도록 설정
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (GetCharacterMovement())
	{
		// 탑승 직전에 캐릭터가 가지고 있던 속도를 제거
		GetCharacterMovement()->StopMovementImmediately();
		// 적재함 위에서 걷기 가능하도록 이동 모드 변경
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		GetCharacterMovement()->bFastAttachedMove = true;

	}
	// 캐릭터 캡슐과 트럭 메시가 서로 이동 충돌을 무시하도록 설정
	SetTruckMeshMovementIgnored(Truck, true);

	if (UPrimitiveComponent* CargoMovementBase = Truck->GetCargoMovementBase())
	{
		// 캐릭터가 어떤 바닥 위에서 움직이는지 설정을 해줌
		SetBase(CargoMovementBase);
	}

	ConstrainToTruckCargoBounds();


	if (UBoxComponent* CargoBounds = Truck->GetCargoMoveBoundsComponent())
	{
		const FVector CargoLocalLocation =
			CargoBounds->GetComponentTransform().InverseTransformPosition(GetActorLocation());
		ReplicatedTruckCargoLocalLocation = CargoLocalLocation;
		bHasReplicatedTruckCargoLocalLocation = true;
		LastTruckCargoLocalLocationForMoveState = CargoLocalLocation;
		bHasLastTruckCargoLocalLocationForMoveState = true;
	}
	else
	{
		bHasReplicatedTruckCargoLocalLocation = false;
		bHasLastTruckCargoLocalLocationForMoveState = false;
	}
}

void AFPSBaseCharacter::EndTruckCargoWalk()
{
	SetBase(nullptr);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	bHasReplicatedTruckCargoLocalLocation = false;
	bHasLastTruckCargoLocalLocationForMoveState = false;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bFastAttachedMove = false;
	}

	if (CurrentTruck)
	{
		SetTruckMeshMovementIgnored(CurrentTruck, false);
	}
}

void AFPSBaseCharacter::ConstrainToTruckCargoBounds()
{
	if (!bIsOnTruckCargo || !CurrentTruck)
	{
		return;
	}

	UBoxComponent* CargoBounds = CurrentTruck->GetCargoMoveBoundsComponent();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!CargoBounds || !Capsule)
	{
		return;
	}

	const FTransform BoundsTransform = CargoBounds->GetComponentTransform();
	FVector LocalLocation = BoundsTransform.InverseTransformPosition(GetActorLocation());
	const FVector Extent = CargoBounds->GetScaledBoxExtent();
	const float Radius = Capsule->GetScaledCapsuleRadius() + TruckCargoBoundsPadding;

	const float MaxX = FMath::Max(Extent.X - Radius, 0.0f);
	const float MaxY = FMath::Max(Extent.Y - Radius, 0.0f);

	const float ClampedX = FMath::Clamp(LocalLocation.X, -MaxX, MaxX);
	const float ClampedY = FMath::Clamp(LocalLocation.Y, -MaxY, MaxY);

	if (FMath::IsNearlyEqual(LocalLocation.X, ClampedX, 0.5f)
		&& FMath::IsNearlyEqual(LocalLocation.Y, ClampedY, 0.5f))
	{
		return;
	}

	LocalLocation.X = ClampedX;
	LocalLocation.Y = ClampedY;

	const FVector ClampedWorldLocation = BoundsTransform.TransformPosition(LocalLocation);
	SetActorLocation(ClampedWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AFPSBaseCharacter::SetTruckMeshMovementIgnored(ATruck* Truck, bool bShouldIgnore)
{
	if (!Truck)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		if (USkeletalMeshComponent* TruckMesh = Truck->GetMesh())
		{
			Capsule->IgnoreComponentWhenMoving(TruckMesh, bShouldIgnore);
			TruckMesh->IgnoreComponentWhenMoving(Capsule, bShouldIgnore);
		}
		auto IgnoreTruckComponent = [Capsule, bShouldIgnore](UPrimitiveComponent* Component)
		{
			if (!Component)
			{
				return;
			}

			Capsule->IgnoreComponentWhenMoving(Component, bShouldIgnore);
			Component->IgnoreComponentWhenMoving(Capsule, bShouldIgnore);
		};

		IgnoreTruckComponent(Truck->GetMesh());
		IgnoreTruckComponent(Truck->CargoLeftWallCollision);
		IgnoreTruckComponent(Truck->CargoRightWallCollision);
		IgnoreTruckComponent(Truck->CargoFrontWallCollision);
		IgnoreTruckComponent(Truck->CargoBackWallCollision);
	}
}

void AFPSBaseCharacter::SetHeldWeaponVehicleVisibility(bool bShouldHide)
{
	if (bShouldHide)
	{
		UpdateIronSightFirstPersonView(false);
	}

	if (!IsValid(CurrentWeapon))
	{
		return;
	}

	// An equipped weapon is attached to the character and must never participate
	// in world collision. Re-enabling it on vehicle exit pushes or blocks the
	// owning character differently depending on the weapon's socket orientation.
	CurrentWeapon->SetWeaponCollisionEnabled(false);
	CurrentWeapon->SetWeaponHidden(bShouldHide);
}

void AFPSBaseCharacter::UpdateIronSightFirstPersonView(bool bEnable)
{
	const bool bShouldEnable =
		bEnable &&
		IsLocallyControlled() &&
		!bIsDrivingTruck &&
		!bIsUsingMountedWeapon &&
		IsValid(CurrentWeapon) &&
		IsValid(FPSCameraComponent);

	if (bFirstPersonIronSightViewActive == bShouldEnable)
	{
		return;
	}

	bFirstPersonIronSightViewActive = bShouldEnable;

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetOwnerNoSee(bShouldEnable);
	}

	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->SetFirstPersonViewEnabled(bShouldEnable, bShouldEnable ? FPSCameraComponent : nullptr);
	}
}
void AFPSBaseCharacter::ClearTruckInteractionState()
{
	CurrentInteractableActor = nullptr;
	CurrentTruckInteractType = ETruckInteractType::None;
}

void AFPSBaseCharacter::RefreshTruckInteractionState(ATruck* Truck)
{
	ClearTruckInteractionState();

	if (!Truck)
	{
		return;
	}

	if (bIsOnTruckCargo && CurrentTruck == Truck &&
		!IsValid(Truck->GetMountedWeaponUser()) &&
		Truck->TurretSeatInteractTrigger &&
		Truck->TurretSeatInteractTrigger->IsOverlappingActor(this))
	{
		CurrentInteractableActor = Truck;
		CurrentTruckInteractType = ETruckInteractType::TurretSeat;
		return;
	}

	if (CurrentTruck != nullptr || bIsOnTruckCargo || bIsDrivingTruck || bIsUsingMountedWeapon)
	{
		return;
	}

	if (Truck->CargoSeatInteractTrigger && Truck->CargoSeatInteractTrigger->IsOverlappingActor(this))
	{
		CurrentInteractableActor = Truck;
		CurrentTruckInteractType = ETruckInteractType::CargoSeat;
		return;
	}

	if (Truck->DriverSeatInteractTrigger && Truck->DriverSeatInteractTrigger->IsOverlappingActor(this))
	{
		CurrentInteractableActor = Truck;
		CurrentTruckInteractType = ETruckInteractType::DriverSeat;
	}
}

void AFPSBaseCharacter::SetPlayerInfo(const Protocol::PosInfo& Info)
{
	if (PlayerInfo->object_id() != 0 && PlayerInfo->object_id() != Info.object_id())
		return;

	PlayerInfo->CopyFrom(Info);
	DestInfo->CopyFrom(Info);
}

void AFPSBaseCharacter::SetDestInfo(const Protocol::PosInfo& Info)
{
	if (PlayerInfo->object_id() != 0 && PlayerInfo->object_id() != Info.object_id())
		return;
	const Protocol::MoveState PreviousDestState = DestInfo->state();
	DestInfo->CopyFrom(Info);
	SetPlayerInfo(Info);

	if (bIsOnTruckCargo && CurrentTruck)
	{
		if (UBoxComponent* CargoBounds = CurrentTruck->GetCargoMoveBoundsComponent())
		{
			const FVector IncomingCargoLocalLocation(Info.x(), Info.y(), Info.z());
			const bool bWasCargoMoving =
				PreviousDestState == Protocol::MOVE_STATE_RUN ||
				PreviousDestState == Protocol::MOVE_STATE_JUMP;
			const bool bIsCargoMoving =
				Info.state() == Protocol::MOVE_STATE_RUN ||
				Info.state() == Protocol::MOVE_STATE_JUMP;

			if (!bHasReplicatedTruckCargoLocalLocation ||
				bIsCargoMoving ||
				bWasCargoMoving ||
				FVector::DistSquaredXY(IncomingCargoLocalLocation, ReplicatedTruckCargoLocalLocation) >
					FMath::Square(40.0f))
			{
				ReplicatedTruckCargoLocalLocation = IncomingCargoLocalLocation;
			}

			bHasReplicatedTruckCargoLocalLocation = true;
		}
		else
		{
			bHasReplicatedTruckCargoLocalLocation = false;
		}
	}
	else
	{
		bHasReplicatedTruckCargoLocalLocation = false;
	}
}

void AFPSBaseCharacter::EquipWeapon(AWeaponBase* Weapon)
{
	if (Weapon == nullptr) return;

	DestroyEquippedWeapon();

	Weapon->SetWeaponCollisionEnabled(false);
	Weapon->SetWeaponHidden(false);

	const FName SocketName = TEXT("Gun_socket");
	Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	Weapon->SetActorRelativeLocation(FVector(-8.883712f, 5.298776f, -0.142411f));
	Weapon->SetActorRelativeRotation(FRotator(-0.023171f, 82.465882f, 13.423545f));
	Weapon->SetActorRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	SetCurrentWeapon(Weapon);

	Weapon->SetWeaponUser(this);
	Weapon->SetOwner(this);

	if (IsLocallyControlled())
	{
		if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
		{
			if (PC->BasicW)
			{
				PC->BasicW->GetGunAR4();
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Network] %s equipped weapon (%s)."), *GetName(), *Weapon->GetName());
}

void AFPSBaseCharacter::DestroyEquippedWeapon()
{
	if (!IsValid(CurrentWeapon))
	{
		CurrentWeapon = nullptr;
		return;
	}

	AWeaponBase* WeaponToDestroy = CurrentWeapon;
	ClearCurrentWeapon();

	WeaponToDestroy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	WeaponToDestroy->Destroy();
}

void AFPSBaseCharacter::SendDeathPacket()
{
	Protocol::C_MOVE MovePkt;
	Protocol::PosInfo* Info = MovePkt.mutable_info();
	Info->set_object_id(PlayerInfo ? PlayerInfo->object_id() : 0);
	Info->set_x(GetActorLocation().X);
	Info->set_y(GetActorLocation().Y);
	Info->set_z(GetActorLocation().Z);
	Info->set_yaw(GetControlRotation().Yaw);
	Info->set_state(Protocol::MOVE_STATE_DEAD);

	SEND_PACKET(MovePkt);
}

void AFPSBaseCharacter::RemoveDeadBody()
{
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	if (!IsLocallyControlled())
	{
		Destroy();
	}
}

void AFPSBaseCharacter::SendMovePacket()
{
	if (bIsDead)
	{
		return;
	}

	Protocol::C_MOVE MovePkt;
	Protocol::PosInfo* Info = MovePkt.mutable_info();
	Info->set_object_id(PlayerInfo->object_id());
	Info->set_x(GetActorLocation().X);
	Info->set_y(GetActorLocation().Y);
	Info->set_z(GetActorLocation().Z);
	Info->set_yaw(GetControlRotation().Yaw);

	Protocol::MoveState MoveState = Protocol::MOVE_STATE_IDLE;
	if (GetCharacterMovement()->IsFalling())
	{
		MoveState = Protocol::MOVE_STATE_JUMP;
	}
	else if (bIsOnTruckCargo && CurrentTruck)
	{
		if (UBoxComponent* CargoBounds = CurrentTruck->GetCargoMoveBoundsComponent())
		{
			const FVector CargoLocalLocation =
				CargoBounds->GetComponentTransform().InverseTransformPosition(GetActorLocation());
			Info->set_x(CargoLocalLocation.X);
			Info->set_y(CargoLocalLocation.Y);
			Info->set_z(CargoLocalLocation.Z);

			const bool bHasMeaningfulCargoMovement =
				!bHasLastTruckCargoLocalLocationForMoveState ||
				FVector::DistSquaredXY(CargoLocalLocation, LastTruckCargoLocalLocationForMoveState) >
					FMath::Square(2.0f);

			MoveState = bHasMeaningfulCargoMovement
				? Protocol::MOVE_STATE_RUN
				: Protocol::MOVE_STATE_IDLE;
			LastTruckCargoLocalLocationForMoveState = CargoLocalLocation;
			bHasLastTruckCargoLocalLocationForMoveState = true;
		}
		else if (GetVelocity().SizeSquared2D() > FMath::Square(5.0f))
		{
			MoveState = Protocol::MOVE_STATE_RUN;
		}
	}
	else if (GetVelocity().SizeSquared2D() > FMath::Square(5.0f))
	{
		MoveState = Protocol::MOVE_STATE_RUN;
	}
	Info->set_state(MoveState);

	static Protocol::MoveState LastState = Protocol::MOVE_STATE_IDLE;
	if (LastState != Info->state())
	{
		LastState = Info->state();
	}

	SEND_PACKET(MovePkt);
}

void AFPSBaseCharacter::SetInteractableActor(AActor* NewActor)
{
	CurrentInteractableActor = NewActor;
}

void AFPSBaseCharacter::Interact()
{
	if (bIsUsingMountedWeapon)
	{
		StopFire();

		if (!CurrentTruck)
		{
			return;
		}

		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		{
			if (GameInstance->TryEnterTruckLocally(this, CurrentTruck, Protocol::TRUCK_SEAT_CARGO))
			{
				return;
			}
		}

		Protocol::C_ENTER_TRUCK EnterPkt;
		EnterPkt.set_truck_id(CurrentTruck->NetworkTruckId);
		EnterPkt.set_seat_type(Protocol::TRUCK_SEAT_CARGO);
		SEND_PACKET(EnterPkt);
		return;
	}

	if (CanInteractWithMountedWeapon())
	{
		if (CurrentInteractableActor->GetClass()->ImplementsInterface(UInteractInterface::StaticClass()))
		{
			IInteractInterface::Execute_Interact(CurrentInteractableActor, this);
		}
		return;
	}

	if (bIsOnTruckCargo)
	{
		if (CurrentTruck && CurrentTruck->TryEnterMountedWeapon(this))
		{
			return;
		}

		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		{
			if (GameInstance->TryExitTruckLocally(this))
			{
				return;
			}
		}

		Protocol::C_EXIT_TRUCK ExitPkt;
		SEND_PACKET(ExitPkt);
		return;
	}

	if (CurrentInteractableActor)
	{
		if (CurrentInteractableActor->GetClass()->ImplementsInterface(UInteractInterface::StaticClass()))
		{
			IInteractInterface::Execute_Interact(CurrentInteractableActor, this);
		}
	}
}

bool AFPSBaseCharacter::AddItem(EItemType NewItemType)
{
	NewItemType = NormalizeStageItemType(NewItemType);

	if (Inventory.Num() >= MaxItemCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory Full!"));
		return false;
	}

	Inventory.Add(NewItemType);

	if (OnInventoryUpdated.IsBound())
	{
		OnInventoryUpdated.Broadcast(Inventory);
	}

	UE_LOG(LogTemp, Log, TEXT("Item Added. Current: %d"), Inventory.Num());
	return true;
}

void AFPSBaseCharacter::AddStage2DistributedItem(EItemType NewItemType)
{
	NewItemType = NormalizeStageItemType(NewItemType);

	if (NewItemType == EItemType::None)
	{
		return;
	}

	Inventory.Add(NewItemType);

	if (OnInventoryUpdated.IsBound())
	{
		OnInventoryUpdated.Broadcast(Inventory);
	}
}

bool AFPSBaseCharacter::ConsumeInventoryItem(EItemType ItemType)
{
	ItemType = NormalizeStageItemType(ItemType);

	const int32 ItemIndex = Inventory.Find(ItemType);
	if (ItemIndex == INDEX_NONE)
	{
		return false;
	}

	Inventory.RemoveAt(ItemIndex);
	if (OnInventoryUpdated.IsBound())
	{
		OnInventoryUpdated.Broadcast(Inventory);
	}

	return true;
}

bool AFPSBaseCharacter::ConsumeStage2AmmoBullet()
{
	const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	if (GameInstance == nullptr || !GameInstance->IsInStage2World())
	{
		return true;
	}

	if (CurrentAmmoBulletCount <= 0)
	{
		CurrentAmmoBulletCount = 0;
		UpdateStage2AmmoUI();
		return false;
	}

	--CurrentAmmoBulletCount;
	UpdateStage2AmmoUI();
	return true;
}

void AFPSBaseCharacter::UpdateStage2AmmoUI() const
{
	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	AFPSPlayerController* PlayerController = Cast<AFPSPlayerController>(GetController());
	if (PlayerController == nullptr && GameInstance && GameInstance->MyPlayer == this)
	{
		PlayerController = Cast<AFPSPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	if (PlayerController && PlayerController->BasicW)
	{
		PlayerController->BasicW->SetRemainingAmmoCount(CurrentAmmoBulletCount);
	}
}

int32 AFPSBaseCharacter::GetInventoryItemCount(EItemType ItemType) const
{
	ItemType = NormalizeStageItemType(ItemType);

	int32 Count = 0;
	for (const EItemType InventoryItem : Inventory)
	{
		if (NormalizeStageItemType(InventoryItem) == ItemType)
		{
			++Count;
		}
	}

	return Count;
}

bool AFPSBaseCharacter::UseHealPack()
{
	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	if (GameInstance == nullptr || !GameInstance->IsInStage2World() || HealthComponent == nullptr)
	{
		return false;
	}

	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const bool bIsLocalPlayerCharacter =
		IsLocallyControlled() ||
		GameInstance->MyPlayer == this ||
		(CurrentTruck && CurrentTruck->GetDriverCharacter() == this && CurrentTruck->GetController() == LocalPlayerController);
	if (!bIsLocalPlayerCharacter || HealthComponent->IsAtFullHealth())
	{
		return false;
	}

	const bool bConsumedHealPack =
		ConsumeInventoryItem(EItemType::HealPack) ||
		ConsumeInventoryItem(EItemType::MedicalKit);
	if (!bConsumedHealPack)
	{
		RefreshStage2ItemUI();
		UE_LOG(LogTemp, Warning, TEXT("Cannot use Heal Pack: no Stage 1 Heal Packs remain."));
		return false;
	}

	Heal(HealPackHealAmount);
	RefreshStage2ItemUI();
	UE_LOG(LogTemp, Log, TEXT("Heal Pack used. Healed %.1f, remaining=%d"),
		HealPackHealAmount,
		GetInventoryItemCount(EItemType::HealPack) +
		GetInventoryItemCount(EItemType::MedicalKit));
	return true;
}

bool AFPSBaseCharacter::UseFuelCan()
{
	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	if (GameInstance == nullptr || !GameInstance->IsInStage2World() || !CurrentTruck)
	{
		return false;
	}

	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const bool bIsTruckOccupant =
		CurrentTruck &&
		(bIsDrivingTruck ||
			bIsOnTruckCargo ||
			bIsUsingMountedWeapon ||
			CurrentTruck->GetDriverCharacter() == this ||
			CurrentTruck->GetMountedWeaponUser() == this);
	const bool bIsLocalPlayerCharacter =
		IsLocallyControlled() ||
		GameInstance->MyPlayer == this ||
		(CurrentTruck->GetDriverCharacter() == this && CurrentTruck->GetController() == LocalPlayerController);
	if (!bIsTruckOccupant || !bIsLocalPlayerCharacter)
	{
		return false;
	}

	if (CurrentTruck->GetTruckFuel() >= CurrentTruck->GetTruckMaxFuel() - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (!ConsumeInventoryItem(EItemType::Fuel))
	{
		RefreshStage2ItemUI();
		UE_LOG(LogTemp, Warning, TEXT("Cannot use Fuel Can: no Stage 1 Fuel items remain."));
		return false;
	}

	CurrentTruck->RefuelTruck(FuelCanRefuelAmount);
	ShowTruckFuelOnHUD(CurrentTruck);
	CurrentTruck->SyncTruckStateToServer();
	RefreshStage2ItemUI();
	UE_LOG(LogTemp, Log, TEXT("Fuel Can used. Refueled %.1f, truck fuel=%.1f/%.1f, remaining=%d"),
		FuelCanRefuelAmount,
		CurrentTruck->GetTruckFuel(),
		CurrentTruck->GetTruckMaxFuel(),
		GetInventoryItemCount(EItemType::Fuel));
	return true;
}

bool AFPSBaseCharacter::UseTruckRepairKit()
{
	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	if (GameInstance == nullptr || !GameInstance->IsInStage2World() || !CurrentTruck)
	{
		return false;
	}

	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const bool bIsTruckOccupant =
		CurrentTruck &&
		(bIsDrivingTruck ||
			bIsOnTruckCargo ||
			bIsUsingMountedWeapon ||
			CurrentTruck->GetDriverCharacter() == this ||
			CurrentTruck->GetMountedWeaponUser() == this);
	const bool bIsLocalPlayerCharacter =
		IsLocallyControlled() ||
		GameInstance->MyPlayer == this ||
		(CurrentTruck->GetDriverCharacter() == this && CurrentTruck->GetController() == LocalPlayerController);
	if (!bIsTruckOccupant || !bIsLocalPlayerCharacter)
	{
		return false;
	}

	if (CurrentTruck->IsTruckDestroyed() ||
		CurrentTruck->GetTruckHealth() >= CurrentTruck->GetTruckMaxHealth() - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (!ConsumeInventoryItem(EItemType::TruckRepairKit))
	{
		RefreshStage2ItemUI();
		UE_LOG(LogTemp, Warning, TEXT("Cannot use Truck Repair Kit: no Stage 1 Repair Kits remain."));
		return false;
	}

	CurrentTruck->RepairTruck(TruckRepairKitHealAmount);
	ShowTruckHealthOnHUD(CurrentTruck);
	CurrentTruck->SyncTruckStateToServer(true);
	RefreshStage2ItemUI();
	UE_LOG(LogTemp, Log, TEXT("Truck Repair Kit used. Repaired %.1f, truck health=%.1f/%.1f, remaining=%d"),
		TruckRepairKitHealAmount,
		CurrentTruck->GetTruckHealth(),
		CurrentTruck->GetTruckMaxHealth(),
		GetInventoryItemCount(EItemType::TruckRepairKit));
	return true;
}

void AFPSBaseCharacter::RefreshStage2ItemUI()
{
	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	AFPSPlayerController* PlayerController = Cast<AFPSPlayerController>(GetController());
	if (PlayerController == nullptr && GameInstance && GameInstance->MyPlayer == this)
	{
		PlayerController = Cast<AFPSPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	if (GameInstance == nullptr || PlayerController == nullptr)
	{
		return;
	}

	if (PlayerController->L2BaseW)
	{
		PlayerController->L2BaseW->ItemSetting(
			GetInventoryItemCount(EItemType::Fuel),
			GetInventoryItemCount(EItemType::HealPack) + GetInventoryItemCount(EItemType::MedicalKit),
			GetInventoryItemCount(EItemType::TruckRepairKit));
	}

	if (PlayerController->BasicW)
	{
		const int32 AmmoBoxCount =
			GetInventoryItemCount(EItemType::Ammo) +
			GetInventoryItemCount(EItemType::CharacterAmmo);
		const int32 NewMaxAmmoBulletCount = FMath::Max(AmmoBoxCount, 0) * BulletsPerAmmoBox;

		if (MaxAmmoBulletCount != NewMaxAmmoBulletCount)
		{
			CurrentAmmoBulletCount = NewMaxAmmoBulletCount;
			MaxAmmoBulletCount = NewMaxAmmoBulletCount;
		}
		else
		{
			CurrentAmmoBulletCount = FMath::Clamp(CurrentAmmoBulletCount, 0, MaxAmmoBulletCount);
		}

		PlayerController->BasicW->SetAmmoCount(AmmoBoxCount);
		PlayerController->BasicW->SetRemainingAmmoCount(CurrentAmmoBulletCount);
	}
}

TArray<EItemType> AFPSBaseCharacter::OffloadItems()
{
	TArray<EItemType> ItemsToGive = Inventory;
	Inventory.Empty();

	if (OnInventoryUpdated.IsBound())
	{
		OnInventoryUpdated.Broadcast(Inventory);
	}

	return ItemsToGive;
}

void AFPSBaseCharacter::SetHealth(float currentHp,float maxHp) {

	AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController());
	if (PC == nullptr)
	{
		if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
		{
			if (GameInstance->MyPlayer == this)
			{
				PC = Cast<AFPSPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
			}
		}

		if (PC == nullptr && CurrentTruck && CurrentTruck->GetDriverCharacter() == this)
		{
			APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
			if (CurrentTruck->GetController() == LocalPlayerController)
			{
				PC = Cast<AFPSPlayerController>(LocalPlayerController);
			}
		}
	}

	if (PC)
	{
		if (PC->BasicW)
		{
			PC->BasicW->SetHealth(currentHp, maxHp);
		}

		if (PC->EffectW)
		{
			if (currentHp <= 40) {
				PC->EffectW->PlayAni_Effect(true);
				PC->EffectW->SpawnBloodEffects();
			}
			else
				PC->EffectW->PlayAni_Effect(false);
		}
	}

	if (currentHp <= 0.0f)
	{
		Die();
	}
}

void AFPSBaseCharacter::Heal(float h) {
	if (HealthComponent)
	{
		HealthComponent->Heal(h);
		SetHealth(HealthComponent->GetHealth(), HealthComponent->MaxGetHealth());
	}
}
