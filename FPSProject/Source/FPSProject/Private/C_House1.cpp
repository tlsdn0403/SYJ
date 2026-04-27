#include "C_House1.h"
#include "ADoor.h"

AC_House1::AC_House1()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	HISM_Floor = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Floor"));
	HISM_Floor->SetupAttachment(Root);

	HISM_Pillar = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Pillar"));
	HISM_Pillar->SetupAttachment(Root);

	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wall_%d"), i));
		Wall->SetupAttachment(Root);
		WallComponents.Add(Wall);
	}

	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Roof = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Roof_%d"), i));
		Roof->SetupAttachment(Root);
		RoofComponents.Add(Roof);
	}

	for (int32 i = 0; i < 3; ++i)
	{
		UStaticMeshComponent* Etc = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Etc_%d"), i));
		Etc->SetupAttachment(Root);
		EtcComponents.Add(Etc);
	}
}

void AC_House1::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	HISM_Floor->ClearInstances();

	if (!HISM_Floor->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("HISM_Floor StaticMesh is NULL"));
		return;
	}

	TArray<FTransform> Instances;
	Instances.Reserve(6);

	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 2; ++j)
		{
			Instances.Add(FTransform(FVector(i * 400.f, j * 400.f, 0.f)));
		}
	}
	HISM_Floor->AddInstances(Instances, false);

	Instances.Empty();

	for (const FTransform& Offset : PillarOffsets)
	{
		Instances.Add(Offset);
	}
	HISM_Pillar->AddInstances(Instances, false);

	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents<UChildActorComponent>(ChildActorComponents);

	ChildActorComponents.Sort([](const UChildActorComponent& A, const UChildActorComponent& B)
	{
		return A.GetName() < B.GetName();
	});

	int32 DoorIndex = 0;
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (ChildActorComponent == nullptr)
		{
			continue;
		}

		if (AADoor* Door = Cast<AADoor>(ChildActorComponent->GetChildActor()))
		{
			Door->NetworkDoorId = DoorNetworkIds.IsValidIndex(DoorIndex) ? DoorNetworkIds[DoorIndex] : 0;
			++DoorIndex;
		}
	}
}