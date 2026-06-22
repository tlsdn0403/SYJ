// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "InteractTriggerComponent.generated.h"

UENUM(BlueprintType)
enum class ETruckInteractType : uint8
{
	None,
	LoadCargo,
	DriverSeat,
	CargoSeat,
	TurretSeat
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractEnter, AActor*, OtherActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractExit, AActor*, OtherActor);

UCLASS()
class FPSPROJECT_API UInteractTriggerComponent : public USphereComponent
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	bool IsAvailableForCharacter(const class AFPSBaseCharacter* Character) const;

	UPROPERTY(BlueprintAssignable)
	FOnInteractEnter OnEnter;

	UPROPERTY(BlueprintAssignable)
	FOnInteractExit OnExit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ETruckInteractType InteractType = ETruckInteractType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bRequiresTruckCargo = false;
};
