// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/InteractTriggerComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "GameFramework/Actor.h"

void UInteractTriggerComponent::BeginPlay()
{
	Super::BeginPlay();
	// 겹침 이벤트 바인딩
	OnComponentBeginOverlap.AddDynamic(this, &UInteractTriggerComponent::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &UInteractTriggerComponent::OnOverlapEnd);
}

void UInteractTriggerComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AFPSBaseCharacter* Character = Cast<AFPSBaseCharacter>(OtherActor);
	if (Character && Character->IsPlayerControlled())
	{
		// 캐릭터에게 나랑 상호작용 가능하다고 알림
		Character->SetInteractableActor(GetOwner());

		if(Character->GetCurrentTruckInteractType())
		// 인터렉트 컴포넌트 타입
		Character->SetCurrentTruckInteractType(InteractType);
	}
	if (!OtherActor || OtherActor == GetOwner()) return;

	// 델리게이트 이벤트 발생 (문이 AddDynamic으로 듣고 있어야 함)
	OnEnter.Broadcast(OtherActor);
}

void UInteractTriggerComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AFPSBaseCharacter* Character = Cast<AFPSBaseCharacter>(OtherActor);
	if (Character && Character->IsPlayerControlled())
	{
		// 범위 밖으로 나가면 상호작용 대상 해제
		if (Character->GetCurrentInteractableActor() == GetOwner())
		{
			Character->SetInteractableActor(nullptr);
		}
		if (!OtherActor || OtherActor == GetOwner()) return;

		// 델리게이트 이벤트 발생
		OnExit.Broadcast(OtherActor);
	}
}
