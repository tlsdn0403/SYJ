// Fill out your copyright notice in the Description page of Project Settings.


#include "ADoor.h"
#include "Components/InteractTriggerComponent.h"

// Sets default values
AADoor::AADoor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//PrimaryActorTick.bCanEverTick = true;
	// Scene Component를 생성하고 루트로 설정
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Static Mesh Component를 생성하고 Scene Component에 Attach
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetupAttachment(SceneRoot);

	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(SceneRoot);
	InteractTrigger->InitSphereRadius(200.0f); // 범위 설정

    //상호작용 문 생성중. 범위설정 저거 박스로 변경가능한지 파악하기.
}

// Called when the game starts or when spawned
//void AADoor::BeginPlay()
//{
//	Super::BeginPlay();
//	
//}

// Called every frame
//void AADoor::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//
//}

void AADoor::Interact_Implementation(AFPSBaseCharacter* Character)
{
    if (Character)
    {
        //// 캐릭터 인벤토리에 추가 시도
        //if (Character->AddItem(this->ItemType))
        //{
        //    // 성공하면 아이템 삭제
        //    Destroy();
        //}
        //else
        //{
        //    // 인벤토리가 꽉 찼을 때 로직
        //    UE_LOG(LogTemp, Warning, TEXT("Cannot pick up item: Inventory is full."));
        //}
    }
}



