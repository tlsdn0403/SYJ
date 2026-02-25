// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSBaseCharacter.h"
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


// ---------------------------------- 이동 , 점프, 발사 관련 함수들 ----------------------------------
void AFPSBaseCharacter::MoveForward(float Value)
{
 
    if (Controller != nullptr && Value != 0.0f)
    {
        // 어디가 앞인지 찾고, 플레이어가 해당 방향으로 이동하고자 한다는 것을 기록
        FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AFPSBaseCharacter::MoveRight(float Value)
{
  

    if (Controller != nullptr && Value != 0.0f)
    {
        // 어디가 오른쪽인지 찾고, 플레이어가 해당 방향으로 이동하고자 한다는 것을 기록
        FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }
}

void AFPSBaseCharacter::StartJump()
{
    bPressedJump = true;
}

void AFPSBaseCharacter::StopJump()
{
    bPressedJump = false;
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
//--------------------------------------------------------------------------------------------

// ---------------------------------- 상호작용 관련 함수들 ----------------------------------

void AFPSBaseCharacter::SetInteractableActor(AActor* NewActor)
{
    CurrentInteractableActor = NewActor;
}


void AFPSBaseCharacter::Interact()
{
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