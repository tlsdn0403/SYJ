// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InteractTriggerComponent.h"
#include "Interface/InteractInterface.h"
#include "Engine/Texture2D.h"
#include "LootItemBase.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class AFPSBaseCharacter;

UENUM(BlueprintType)
enum class EItemType : uint8
{
	None UMETA(DisplayName = "None"),
	Ammo UMETA(DisplayName = "Ammo"),
	Fuel UMETA(DisplayName = "Fuel"),
	MedicalKit UMETA(DisplayName = "Medical Kit"),
	CharacterAmmo UMETA(DisplayName = "Character Ammo"),
	MountedGunAmmo UMETA(DisplayName = "Mounted Gun Ammo"),
	TruckRepairKit UMETA(DisplayName = "Truck Repair Kit"),
	HealPack UMETA(DisplayName = "Heal Pack"),
	TT UMETA(DisplayName = "Heal Pack")			// 빈 값 채워주는 용. 무게3이면 2칸은 얘로.
};

UCLASS()
class FPSPROJECT_API ALootItemBase : public AActor, public IInteractInterface
{
	GENERATED_BODY()

public:
	ALootItemBase();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* InteractTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Properties")
	EItemType ItemType = EItemType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UWidgetComponent* WidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UMaterialInterface* OverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText setText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UTexture2D* itemimage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 HandWeight =1;		// 손 몇개 차지하나 , 기본세팅 1

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AFPSBaseCharacter* Character) override;

	UFUNCTION()
	void WidgetStart(AActor* OtherActor);

	UFUNCTION()
	void WidgetEnd(AActor* OtherActor);
};
