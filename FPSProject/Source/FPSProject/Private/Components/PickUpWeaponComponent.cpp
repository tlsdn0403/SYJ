// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/PickUpWeaponComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Weapon/WeaponBase.h"
#include "Engine/Engine.h" // 디버그 메시지 출력용

UPickUpWeaponComponent::UPickUpWeaponComponent()
{
	InitSphereRadius(32.f);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void UPickUpWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddDynamic(this, &UPickUpWeaponComponent::OnSphereBeginOverlap);
}

void UPickUpWeaponComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AFPSBaseCharacter* Character = Cast<AFPSBaseCharacter>(OtherActor);

	if (Character && Character->IsPlayerControlled())
	{
		// 디버깅 메시지: 플레이어가 무기 장착(픽업) 트리거에 진입했는지 확인
		UE_LOG(LogTemp, Log, TEXT("[PickUpWeaponComponent] '%s'  (Owner: '%s')."),
			*GetNameSafe(Character),
			*GetNameSafe(GetOwner()));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Green,
				FString::Printf(TEXT("Attatch Weapon: %s"), *GetNameSafe(Character))
			);
		}

		OnPickUp.Broadcast(Character);
		Character->SetCurrentWeapon(Cast<AWeaponBase>(GetOwner()));
		OnComponentBeginOverlap.RemoveAll(this); // 한 번만 실행 
	}
}