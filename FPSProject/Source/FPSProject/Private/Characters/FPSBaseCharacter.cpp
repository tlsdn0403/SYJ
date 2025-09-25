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

	// ����� �޽����� 5�ʰ� ǥ���մϴ�. 
	// -1 'Ű' �� �������ڰ� �޽����� ������Ʈ�ǰų� ���ΰ�ħ���� �ʵ��� �����մϴ�.
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


    // movement ���ε��� ����
    PlayerInputComponent->BindAxis("MoveForward", this, &AFPSBaseCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &AFPSBaseCharacter::MoveRight);

    // look ���ε��� ����
    PlayerInputComponent->BindAxis("Turn", this, &AFPSBaseCharacter::AddControllerYawInput);
    PlayerInputComponent->BindAxis("LookUp", this, &AFPSBaseCharacter::AddControllerPitchInput);

    // action ���ε��� ����
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AFPSBaseCharacter::StartJump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &AFPSBaseCharacter::StopJump);
}

void AFPSBaseCharacter::MoveForward(float Value)
{
    // ��� ������ ã��, �÷��̾ �ش� �������� �̵��ϰ��� �Ѵٴ� ���� ���
    FVector Direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::X);
    AddMovementInput(Direction, Value);
}

void AFPSBaseCharacter::MoveRight(float Value)
{
    // ��� ���������� ã��, �÷��̾ �ش� �������� �̵��ϰ��� �Ѵٴ� ���� ���
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