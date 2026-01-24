// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "C_House1.generated.h"

UCLASS()
class FPSPROJECT_API AC_House1 : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AC_House1();

protected:
	// Called when the game starts or when spawned
	//virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:	

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* HISM_Floor;
	
	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* HISM_Pillar;

	UPROPERTY(EditAnywhere, Category = "Pillar")
	TArray<FTransform> PillarOffsets;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMeshComponent*> WallComponents;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMeshComponent*> RoofComponents;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMeshComponent*> EtcComponents;

};
