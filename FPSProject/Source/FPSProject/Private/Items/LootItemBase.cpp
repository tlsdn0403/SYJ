// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/LootItemBase.h"
#include "Characters/FPSBaseCharacter.h"
#include "Engine/Engine.h" // GEngine 사용을 위한 헤더

// Sets default values
ALootItemBase::ALootItemBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(RootComponent);
	InteractTrigger->InitSphereRadius(20.0f); // 범위 설정
}

// Called when the game starts or when spawned
void ALootItemBase::BeginPlay()
{
	Super::BeginPlay();
    GEngine->AddOnScreenDebugMessage(   //한글 출력되는지 확인용 코드
        -1,         // 메시지 ID (-1이면 계속 새로 생성)
        5.f,        // 화면에 표시 시간 (초)
        FColor::Yellow,
		ItemName.ToString()
    );
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
            Destroy();
        }
        else
        {
            // 인벤토리가 꽉 찼을 때 로직
			UE_LOG(LogTemp, Warning, TEXT("Cannot pick up item: Inventory is full."));
        }
    }
}

