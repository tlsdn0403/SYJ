// Fill out your copyright notice in the Description page of Project Settings.


#include "ADoor.h"
#include "Components/WidgetComponent.h"
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

	InteractWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	InteractWidget->SetupAttachment(SceneRoot);

	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(SceneRoot);
	InteractTrigger->InitSphereRadius(200.0f); // 범위 설정

}

// Called when the game starts or when spawned
void AADoor::BeginPlay()
{
	Super::BeginPlay();
	OriginalRotation = DoorMeshComp->GetRelativeRotation(); //문이 처음에 어떤 방향을 향하고 있는지 저장
	Target = OriginalRotation; // 처음 목표도 원래 각도
}

// Called every frame
void AADoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FRotator Current = DoorMeshComp->GetRelativeRotation();

	FRotator NewRot = FMath::RInterpTo(Current, Target, DeltaTime, MoveTime);
	DoorMeshComp->SetRelativeRotation(NewRot);
}

void AADoor::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (!Character) return;

	bOpen = !bOpen;

	if (bOpen)
	{
		Target = OriginalRotation + MoveDir;
	}
	else
	{
		Target = OriginalRotation;
	}
}
