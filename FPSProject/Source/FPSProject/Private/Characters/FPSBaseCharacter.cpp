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
#include "ClientPacketHandler.h"

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
        // 바로 보내지 말고, 0.2초 뒤에 SendEnterGame 함수를 실행하도록 예약!
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([this]()
            {
                Protocol::C_ENTER_GAME EnterGamePkt;
                EnterGamePkt.set_playerindex(0);

                if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
                {
                    SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(EnterGamePkt);
                    GameInstance->SendPacket(SendBuffer);
                    UE_LOG(LogTemp, Warning, TEXT("[Network] 0.2초 대기 후 C_ENTER_GAME 안전하게 전송 완료!"));
                }
            }), 0.2f, false); // 0.2초 딜레이

        if (PC)
        {
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = false;
        }
    }

    // 인벤토리 UI 띄우기
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
    else // 남의 캐릭터인 경우
    {
        const Protocol::MoveState State = PlayerInfo->state();
        FVector CurrentLocation = GetActorLocation();
        FVector TargetLocation = FVector(DestInfo->x(), DestInfo->y(), DestInfo->z());

        // 1. 회전은 지금처럼 부드럽게 유지
        FRotator TargetRot(0.f, DestInfo->yaw(), 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 10.f));

        // 2. 점프 처리는 그대로 유지 (이벤트성)
        if (State == Protocol::MOVE_STATE_JUMP && RemoteLastState != Protocol::MOVE_STATE_JUMP)
        {
            if (!GetCharacterMovement()->IsFalling()) Jump();
        }
        RemoteLastState = State;

        // 3. [핵심] 위치 이동 로직 변경
        float DistToDest = FVector::Dist(CurrentLocation, TargetLocation);

        if (State == Protocol::MOVE_STATE_RUN || State == Protocol::MOVE_STATE_JUMP)
        {
            if (DistToDest > 2.0f) // 아주 작은 데드존만 설정
            {
                // 속도(15.0f)는 서버의 패킷 주기와 캐릭터 이동 속도에 맞춰 조절해봐!
                // SetActorLocation으로 직접 밀어버리면 물리 엔진 간섭 없이 목표까지 쭉 미끄러지듯 이동해.
                FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, 15.0f);
                SetActorLocation(NewLocation);
            }
        }
        else // IDLE 상태 등
        {
            if (DistToDest > 5.0f)
            {
                // IDLE인데 거리가 멀면 부드럽게 최종 위치로 수렴
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
    PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AFPSBaseCharacter::Interact);
}

void AFPSBaseCharacter::EnterTruckCargo(ATruck* Truck)
{
    if (!Truck || bIsOnTruckCargo)
    {
        return;
    }

    bIsOnTruckCargo = true;
    CurrentTruck = Truck;

    SetActorLocationAndRotation(
        Truck->GetCargoRideLocation(),
        Truck->GetCargoRideRotation()
    );

    AttachToActor(Truck, FAttachmentTransformRules::KeepWorldTransform);

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

    bIsUsingMountedWeapon = true;
    bIsOnTruckCargo = false;
    CurrentTruck = Truck;
    CurrentMountedWeapon = MountedWeapon;
    CurrentMountedWeapon->SetWeaponUser(this);

    SetActorLocationAndRotation(
        Truck->GetTurretSeatLocation(),
        Truck->GetTurretSeatRotation()
    );

    AttachToActor(Truck, FAttachmentTransformRules::KeepWorldTransform);

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
    }

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorLocationAndRotation(
        Truck->GetCargoRideLocation(),
        Truck->GetCargoRideRotation()
    );
    AttachToActor(Truck, FAttachmentTransformRules::KeepWorldTransform);

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

void AFPSBaseCharacter::SetCurrentWeapon(AWeaponBase* NewWeapon)
{
    CurrentWeapon = NewWeapon;

    if (!GetMesh()) return;

    if (CurrentWeapon != nullptr)
    {
        if (ArmedAnimClass)
        {
            GetMesh()->SetAnimInstanceClass(ArmedAnimClass);
        }
    }
    else
    {
        if (UnarmedAnimClass)
        {
            GetMesh()->SetAnimInstanceClass(UnarmedAnimClass);
        }
    }
}

void AFPSBaseCharacter::ClearCurrentWeapon()
{
    SetCurrentWeapon(nullptr);
}

void AFPSBaseCharacter::Fire()
{
    if (bIsUsingMountedWeapon && CurrentMountedWeapon)
    {
        CurrentMountedWeapon->SetWeaponUser(this);
        CurrentMountedWeapon->Fire();
        return;
    }

    if (GetCurrentWeapon())
    {
        GetCurrentWeapon()->Fire();
        return;
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

