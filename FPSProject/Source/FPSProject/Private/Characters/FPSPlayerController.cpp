// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSPlayerController.h"
#include "Characters/FPSBaseCharacter.h"

//void AFPSPlayerController::SetupInputComponent()
//{
//    Super::SetupInputComponent();
//
//    InputComponent->BindAxis("MoveForward", this, &AMyPlayerController::MoveForward);
//    InputComponent->BindAxis("MoveRight", this, &AMyPlayerController::MoveRight);
//    InputComponent->BindAxis("Turn", this, &AMyPlayerController::Turn);
//    InputComponent->BindAxis("LookUp", this, &AMyPlayerController::LookUp);
//
//    InputComponent->BindAction("Jump", IE_Pressed, this, &AMyPlayerController::JumpPressed);
//    InputComponent->BindAction("Jump", IE_Released, this, &AMyPlayerController::JumpReleased);
//    InputComponent->BindAction("Fire", IE_Pressed, this, &AMyPlayerController::Fire);
//
//    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMyPlayerController::Press1);
//}

// Called to bind functionality to input
void AFPSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    //// 앞으로 이동 및 뒤로 이동 입력을 처리
    //void MoveForward(float Value);

    //// 오른쪽 이동 및 왼쪽 이동 입력을 처리
    //void MoveRight(float Value);

    //// 키가 눌릴 경우 점프 플래그를 설정
    //void StartJump();

    //// 키가 떼어질 경우 점프 플래그를 지움
    //void StopJump();

    //void Fire();
    //void OnPress1();

    // movement 바인딩을 구성
    InputComponent->BindAxis("MoveForward", this, &AFPSPlayerController::MoveForward);
    InputComponent->BindAxis("MoveRight", this, &AFPSPlayerController::MoveRight);

    // look 바인딩을 구성
    //InputComponent->BindAxis("Turn", this, &AFPSPlayerController::AddControllerYawInput);
    //InputComponent->BindAxis("LookUp", this, &AFPSPlayerController::AddControllerPitchInput);

    // action 바인딩을 구성
    InputComponent->BindAction("Jump", IE_Pressed, this, &AFPSPlayerController::StartJump);
    InputComponent->BindAction("Jump", IE_Released, this, &AFPSPlayerController::StopJump);

    // Fire 액션 바인딩을 구성
    InputComponent->BindAction("Fire", IE_Pressed, this, &AFPSPlayerController::Fire);

  //  InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AFPSPlayerController::OnPress1);
}


void AFPSPlayerController::MoveForward(float Value)
{
    AFPSBaseCharacter* Char = Cast<AFPSBaseCharacter>(GetPawn());
    if (Char)
        Char->MoveForward(Value);
}

void AFPSPlayerController::MoveRight(float Value)
{
    AFPSBaseCharacter* Char = Cast<AFPSBaseCharacter>(GetPawn());
    if (Char)
        Char->MoveRight(Value);
}

void AFPSPlayerController::Fire()
{
    AFPSBaseCharacter* Char = Cast<AFPSBaseCharacter>(GetPawn());
    if (Char)
        Char->Fire();
}


void AFPSPlayerController::StopJump()
{
    AFPSBaseCharacter* Char = Cast<AFPSBaseCharacter>(GetPawn());
    if (Char)
        Char->StopJump();
}

void AFPSPlayerController::StartJump()
{
    AFPSBaseCharacter* Char = Cast<AFPSBaseCharacter>(GetPawn());
    if (Char)
        Char->StartJump();
}

