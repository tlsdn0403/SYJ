// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSBaseCharacter.h"

// Sets default values
AFPSBaseCharacter::AFPSBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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