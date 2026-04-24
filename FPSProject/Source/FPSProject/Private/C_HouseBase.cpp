// Fill out your copyright notice in the Description page of Project Settings.

#include "C_HouseBase.h"
#include "ADoor.h"

AC_HouseBase::AC_HouseBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	HISM_Floor = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Floor"));
	HISM_Floor->SetupAttachment(Root);

	HISM_Pillar = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Pillar"));
	HISM_Pillar->SetupAttachment(Root);
}

void AC_HouseBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!HISM_Floor->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("HISM_Floor StaticMesh is NULL"));
		return;
	}

	TArray<FTransform> Instances;
	Instances.Reserve(Fwidth * Flength);

	for (int32 i = 0; i < Flength; ++i)
	{
		for (int32 j = 0; j < Fwidth; ++j)
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