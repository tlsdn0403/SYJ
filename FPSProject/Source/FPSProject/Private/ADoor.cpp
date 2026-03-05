// Fill out your copyright notice in the Description page of Project Settings.


#include "ADoor.h"
#include "HUD/InteractUIClass.h"
#include "Components/WidgetComponent.h"
#include "Components/InteractTriggerComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Materials/MaterialInterface.h"

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

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	WidgetComp->SetupAttachment(DoorMeshComp);
	WidgetComp->SetTwoSided(true);
	WidgetComp->SetWidgetSpace(EWidgetSpace::World);

	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(SceneRoot);
	InteractTrigger->InitSphereRadius(200.0f); // 범위 설정

}

// Called when the game starts or when spawned
void AADoor::BeginPlay()
{
	Super::BeginPlay();

	//UE_LOG(LogTemp, Warning, TEXT("WidgetComp valid: %d"), IsValid(WidgetComp));

	//if (WidgetComp)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("WidgetClass: %s"),
	//		*GetNameSafe(WidgetComp->GetWidgetClass()));

	//	UUserWidget* Obj = WidgetComp->GetUserWidgetObject();
	//	UE_LOG(LogTemp, Warning, TEXT("UserWidgetObject: %s"), *GetNameSafe(Obj));

	//	// 강제로 생성/갱신 (중요)
	//	WidgetComp->InitWidget();
	//	Obj = WidgetComp->GetUserWidgetObject();
	//	UE_LOG(LogTemp, Warning, TEXT("After InitWidget UserWidgetObject: %s"), *GetNameSafe(Obj));
	//}

	OriginalRotation = DoorMeshComp->GetRelativeRotation(); //문이 처음에 어떤 방향을 향하고 있는지 저장
	Target = OriginalRotation; // 처음 목표도 원래 각도
	//델리게이트 바인딩 (트리거 컴포넌트의 OnEnter 이벤트를 듣도록 설정)
	InteractTrigger->OnEnter.AddDynamic(this, &AADoor::WidgetStart);
	InteractTrigger->OnExit.AddDynamic(this, &AADoor::WidgetEnd);

	
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


void AADoor::WidgetStart(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor)) return;
	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->PlayAni_PopUp(); // 위젯 클래스에 만든 함수
	}

	if (DoorMeshComp && OverlayMaterial)
	{
		DoorMeshComp->SetOverlayMaterial(OverlayMaterial);
	}
}

void AADoor::WidgetEnd(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor)) return;

	if (DoorMeshComp && OverlayMaterial)
	{
		DoorMeshComp->SetOverlayMaterial(nullptr);
	}
}