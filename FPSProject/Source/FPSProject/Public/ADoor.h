#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractInterface.h"
#include "ADoor.generated.h"

class UWidgetComponent;
class UInteractTriggerComponent;
class AFPSBaseCharacter;
class UMaterialInterface;

UCLASS()
class FPSPROJECT_API AADoor : public AActor, public IInteractInterface
{
	GENERATED_BODY()

public:
	AADoor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	int32 NetworkDoorId = 0;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* InteractTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UWidgetComponent* WidgetComp;

	virtual void Interact_Implementation(AFPSBaseCharacter* Character) override;
	void ApplyDoorState(bool bShouldOpen);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	bool bOpen = false;

	FRotator OriginalRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	FRotator Target = FRotator(0.f, 90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	FRotator MoveDir = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	float MoveTime = 3.f;

	UFUNCTION()
	void WidgetStart(AActor* OtherActor);

	UFUNCTION()
	void WidgetEnd(AActor* OtherActor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	UMaterialInterface* OverlayMaterial;
};