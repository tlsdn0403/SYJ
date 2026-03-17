// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "InteractTriggerComponent.generated.h"


UENUM(BlueprintType)
enum class ETruckInteractType : uint8
{
	None,
	LoadCargo,   // 1스테이지 적재
	DriverSeat,  // 운전석 탑승
	CargoSeat    // 적재함 탑승
};
 
//델리게이트 선언  (상호작용 ui띄우기 위해, 오버랩 시작, 끝에 이벤트 발생)
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
	UPROPERTY(BlueprintAssignable)
	FOnInteractEnter OnEnter;

	UPROPERTY(BlueprintAssignable)
	FOnInteractExit OnExit;

	// 이 트리거가 어떤 탑승 역할인지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ETruckInteractType InteractType = ETruckInteractType::None;
};
