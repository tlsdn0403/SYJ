// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/LootItemBase.h"

#include "Characters/FPSBaseCharacter.h"
#include "Characters/FPSPlayerController.h"
#include "ClientPacketHandler.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "FPSProjectGameInstance.h"
#include "HUD/InteractUIClass.h"
#include "Protocol.pb.h"

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

	FString StripPiePrefix(const FString& InValue)
	{
		FString Result = InValue;
		const FString Prefix = TEXT("UEDPIE_");

		int32 PrefixIndex = Result.Find(Prefix);
		while (PrefixIndex != INDEX_NONE)
		{
			int32 SuffixIndex = PrefixIndex + Prefix.Len();
			while (SuffixIndex < Result.Len() && FChar::IsDigit(Result[SuffixIndex]))
			{
				++SuffixIndex;
			}

			if (SuffixIndex < Result.Len() && Result[SuffixIndex] == TEXT('_'))
			{
				Result.RemoveAt(PrefixIndex, SuffixIndex - PrefixIndex + 1, EAllowShrinking::No);
			}
			else
			{
				break;
			}

			PrefixIndex = Result.Find(Prefix);
		}

		return Result;
	}

	FString BuildStableActorKey(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return FString();
		}

		const FVector Location = Actor->GetActorLocation();
		const FIntVector QuantizedLocation(
			FMath::RoundToInt(Location.X),
			FMath::RoundToInt(Location.Y),
			FMath::RoundToInt(Location.Z));

		return FString::Printf(
			TEXT("%s:%d:%d:%d"),
			*StripPiePrefix(Actor->GetClass()->GetPathName()),
			QuantizedLocation.X,
			QuantizedLocation.Y,
			QuantizedLocation.Z);
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

	if (NetworkItemId == 0)
	{
		NetworkItemId = ResolveDefaultNetworkItemId();
	}

	if (UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
	{
		if (GameInstance->IsConnectedToGameServer())
		{
			GameInstance->RegisterNetworkLootItem(this);
		}
	}
}

void ALootItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ALootItemBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
	{
		GameInstance->UnregisterNetworkLootItem(NetworkItemId);
	}

	Super::EndPlay(EndPlayReason);
}

void ALootItemBase::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (Character == nullptr || bPickupPending)
	{
		return;
	}

	if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(Character->GetGameInstance()))
	{
		if (GameInstance->IsConnectedToGameServer())
		{
			if (AFPSPlayerController* PlayerController = Character->GetController<AFPSPlayerController>())
			{
				const bool bAddedToInventory = PlayerController->PickUp_Item(itemimage, HandWeight);
				if (bAddedToInventory)
				{
					bPickupPending = true;
					SetNetworkItemActive(false);

					Protocol::C_PICKUP_LOOT_ITEM PickupPkt;
					PickupPkt.set_item_object_id(NetworkItemId);
					PickupPkt.set_should_respawn(bRespawnOnPickup);
					PickupPkt.set_respawn_delay(RespawnDelay);
					GameInstance->SendPacket(ClientPacketHandler::MakeSendBuffer(PickupPkt));

					Character->AddItem(ItemType);
					if (HandWeight > 1)
					{
						for (int32 i = 0; i < HandWeight; ++i)
						{
							Character->AddItem(EItemType::TT);
						}
					}
				}
			}
			return;
		}
	}

	bool bAdded = false;
	if (AFPSPlayerController* PlayerController = Character->GetController<AFPSPlayerController>())
	{
		bAdded = PlayerController->PickUp_Item(itemimage, HandWeight);
	}

	if (bAdded)
	{
		Character->AddItem(ItemType);
		if (HandWeight > 1)
		{
			for (int32 i = 0; i < HandWeight; ++i)
			{
				Character->AddItem(EItemType::TT);
			}
		}
		Destroy();
	}
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

void ALootItemBase::ConfigureNetworkItem(uint64 InNetworkItemId, bool bInRespawnOnPickup, float InRespawnDelay)
{
	if (NetworkItemId != 0 && NetworkItemId != InNetworkItemId)
	{
		if (UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
		{
			GameInstance->UnregisterNetworkLootItem(NetworkItemId);
		}
	}

	NetworkItemId = InNetworkItemId;
	bRespawnOnPickup = bInRespawnOnPickup;
	RespawnDelay = InRespawnDelay;

	if (UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
	{
		if (GameInstance->IsConnectedToGameServer())
		{
			GameInstance->RegisterNetworkLootItem(this);
		}
	}
}

void ALootItemBase::SetNetworkItemActive(bool bIsActive)
{
	bPickupPending = !bIsActive;
	SetActorHiddenInGame(!bIsActive);
	SetActorEnableCollision(bIsActive);

	if (MeshComp)
	{
		MeshComp->SetVisibility(bIsActive, true);
		MeshComp->SetCollisionEnabled(bIsActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	if (InteractTrigger)
	{
		InteractTrigger->SetCollisionEnabled(bIsActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		InteractTrigger->SetGenerateOverlapEvents(bIsActive);
	}

	if (WidgetComp)
	{
		WidgetComp->SetVisibility(bIsActive);
	}
}

uint64 ALootItemBase::ResolveDefaultNetworkItemId() const
{
	return static_cast<uint64>(GetTypeHash(BuildStableActorKey(this)));
}
