// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/LootItemBase.h"

#include "Characters/FPSBaseCharacter.h"
#include "Characters/FPSPlayerController.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "HUD/InteractUIClass.h"

namespace
{
	FText GetDefaultPickupText(EItemType ItemType)
	{
		switch (ItemType)
		{
		case EItemType::Ammo:
			return FText::FromString(TEXT("Ammo"));
		case EItemType::Fuel:
			return FText::FromString(TEXT("Fuel"));
		case EItemType::MedicalKit:
			return FText::FromString(TEXT("Medical Kit"));
		case EItemType::CharacterAmmo:
			return FText::FromString(TEXT("Character Ammo"));
		case EItemType::MountedGunAmmo:
			return FText::FromString(TEXT("Mounted Gun Ammo"));
		case EItemType::TruckRepairKit:
			return FText::FromString(TEXT("Truck Repair Kit"));
		case EItemType::HealPack:
			return FText::FromString(TEXT("Heal Pack"));
		default:
			return FText::FromString(TEXT("Item"));
		}
	}
}

ALootItemBase::ALootItemBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SceneRoot);

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	WidgetComp->SetupAttachment(MeshComp);
	WidgetComp->SetTwoSided(true);
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);

	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(RootComponent);
	InteractTrigger->InitSphereRadius(20.0f);
}

void ALootItemBase::BeginPlay()
{
	Super::BeginPlay();

	if (WidgetComp)
	{
		WidgetComp->InitWidget();
	}

	if (InteractTrigger)
	{
		InteractTrigger->OnEnter.AddDynamic(this, &ALootItemBase::WidgetStart);
		InteractTrigger->OnExit.AddDynamic(this, &ALootItemBase::WidgetEnd);
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp ? WidgetComp->GetUserWidgetObject() : nullptr))
	{
		UI->SetInteractText(setText.IsEmpty() ? GetDefaultPickupText(ItemType) : setText);
	}
}

void ALootItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ALootItemBase::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (Character == nullptr)
	{
		return;
	}

	//if (Character->AddItem(ItemType))
	{
		bool t = false;
		if (AFPSPlayerController* PlayerController = Character->GetController<AFPSPlayerController>())
		{
			t = PlayerController->PickUp_Item(itemimage, HandWeight);
		}
		if (t) {//위젯에 추가 되었으면. 추가되지 않았으면 ==자리부족 ->그럼 삭제x
			Character->AddItem(ItemType);
			if (HandWeight > 1) {
				for (int i = 0; i < HandWeight; ++i) {
					Character->AddItem(EItemType::TT);
				}
			}
			Destroy();
		}
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Cannot pick up item: Inventory is full."));
}

void ALootItemBase::WidgetStart(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor))
	{
		return;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp ? WidgetComp->GetUserWidgetObject() : nullptr))
	{
		UI->PlayAni_PopUp(true);
	}

	if (MeshComp && OverlayMaterial)
	{
		MeshComp->SetOverlayMaterial(OverlayMaterial);
	}
}

void ALootItemBase::WidgetEnd(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor))
	{
		return;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp ? WidgetComp->GetUserWidgetObject() : nullptr))
	{
		UI->RePlayAni_PopUp();
	}

	if (MeshComp && OverlayMaterial)
	{
		MeshComp->SetOverlayMaterial(nullptr);
	}
}
