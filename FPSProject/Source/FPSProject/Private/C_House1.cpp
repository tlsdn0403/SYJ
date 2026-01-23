// Fill out your copyright notice in the Description page of Project Settings.


#include "C_House1.h"

// Sets default values
AC_House1::AC_House1()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;	//틱 안쓸거라 false로 설정

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	HISM_Floor = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Floor"));
	HISM_Floor->SetupAttachment(Root);

}

// Called when the game starts or when spawned
//void AC_House1::BeginPlay()
//{
//	Super::BeginPlay();
//
//
//	
//	if (HISM_Floor->GetStaticMesh()) {
//		HISM_Floor->AddInstance(FTransform(FVector(0.f, 0.f, 0.f)));
//	}
//
//
//}

//BeginPlay()에서 하면 실행시에만 보이고 에디터상에서는 안보이기 때문에 OnConstruction()에서 처리
void AC_House1::OnConstruction(const FTransform& Transform)		
{
	Super::OnConstruction(Transform);

	HISM_Floor->ClearInstances();

	if (HISM_Floor->GetStaticMesh())
	{
		HISM_Floor->AddInstance(FTransform(FVector(0.0f,0.0f,1.0f)));
	}
}
