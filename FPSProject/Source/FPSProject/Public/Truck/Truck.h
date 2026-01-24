// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Components/InteractTriggerComponent.h"
#include "Interface/InteractInterface.h"
#include "Items/LootItemBase.h"
#include "Components/AudioComponent.h"
#include "Truck.generated.h"

/**
 * 
 */


UCLASS()
class FPSPROJECT_API ATruck : public AWheeledVehiclePawn, public IInteractInterface
{
	GENERATED_BODY()

public:
	ATruck();

	// 상호작용 범위 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* InteractTrigger;

	// 인터페이스 함수 오버라이드 (F키 눌렀을 때 실행될 내용)
	virtual void Interact_Implementation(class AFPSBaseCharacter* Character) override;

protected:
	//트럭 이동을 위한 것.
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Brake(float Value);

	// 트럭에 적재된 총 아이템 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameLogic")
	int32 TotalLoadedItems = 0;

	// 스테이지 구별용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameLogic")
	bool bIsLoadingPhase = true;
};
