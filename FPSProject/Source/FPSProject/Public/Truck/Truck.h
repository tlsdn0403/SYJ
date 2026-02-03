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

	void BeginPlay();

	// 상호작용 범위 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* InteractTrigger;

	// 인터페이스 함수 오버라이드 (F키 눌렀을 때 실행될 내용)
	virtual void Interact_Implementation(class AFPSBaseCharacter* Character) override;

	void UpdateEngineSound();
	void UpdateBrakeSound();
protected:
	virtual void Tick(float DeltaTime) override; // RPM 체크를 위해 필요
	//트럭 이동을 위한 것.
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Brake(float Value);

	//-------------------------------트럭 아이템 적재용 --------------------------------
	// 트럭에 적재된 총 아이템 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameLogic")
	int32 TotalLoadedItems = 0;

	// 스테이지 구별용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameLogic")
	bool bIsLoadingPhase = true;

	// -------------------------------------------------------------------------------------


	// -------------------------------사운드 관련 컴포넌트 및 변수들--------------------------------
	
	// 엔진 소리 재생용 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAudioComponent* EngineAudioComponent;

	// 엔진 사운드 큐 
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* EngineSoundCue;

	//브레이크 소리 (Car_Brakes_Squeal_01)
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* BrakeSound;

private:
	// 브레이크 소리가 중복 재생되지 않도록 막는 플래그
	bool bIsBrakingSoundPlaying = false;
};
