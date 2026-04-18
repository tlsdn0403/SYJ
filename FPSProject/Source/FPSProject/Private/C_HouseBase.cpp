// Fill out your copyright notice in the Description page of Project Settings.


#include "C_HouseBase.h"

// Sets default values
AC_HouseBase::AC_HouseBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
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
	Instances.Reserve(Fwidth*Flength);	//미리 메모리 할당

	for (int i = 0; i < Flength; ++i)
	{
		for (int j = 0; j < Fwidth; ++j)
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
}