#include "Characters/FPSBaseCharacter.h"
#include "Protocol.pb.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/MountedMachineGun.h"
#include "Projectiles/FPSProjectile.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/HealthComponent.h"
#include "HUD/InventoryWidget.h"
#include "HUD/BaseUI.h"
#include "Characters/FPSPlayerController.h"
#include "Interface/InteractInterface.h"
#include "Truck/Truck.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimationAsset.h"
#include "ClientPacketHandler.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AFPSBaseCharacter::AFPSBaseCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 300.0f;
    CameraBoom->bUsePawnControlRotation = true;

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
}

AFPSBaseCharacter::~AFPSBaseCharacter()
{
    delete PlayerInfo;
    delete DestInfo;
}

void AFPSBaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    check(GEngine != nullptr);
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("We are using FPSCharacter."));
  
    AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController());

    if (IsLocallyControlled())
    {
        // [복구 1] 맵 로딩 후 0.2초 대기했다가 서버에 C_ENTER_GAME 보내기!
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([this]()
            {
                Protocol::C_ENTER_GAME EnterGamePkt;
                EnterGamePkt.set_playerindex(0); // 임시로 0번

                if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
                {
                    SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(EnterGamePkt);
                    GameInstance->SendPacket(SendBuffer);
                    UE_LOG(LogTemp, Warning, TEXT("[Network] 0.2초 대기 후 C_ENTER_GAME 안전하게 전송 완료!"));
                }
            }), 0.2f, false);

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
        if (PC->InventoryW)
        {
            PC->InventoryW->AddToViewport();
            PC->TimerW->AddToViewport();
        }
    }
}

void AFPSBaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const bool bIsAttachedToTruckSeat = bIsDrivingTruck || bIsOnTruckCargo || bIsUsingMountedWeapon;

    // 줌 했을 떄 FOV 확대
    if (ThirdPersonCameraComponent && CameraBoom)
    {
        // 조준중이면 Aiming 카메라 , 아니면 기존
        const float TargetFOV = bIsAiming ? AimingThirdPersonFOV : DefaultThirdPersonFOV;
		// 이것도 조준중이면 Aiming 카메라 붐
        const float TargetBoomLength = bIsAiming ? AimingBoomLength : DefaultBoomLength;\
	    // 이것도 조준중이면 Aiming 카메라 붐 소켓 오프셋
        const FVector TargetSocketOffset = bIsAiming ? AimingCameraBoomSocketOffset : DefaultCameraBoomSocketOffset;

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
    if (bIsUsingMountedWeapon && CurrentMountedWeapon)
    {
        CurrentMountedWeapon->UpdateAim(GetControlRotation());

        if (FPSCameraComponent)
        {
            FPSCameraComponent->SetWorldLocationAndRotation(
                CurrentMountedWeapon->GetCameraLocation(),
                CurrentMountedWeapon->GetCameraRotation()
            );
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
    else if (bIsAttachedToTruckSeat)
    {
        return;
    }
    else // 남의 캐릭터일 때
    {
        const Protocol::MoveState State = PlayerInfo->state();
        FVector CurrentLocation = GetActorLocation();
        FVector TargetLocation = FVector(DestInfo->x(), DestInfo->y(), DestInfo->z());

        // 1. 회전 보간 (부드럽게 15.f 적용)
        FRotator TargetRot(0.f, DestInfo->yaw(), 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 15.f));

        // 2. 점프 이벤트 유지
        if (State == Protocol::MOVE_STATE_JUMP && RemoteLastState != Protocol::MOVE_STATE_JUMP)
        {
            if (!GetCharacterMovement()->IsFalling()) Jump();
        }
        RemoteLastState = State;

        // 3. [복구 2] AddMovementInput 버리고 VInterpTo로 직접 밀어주기!
        float DistToDest = FVector::Dist(CurrentLocation, TargetLocation);

        if (State == Protocol::MOVE_STATE_RUN || State == Protocol::MOVE_STATE_JUMP)
        {
            if (DistToDest > 2.0f) // 오차가 작을 때만 보간
            {
                FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, 15.0f);
                SetActorLocation(NewLocation);
            }
        }
        else // IDLE 상태
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
}

void AFPSBaseCharacter::EnterTruckDriverSeat(ATruck* Truck)
{
    if (!Truck || bIsDrivingTruck || bIsOnTruckCargo || bIsUsingMountedWeapon)
    {
        return;
    }

    CurrentTruck = Truck;
    bIsDrivingTruck = true;
    bIsAiming = false;
    StopFire();
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
        Truck->GetActorRotation()
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
    CurrentTruckInteractType = ETruckInteractType::None;
    CurrentInteractableActor = nullptr;

    ApplyDefaultAnimationClass();
}

void AFPSBaseCharacter::EnterTruckCargo(ATruck* Truck)
{
    if (!Truck || bIsOnTruckCargo)
    {
        return;
    }

    StopFire();
    bIsOnTruckCargo = true;
    bIsDrivingTruck = false;
    bIsAiming = false;
    CurrentTruck = Truck;

    SetActorLocationAndRotation(
        Truck->GetCargoRideLocation(),
        Truck->GetCargoRideRotation()
    );

    AttachToComponent(Truck->CargoRidePoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}

void AFPSBaseCharacter::ExitTruckCargo()
{
    if (!bIsOnTruckCargo || !CurrentTruck)
    {
        return;
    }

    ATruck* Truck = CurrentTruck;

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorLocation(Truck->GetCargoExitLocation());

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    bIsOnTruckCargo = false;
    CurrentTruck = nullptr;
}

void AFPSBaseCharacter::EnterMountedWeapon(ATruck* Truck, AMountedMachineGun* MountedWeapon)
{
    if (!Truck || !MountedWeapon || bIsUsingMountedWeapon)
    {
        return;
    }

    StopFire();
    bIsUsingMountedWeapon = true;
    bIsOnTruckCargo = false;
    bIsDrivingTruck = false;
    bIsAiming = false;
    CurrentTruck = Truck;
    CurrentMountedWeapon = MountedWeapon;
    CurrentMountedWeapon->SetWeaponUser(this);

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
        FPSCameraComponent->SetActive(true);
    }

    if (Controller)
    {
        Controller->SetControlRotation(MountedWeapon->GetCameraRotation());
    }
}

void AFPSBaseCharacter::ExitMountedWeapon()
{
    if (!bIsUsingMountedWeapon || !CurrentTruck)
    {
        return;
    }

    ATruck* Truck = CurrentTruck;

    if (CurrentMountedWeapon)
    {
        CurrentMountedWeapon->SetWeaponUser(nullptr);
    }

    StopFire();
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorLocationAndRotation(
        Truck->GetCargoRideLocation(),
        Truck->GetCargoRideRotation()
    );
    AttachToComponent(Truck->CargoRidePoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

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
    bIsOnTruckCargo = true;
    ApplyDefaultAnimationClass();
}

bool AFPSBaseCharacter::CanInteractWithMountedWeapon() const
{
    return CurrentInteractableActor != nullptr
        && CurrentTruckInteractType == ETruckInteractType::TurretSeat;
}

void AFPSBaseCharacter::MoveForward(float Value)
{
    if (bIsUsingMountedWeapon)
    {
        return;
    }

    if (Controller != nullptr && Value != 0.0f)
    {
        const FRotator ControlRot = Controller->GetControlRotation();
        const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);

        const FVector Direction = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AFPSBaseCharacter::MoveRight(float Value)
{
    if (bIsUsingMountedWeapon)
    {
        return;
    }

    if (Controller != nullptr && Value != 0.0f)
    {
        const FRotator ControlRot = Controller->GetControlRotation();
        const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);

        const FVector Direction = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }
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
    if (bIsDrivingTruck || bIsUsingMountedWeapon || bIsOnTruckCargo || !CurrentWeapon)
    {
        return;
    }

    bIsAiming = true;
}

void AFPSBaseCharacter::StopAim()
{
    bIsAiming = false;
}

void AFPSBaseCharacter::SetCurrentWeapon(AWeaponBase* NewWeapon)
{
    CurrentWeapon = NewWeapon;

    if (bIsDrivingTruck)
    {
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
    SetCurrentWeapon(nullptr);
    bIsAiming = false;
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
        return;
    }

    if (GetCurrentWeapon())
    {
        GetCurrentWeapon()->Fire();
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
    DestInfo->CopyFrom(Info);
    SetPlayerInfo(Info);
}

void AFPSBaseCharacter::EquipWeaponFromField(AWeaponBase* Weapon)
{
    if (Weapon == nullptr) return;

    // 1. �ٴڿ� �����Ǿ� �ִ� ������ �浹�� ���
    Weapon->SetActorEnableCollision(false);

    // 2. ���Ͽ� ���� (SnapToTarget�� ��� ���� ��ġ�� �����̵��մϴ�.)
    const FName SocketName = TEXT("Gun_socket");
    Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
    // 3. ��ġ ���� (Location)
    Weapon->SetActorRelativeLocation(FVector(-35.209697f, 2.353551f, 0.508678f));

    // ȸ�� ���� (Rotation - Pitch, Yaw, Roll ����)
    Weapon->SetActorRelativeRotation(FRotator(1.090108f, -88.966904f, -4.015320f));

    // ������ ���� (Scale) - ���� 0.15��� �۾����� �ϴϱ�!
    Weapon->SetActorRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));

    // 4. ĳ���� ���� ������Ʈ �� �ִϸ��̼�Instance ����
    SetCurrentWeapon(Weapon);

    UE_LOG(LogTemp, Log, TEXT("[Network] %s�� �ٴڿ� �ִ� ����(%s)�� �����߽��ϴ�."), *GetName(), *Weapon->GetName());
}

void AFPSBaseCharacter::SendMovePacket()
{
    Protocol::C_MOVE MovePkt;
    Protocol::PosInfo* Info = MovePkt.mutable_info();

    Info->set_object_id(PlayerInfo->object_id());
    Info->set_x(GetActorLocation().X);
    Info->set_y(GetActorLocation().Y);
    Info->set_z(GetActorLocation().Z);
    Info->set_yaw(GetControlRotation().Yaw);

    if (GetCharacterMovement()->IsFalling())
    {
        Info->set_state(Protocol::MOVE_STATE_JUMP);
    }
    else if (GetVelocity().Size() > 0.f)
    {
        Info->set_state(Protocol::MOVE_STATE_RUN);
    }
    else
    {
        Info->set_state(Protocol::MOVE_STATE_IDLE);
    }

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
        ExitMountedWeapon();
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
        ExitTruckCargo();
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

