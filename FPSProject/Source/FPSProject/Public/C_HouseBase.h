// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ChildActorComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "C_HouseBase.generated.h"

UCLASS()
class FPSPROJECT_API AC_HouseBase : public AActor
{
	GENERATED_BODY()

public:
	AC_HouseBase();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	TArray<int32> DoorNetworkIds;

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* HISM_Floor;

	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* HISM_Pillar;

	UPROPERTY(EditAnywhere, Category = "MySettings")
	TArray<FTransform> PillarOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySettings")
	int32 Fwidth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySettings")
	int32 Flength = 1;
};