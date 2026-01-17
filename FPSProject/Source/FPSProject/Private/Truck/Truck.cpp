// Fill out your copyright notice in the Description page of Project Settings.


#include "Truck/Truck.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"

ATruck::ATruck()
{
	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(RootComponent);
	InteractTrigger->InitSphereRadius(200.0f); // 범위 설정
}
void ATruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("Throttle", this, &ATruck::MoveForward);   // 가속/후진
	PlayerInputComponent->BindAxis("Steer", this, &ATruck::MoveRight);       // 핸들 좌우
	PlayerInputComponent->BindAxis("Brake", this, &ATruck::Brake);           // 브레이크
}

void ATruck::MoveForward(float Value)
{
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
	}
}


void ATruck::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (!Character) return;

	// 캐릭터를 조종하던 컨트롤러를 가져옴
	AController* PlayerController = Character->GetController();

	if (PlayerController)
	{
		//빙의대상을 캐릭터에서 트럭으로 변경
		PlayerController->Possess(this);


		UE_LOG(LogTemp, Log, TEXT("Truck Possessed!"));
	}
}