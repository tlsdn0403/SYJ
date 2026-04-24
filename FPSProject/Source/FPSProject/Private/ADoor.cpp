#include "ADoor.h"
#include "HUD/InteractUIClass.h"
#include "Components/WidgetComponent.h"
#include "Components/InteractTriggerComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Materials/MaterialInterface.h"
#include "FPSProjectGameInstance.h"
#include "ClientPacketHandler.h"
#include "Protocol.pb.h"

AADoor::AADoor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DoorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	DoorMeshComp->SetupAttachment(SceneRoot);

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	WidgetComp->SetupAttachment(DoorMeshComp);
	WidgetComp->SetTwoSided(true);
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);

	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(SceneRoot);
	InteractTrigger->InitSphereRadius(200.0f);
}

void AADoor::BeginPlay()
{
	Super::BeginPlay();

	if (WidgetComp)
	{
		WidgetComp->InitWidget();
	}

	OriginalRotation = DoorMeshComp->GetRelativeRotation();
	Target = OriginalRotation;

	InteractTrigger->OnEnter.AddDynamic(this, &AADoor::WidgetStart);
	InteractTrigger->OnExit.AddDynamic(this, &AADoor::WidgetEnd);

	ApplyDoorState(bOpen);
}

void AADoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const FRotator Current = DoorMeshComp->GetRelativeRotation();
	const FRotator NewRot = FMath::RInterpTo(Current, Target, DeltaTime, MoveTime);
	DoorMeshComp->SetRelativeRotation(NewRot);
}

void AADoor::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (Character == nullptr)
	{
		return;
	}

	if (UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(Character->GetGameInstance()))
	{
		if (GameInstance->ShouldUseLocalInteractionFallback() || NetworkDoorId == 0)
		{
			ApplyDoorState(!bOpen);
			return;
		}
	}

	Protocol::C_TOGGLE_DOOR ToggleDoorPkt;
	ToggleDoorPkt.set_door_id(NetworkDoorId);
	SEND_PACKET(ToggleDoorPkt);
}

void AADoor::ApplyDoorState(bool bShouldOpen)
{
	bOpen = bShouldOpen;
	Target = bOpen ? (OriginalRotation + MoveDir) : OriginalRotation;

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->SetDoorInteractText(bOpen);
	}
}

void AADoor::WidgetStart(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor))
	{
		return;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->PlayAni_PopUp(bOpen);
	}

	if (DoorMeshComp && OverlayMaterial)
	{
		DoorMeshComp->SetOverlayMaterial(OverlayMaterial);
	}
}

void AADoor::WidgetEnd(AActor* OtherActor)
{
	if (!Cast<AFPSBaseCharacter>(OtherActor))
	{
		return;
	}

	if (UInteractUIClass* UI = Cast<UInteractUIClass>(WidgetComp->GetUserWidgetObject()))
	{
		UI->RePlayAni_PopUp();
	}

	if (DoorMeshComp && OverlayMaterial)
	{
		DoorMeshComp->SetOverlayMaterial(nullptr);
	}
}