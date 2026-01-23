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
void AC_House1::BeginPlay()
{
	Super::BeginPlay();
	
}

