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
		AWeaponBase* Weapon = Cast<AWeaponBase>(GetOwner());
		if (Weapon == nullptr) return;

		OnComponentBeginOverlap.RemoveAll(this);

		// 내 캐릭터가 무기를 주웠으니 서버로 패킷 전송!
		Protocol::C_EQUIP_WEAPON EquipPkt;
		EquipPkt.set_itemobjectid(Weapon->ItemObjectId);
		SEND_PACKET(EquipPkt);
	}
}