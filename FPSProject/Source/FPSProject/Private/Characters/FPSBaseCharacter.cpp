// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/FPSBaseCharacter.h"
#include "Protocol.pb.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/WeaponBase.h"
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


// Sets default values
AFPSBaseCharacter::AFPSBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


    // 스프링 암 생성
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 300.0f;                                                               // 캐릭터와 카메라 사이 거리. 조절 가능
    CameraBoom->bUsePawnControlRotation = true;                                                         // 폰 컨트롤러의 회전 값을 따라감

    // 카메라 생성 후 스프링암에 결합
    ThirdPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    ThirdPersonCameraComponent->bUsePawnControlRotation = false;                                        // 카메라자체는 컨트롤 안함. 붐이 모든 회전 제어


    // 소유 플레이어의 일인칭 메시 컴포넌트를 생성.
    FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ThirdPersonMesh"));
    check(FPSMesh != nullptr);



    // 일부 인바이런먼트 섀도를 비활성화하여 단일 메시 같은 느낌을 보존
    FPSMesh->bCastDynamicShadow = false;
    FPSMesh->CastShadow = false;


    //체력 컴포넌트 추가
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

    PlayerInfo = new Protocol::PosInfo();
    DestInfo = new Protocol::PosInfo();

	// 캐릭터가 컨트롤러 없이도 물리 시뮬레이션을 계속하도록 설정
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    // 임시방편으로 캐릭터가 미는 거 막기
    GetCharacterMovement()->bEnablePhysicsInteraction = false;
}

AFPSBaseCharacter::~AFPSBaseCharacter()
{
    delete PlayerInfo;
    delete DestInfo;
}

// Called when the game starts or when spawned
void AFPSBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	check(GEngine != nullptr);

	// 디버그 메시지를 5초간 표시
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("We are using FPSCharacter."));
  
    AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController());
    if (PC)
    {
        if (PC->InventoryW)
        {
            PC->InventoryW->AddToViewport();
            PC->TimerW->AddToViewport();
        }
    }
}

// Called every frame
void AFPSBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    // 1. 내 캐릭터인 경우
    if (IsLocallyControlled())
    {
        MovePacketSendTimer += DeltaTime;
        if (MovePacketSendTimer >= MOVE_PACKET_SEND_DELAY)
        {
            MovePacketSendTimer = 0.f;
            SendMovePacket();
        }
    }
    // 2. 남의 캐릭터인 경우
    else
    {
        const Protocol::MoveState State = PlayerInfo->state();

        FVector CurrentLocation = GetActorLocation();
        FVector TargetLocation = FVector(DestInfo->x(), DestInfo->y(), DestInfo->z());
        float DistToDest2D = FVector::Dist2D(CurrentLocation, TargetLocation);

        FRotator TargetRot(0.f, DestInfo->yaw(), 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 10.f));

        // 이전 상태가 JUMP가 아니었는데, 지금 JUMP로 바뀌었다면 딱 한 번만 점프!
        if (State == Protocol::MOVE_STATE_JUMP && RemoteLastState != Protocol::MOVE_STATE_JUMP)
        {
            if (!GetCharacterMovement()->IsFalling())
            {
                Jump();
            }
        }

        // 이번 프레임의 상태를 '이전 상태'로 저장해 둡니다. (다음 프레임에서 비교하기 위해)
        RemoteLastState = State;

        // [A] 점프 상태
        if (State == Protocol::MOVE_STATE_JUMP)
        {
            // 점프 중 이동 입력
            FVector MoveDir = TargetLocation - CurrentLocation;
            MoveDir.Z = 0.f;
            if (MoveDir.Size() > 10.f)
            {
                MoveDir.Normalize();
                AddMovementInput(MoveDir);
            }
        }
        // [B] 달리기 상태
        else if (State == Protocol::MOVE_STATE_RUN)
        {
            if (DistToDest2D > 10.0f)
            {
                FVector MoveDir = TargetLocation - CurrentLocation;
                MoveDir.Z = 0.f;
                MoveDir.Normalize();
                AddMovementInput(MoveDir);
            }
        }
        // [C] 가만히 있는 상태 (IDLE)
        else
        {
            if (DistToDest2D > 5.0f)
            {
                TargetLocation.Z = CurrentLocation.Z;
                SetActorLocation(TargetLocation);
            }
            // IDLE일 때는 회전을 즉시 맞춤
            SetActorRotation(FRotator(0.f, DestInfo->yaw(), 0.f));
        }
    }
}

// Called to bind functionality to input
void AFPSBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);


    // movement 바인딩을 구성
    PlayerInputComponent->BindAxis("MoveForward", this, &AFPSBaseCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &AFPSBaseCharacter::MoveRight);

    // look 바인딩을 구성
    PlayerInputComponent->BindAxis("Turn", this, &AFPSBaseCharacter::AddControllerYawInput);
    PlayerInputComponent->BindAxis("LookUp", this, &AFPSBaseCharacter::AddControllerPitchInput);

    // action 바인딩을 구성
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AFPSBaseCharacter::StartJump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &AFPSBaseCharacter::StopJump);

	// Fire 액션 바인딩을 구성
    PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AFPSBaseCharacter::Fire);

    PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AFPSBaseCharacter::Interact); // Interact 액션 바인딩
}

// ---------------------------- 트럭 탑승, 하차 ----------------------------------
void AFPSBaseCharacter::EnterTruckCargo(ATruck* Truck)
{
    if (!Truck || bIsOnTruckCargo)
    {
        return;
    }

    bIsOnTruckCargo = true;
    CurrentTruck = Truck;

    // 적재함 위치로 이동
    SetActorLocationAndRotation(
        Truck->GetCargoRideLocation(),
        Truck->GetCargoRideRotation()
    );

    // 트럭에 부착
    AttachToActor(Truck, FAttachmentTransformRules::KeepWorldTransform);

	// 캐릭터 움직임 멈추고, 걷기 모드로 설정 (점프나 낙하 방지)
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


// ---------------------------------- 이동 , 점프, 발사 관련 함수들 ----------------------------------
void AFPSBaseCharacter::MoveForward(float Value)
{
    if (Controller != nullptr && Value != 0.0f)
    {
        // 어디가 앞인지 찾고, 플레이어가 해당 방향으로 이동하고자 한다는 것을 기록
        const FRotator ControlRot = Controller->GetControlRotation();
        const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);

        const FVector Direction = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AFPSBaseCharacter::MoveRight(float Value)
{
    if (Controller != nullptr && Value != 0.0f)
    {
        // 어디가 오른쪽인지 찾고, 플레이어가 해당 방향으로 이동하고자 한다는 것을 기록
        const FRotator ControlRot = Controller->GetControlRotation();
        const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);

        const FVector Direction = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }

}

void AFPSBaseCharacter::StartJump()
{
    Jump();
    bPressedJump = true;
    SendMovePacket();
}

void AFPSBaseCharacter::StopJump()
{
    StopJumping();
    bPressedJump = false;
}

//--------------------------------------------------------------------------------------------

// ---------------------------------- 총기 관련 함수들 ----------------------------------

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
    // 현재 무기가 있으면 무기의 Fire 호출(총구에서 발사)
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

void AFPSBaseCharacter::SendMovePacket()
{
    Protocol::C_MOVE MovePkt;
    Protocol::PosInfo* Info = MovePkt.mutable_info();

    // 내 ID 설정
    Info->set_object_id(PlayerInfo->object_id());

    // 내 위치, 회전값 설정
    Info->set_x(GetActorLocation().X);
    Info->set_y(GetActorLocation().Y);
    Info->set_z(GetActorLocation().Z);
    Info->set_yaw(GetControlRotation().Yaw);

    // 현재 내가 이동중인지 판단하여 State 설정
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

    // 상태가 바뀔 때 로그를 찍어보세요.
    static Protocol::MoveState LastState = Protocol::MOVE_STATE_IDLE;
    if (LastState != Info->state())
    {
        // UE_LOG(LogTemp, Warning, TEXT("State Changed: %d"), Info->state());
        LastState = Info->state();
    }

    // 버퍼에 담아서 서버로 전송
    SEND_PACKET(MovePkt);
}

//--------------------------------------------------------------------------------------------

// ---------------------------------- 상호작용 관련 함수들 ----------------------------------

void AFPSBaseCharacter::SetInteractableActor(AActor* NewActor)
{
    CurrentInteractableActor = NewActor;
}




void AFPSBaseCharacter::Interact()
{

    if (bIsOnTruckCargo)
    {
        // 탑승중이면 내리도록
        ExitTruckCargo();
        return;
    }
    if (CurrentInteractableActor)
    {
        // 해당 액터가 인터페이스를 가지고 있는지 확인
        if (CurrentInteractableActor->GetClass()->ImplementsInterface(UInteractInterface::StaticClass()))
        {
            // 인터페이스의 Interact 함수 실행
			IInteractInterface::Execute_Interact(CurrentInteractableActor, this); 
        }
    }
}

//--------------------------------------------------------------------------------------------

// ---------------------------------- 인벤토리 관련 함수들 ----------------------------------


bool AFPSBaseCharacter::AddItem(EItemType NewItemType)
{

    if (Inventory.Num() >= MaxItemCount)
    {
        // 꽉 찼다는 알려주기? 
        UE_LOG(LogTemp, Warning, TEXT("Inventory Full!"));
        return false;
    }

    Inventory.Add(NewItemType);

    // UI 업데이트 알림
    if (OnInventoryUpdated.IsBound())  // IsBound() -> 바인딩 된 함수가 있는지? 
    {
        // 현재 인벤토리에 얼마나 찾는지 알려줌
        OnInventoryUpdated.Broadcast(Inventory);
    }

    UE_LOG(LogTemp, Log, TEXT("Item Added. Current: %d"), Inventory.Num());
    return true;
}

TArray<EItemType> AFPSBaseCharacter::OffloadItems()
{
    // 현재 가진 아이템 복사
    TArray<EItemType> ItemsToGive = Inventory;

    // 인벤토리 비우기
    Inventory.Empty();

    // UI 갱신 (빈 배열 전달)
    if (OnInventoryUpdated.IsBound())
    {
        OnInventoryUpdated.Broadcast(Inventory);
    }

    return ItemsToGive;
}