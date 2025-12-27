// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSBaseCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/WeaponBase.h"
#include "Projectiles/FPSProjectile.h"

// Sets default values
AFPSBaseCharacter::AFPSBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


    // 일인칭 카메라 컴포넌트 생성.
    FPSCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    check(FPSCameraComponent != nullptr);

    // 캡슐 컴포넌트에 카메라 컴포넌트 어테치
    FPSCameraComponent->SetupAttachment(CastChecked<USceneComponent, UCapsuleComponent>(GetCapsuleComponent()));

    // 카메라가 눈 약간 위에 위치하도록
    FPSCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f + BaseEyeHeight));

    // 폰이 카메라 회전을 제어함
    FPSCameraComponent->bUsePawnControlRotation = true;


    // 소유 플레이어의 일인칭 메시 컴포넌트를 생성.
    FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
    check(FPSMesh != nullptr);

    // 소유 플레이어만 이 메시를 볼 수 있도록 설정
    FPSMesh->SetOnlyOwnerSee(true);

    // FPS 메시를 FPS 카메라에 어태치합니다.
    FPSMesh->SetupAttachment(FPSCameraComponent);

    // 일부 인바이런먼트 섀도를 비활성화하여 단일 메시 같은 느낌을 보존
    FPSMesh->bCastDynamicShadow = false;
    FPSMesh->CastShadow = false;

    // 플레이어가 자기 몸뚱아리 못보도록 설정
   /* GetMesh()->SetOwnerNoSee(true);*/
}

// Called when the game starts or when spawned
void AFPSBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	check(GEngine != nullptr);

	// 디버그 메시지를 5초간 표시합니다. 
	// -1 '키' 값 실행인자가 메시지가 업데이트되거나 새로고침되지 않도록 방지합니다.
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("We are using FPSCharacter."));
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
}

void AFPSBaseCharacter::MoveForward(float Value)
{
    // 어디가 앞인지 찾고, 플레이어가 해당 방향으로 이동하고자 한다는 것을 기록
    FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::X);
    AddMovementInput(Direction, Value);
}

void AFPSBaseCharacter::MoveRight(float Value)
{
    // 어디가 오른쪽인지 찾고, 플레이어가 해당 방향으로 이동하고자 한다는 것을 기록
    FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::Y);
    AddMovementInput(Direction, Value);
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
    if (CurrentWeapon)
    {
        CurrentWeapon->Fire();
        return;
    }
}
