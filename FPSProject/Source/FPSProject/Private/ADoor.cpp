// Fill out your copyright notice in the Description page of Project Settings.


#include "ADoor.h"
#include "Components/InteractTriggerComponent.h"

// Sets default values
AADoor::AADoor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// Scene Component를 생성하고 루트로 설정
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Static Mesh Component를 생성하고 Scene Component에 Attach
	DoorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	DoorMeshComp->SetupAttachment(SceneRoot);

	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(SceneRoot);
	InteractTrigger->InitSphereRadius(200.0f); // 범위 설정

	//상호작용 문 생성중. 범위설정 저거 박스로 변경가능한지 파악하기.

}

// Called when the game starts or when spawned
void AADoor::BeginPlay()
{
	Super::BeginPlay();
	OriginalRotation = GetActorRotation(); //문이 처음에 어떤 방향을 향하고 있는지 저장
}

// Called every frame
void AADoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bOpen)//문 열기
	{
		//OpenDoor();
		FRotator Current = DoorMeshComp->GetRelativeRotation();

		FRotator NewRot = FMath::RInterpTo(Current, Target, DeltaTime, 3.f);
		DoorMeshComp->SetRelativeRotation(NewRot);
	}
	else //문 닫기
	{

	}
}

void AADoor::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (Character)
	{
	bOpen = !bOpen; //문 상태 토글
		if (bOpen)//문 열기
		{
			Target = FRotator(0.f, 90.f, 0.f);
		}
		else //문 닫기
		{
			Target = FRotator(0.f, 90.f, 0.f);
		}
	}
}
