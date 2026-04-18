#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "PickUpWeaponComponent.generated.h"

// 캐릭터 클래스(수정 필요)
class AFPSBaseCharacter;

// 무기 픽업 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponPickUp, AFPSBaseCharacter*, PickUpCharacter);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPSPROJECT_API UPickUpWeaponComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UPickUpWeaponComponent();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// 블루프린트에서 바인딩 가능한 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnWeaponPickUp OnPickUp;
};