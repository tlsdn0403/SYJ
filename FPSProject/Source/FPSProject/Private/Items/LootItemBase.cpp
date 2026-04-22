// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/LootItemBase.h"
#include "HUD/InteractUIClass.h"
#include "Components/WidgetComponent.h"
#include "Components/InteractTriggerComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Characters/FPSPlayerController.h"
#include "Materials/MaterialInterface.h"

// Sets default values
ALootItemBase::ALootItemBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(SceneRoot);

    WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
    WidgetComp->SetupAttachment(MeshComp);
    WidgetComp->SetTwoSided(true);
    WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);

	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(RootComponent);
	InteractTrigger->InitSphereRadius(20.0f); // 범위 설정
}

// Called when the game starts or when spawned
void ALootItemBase::BeginPlay()
{
	Super::BeginPlay();
    if (WidgetComp) WidgetComp->InitWidget();

    InteractTrigger->OnEnter.AddDynamic(this, &ALootItemBase::WidgetStart);
    InteractTrigger->OnExit.AddDynamic(this, &ALootItemBase::WidgetEnd);
	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->SetInteractText(setText);
	}

}

// Called every frame
void ALootItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ALootItemBase::Interact_Implementation(AFPSBaseCharacter* Character)
{
    if (Character)
    {
        // 캐릭터 인벤토리에 추가 시도
        if (Character->AddItem(this->ItemType))
        {
            // 성공하면 아이템 삭제
			if (AFPSPlayerController* PC = Character->GetController<AFPSPlayerController>())
			{
				PC->PickUp_Item(itemimage);
			}
            Destroy();
        }
        else
        {
            // 인벤토리가 꽉 찼을 때 로직
			UE_LOG(LogTemp, Warning, TEXT("Cannot pick up item: Inventory is full."));
        }

    }
}


void ALootItemBase::WidgetStart(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor)) return;
	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->PlayAni_PopUp(true); // 위젯 클래스에 만든 함수

	}

	if (MeshComp && OverlayMaterial)
	{
		MeshComp->SetOverlayMaterial(OverlayMaterial);
	}
}

void ALootItemBase::WidgetEnd(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor)) return;
	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->RePlayAni_PopUp(); // 위젯 클래스에 만든 함수

	}
	if (MeshComp && OverlayMaterial)
	{
		MeshComp->SetOverlayMaterial(nullptr);
	}
}