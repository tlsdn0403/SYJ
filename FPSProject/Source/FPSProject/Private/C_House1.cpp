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

	HISM_Pillar = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Pillar"));
	HISM_Pillar->SetupAttachment(Root);

	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* Wall =
			CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wall_%d"), i));

		Wall->SetupAttachment(Root);
		WallComponents.Add(Wall);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Roof =
			CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Roof_%d"), i));

		Roof->SetupAttachment(Root);
		RoofComponents.Add(Roof);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		UStaticMeshComponent* Etc =
			CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Etc_%d"), i));

		Etc->SetupAttachment(Root);
		EtcComponents.Add(Etc);
	}
}

//BeginPlay()에서 하면 실행시에만 보이고 에디터상에서는 안보이기 때문에 OnConstruction()에서 처리
void AC_House1::OnConstruction(const FTransform& Transform)		
{
	Super::OnConstruction(Transform);

	HISM_Floor->ClearInstances();

	if (!HISM_Floor->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("HISM_Floor StaticMesh is NULL"));
		return;
	}
	// AddInstance 한 번마다 렌더 상태 갱신, 트리 재계산 등의 오버헤드가 발생하므로 여러번 호출은 비효율적.
	// 따라서 여러 인스턴스를 추가할 때는 미리 ClearInstances()로 비우고 한꺼번에 추가하는 것이 효율적.
	//근데 6개면 걍 거기서 거기래. 

	TArray<FTransform> Instances;
	Instances.Reserve(6);	//미리 메모리 할당

	for (int i = 0; i < 3; ++i)
	{
		for(int j = 0; j < 2; ++j)
		{
			Instances.Add( FTransform( FVector(i * 400.f, j * 400.f, 0.f)));
		}
	}
	HISM_Floor->AddInstances(Instances, false);

}
