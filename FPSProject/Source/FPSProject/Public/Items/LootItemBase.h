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
	TT UMETA(DisplayName = "Heal Pack")
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
	int32 HandWeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	bool bRespawnOnPickup = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0"))
	float RespawnDelay = 20.0f;

	void ConfigureNetworkItem(uint64 InNetworkItemId, bool bInRespawnOnPickup, float InRespawnDelay);

	void SetNetworkItemActive(bool bIsActive);

	uint64 GetNetworkItemId() const { return NetworkItemId; }

	bool ShouldRespawnOnPickup() const { return bRespawnOnPickup; }

	float GetRespawnDelay() const { return RespawnDelay; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

public:
	virtual void Interact_Implementation(AFPSBaseCharacter* Character) override;

	UFUNCTION()
	void WidgetStart(AActor* OtherActor);

	UFUNCTION()
	void WidgetEnd(AActor* OtherActor);

private:
	uint64 ResolveDefaultNetworkItemId() const;

	UPROPERTY(Transient)
	uint64 NetworkItemId = 0;

	UPROPERTY(Transient)
	bool bPickupPending = false;
};
