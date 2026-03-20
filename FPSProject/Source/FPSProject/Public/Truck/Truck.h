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


 // 적재된 아이템 시각 정보
USTRUCT(BlueprintType)
struct FLoadedItemVisual
{
	GENERATED_BODY()

	UPROPERTY()
	UStaticMeshComponent* MeshComponent = nullptr;

	UPROPERTY()
	EItemType ItemType = EItemType::None;
};

UCLASS()
class FPSPROJECT_API ATruck : public AWheeledVehiclePawn, public IInteractInterface
{
	GENERATED_BODY()

public:
	ATruck();

	void BeginPlay();

	// --------------------- 상호작용 범위 컴포넌트 -------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* DriverSeatInteractTrigger;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* CargoSeatInteractTrigger;

	// --------------------- 트렁크에 캐릭터 타고 내리는 포지션 --------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Ride")
	USceneComponent* CargoRidePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Ride")
	USceneComponent* CargoExitPoint;
	// 차량에서 움직임 범위
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Ride")
	class UBoxComponent* CargoMoveBounds;

	// 인터페이스 함수 오버라이드 (F키 눌렀을 때 실행될 내용)
	virtual void Interact_Implementation(class AFPSBaseCharacter* Character) override;

	void UpdateEngineSound();
	void UpdateBrakeSound();


	// 트렁크 포지션 게터
	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FVector GetCargoRideLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FRotator GetCargoRideRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FVector GetCargoExitLocation() const;

	// 움직임 범위 계산
	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FBox GetCargoWorldBounds() const;

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


	 // --------- 적재 시각화 시스템 ---------

	//  짐칸 기준점 (트럭 뒷부분, 에디터에서 위치 조정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo")
	USceneComponent* CargoOrigin;
	// 에디터에서 예쁘게 배치해둔 메시 컴포넌트들을 담을 배열
	// 배열의 순서대로 아이템이 채워집니다 (인덱스 0번부터 차례대로 표시됨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> AmmoSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> FuelSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> MedKitSlots;

	// 현재 각 아이템이 몇 개 실렸는지 추적
	int32 CurrentAmmoCount = 0;
	int32 CurrentFuelCount = 0;
	int32 CurrentMedKitCount = 0;



	//  아이템 적재 함수
	void AddCargoVisual(EItemType ItemType);


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


	// 적재 사운드
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* LoadItemSound;
private:
	// 브레이크 소리가 중복 재생되지 않도록 막는 플래그
	bool bIsBrakingSoundPlaying = false;
};
