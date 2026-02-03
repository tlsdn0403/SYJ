// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "C_HouseBase.generated.h"

UCLASS()
class FPSPROJECT_API AC_HouseBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AC_HouseBase();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* HISM_Floor;

	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* HISM_Pillar;

	UPROPERTY(EditAnywhere, Category = "MySettings")
	TArray<FTransform> PillarOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySettings")
	int32 Fwidth = 3;	//¹Ù´Ú °¡·ÎÄ­¼ö
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySettings")
	int32 Flength = 1;	//¹Ù´Ú ¼¼·ÎÄ­¼ö
};
