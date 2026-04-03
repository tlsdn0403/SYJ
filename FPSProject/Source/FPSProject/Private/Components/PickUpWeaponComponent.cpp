// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/PickUpWeaponComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Weapon/WeaponBase.h"
#include "Engine/Engine.h" // 디버그 메시지 출력용
#include "Characters/FPSPlayerController.h"
#include "HUD/InventoryWidget.h"
#include "ClientPacketHandler.h"
#include "Protocol.pb.h"

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

	if (Character && Character->IsLocallyControlled())
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
		AFPSPlayerController* PC = Cast<AFPSPlayerController>( Character->GetController()); 
		PC->InventoryW->GetGunAR4();	//총 체크 

		OnComponentBeginOverlap.RemoveAll(this); // 한 번만 실행 

		// 내 캐릭터가 무기를 주웠으니 서버로 패킷 전송!
		Protocol::C_EQUIP_WEAPON EquipPkt;
		EquipPkt.set_itemobjectid(1); // (나중에 맵 아이템 ID로 교체할 부분)
		SEND_PACKET(EquipPkt);

		UE_LOG(LogTemp, Error, TEXT("======== C_EQUIP_WEAPON 서버로 전송 완료! ========"));
	}
}