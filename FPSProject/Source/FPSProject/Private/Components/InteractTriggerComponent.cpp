// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/InteractTriggerComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "GameFramework/Actor.h"
#include "Truck/Truck.h"

bool UInteractTriggerComponent::IsAvailableForCharacter(const AFPSBaseCharacter* Character) const
{
	if (Character == nullptr)
	{
		return false;
	}

	if (!bRequiresTruckCargo)
	{
		return true;
	}

	return Character->IsOnTruckCargo() && Character->CurrentTruck == Cast<ATruck>(GetOwner());
}

void UInteractTriggerComponent::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddDynamic(this, &UInteractTriggerComponent::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &UInteractTriggerComponent::OnOverlapEnd);
}

void UInteractTriggerComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AFPSBaseCharacter* Character = Cast<AFPSBaseCharacter>(OtherActor);
	if (Character && Character->IsPlayerControlled() && IsAvailableForCharacter(Character))
	{
		Character->SetInteractableActor(GetOwner());
		Character->SetCurrentTruckInteractType(InteractType);
	}

	if (!OtherActor || OtherActor == GetOwner())
	{
		return;
	}

	OnEnter.Broadcast(OtherActor);
}

void UInteractTriggerComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AFPSBaseCharacter* Character = Cast<AFPSBaseCharacter>(OtherActor);
	if (Character && Character->IsPlayerControlled())
	{
		const bool bWasActiveTrigger =
			Character->GetCurrentInteractableActor() == GetOwner() &&
			Character->GetCurrentTruckInteractType() == InteractType;

		if (bWasActiveTrigger)
		{
			Character->SetInteractableActor(nullptr);
			Character->SetCurrentTruckInteractType(ETruckInteractType::None);

			if (ATruck* Truck = Cast<ATruck>(GetOwner()))
			{
				Truck->RefreshInteractionWidgetsForCharacter(Character);
			}
		}
	}

	if (!OtherActor || OtherActor == GetOwner())
	{
		return;
	}

	OnExit.Broadcast(OtherActor);
}
