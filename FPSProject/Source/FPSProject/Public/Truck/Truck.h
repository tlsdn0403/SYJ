#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Components/InteractTriggerComponent.h"
#include "Interface/InteractInterface.h"
#include "Items/LootItemBase.h"
#include "Components/AudioComponent.h"
#include "Truck.generated.h"

class AFPSBaseCharacter;
class AActor;
class ABaseZombie;
class AMountedMachineGun;
class UAIPerceptionStimuliSourceComponent;
class UHealthComponent;
class UBoxComponent;
class USceneComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USoundBase;
class UWidgetComponent;
class AStage2TileManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTruckHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTruckDestroyed);

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

	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, Category = "Network")
	uint64 NetworkTruckId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* DriverSeatInteractTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* CargoSeatInteractTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* TurretSeatInteractTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UWidgetComponent* DriverSeatInteractWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UWidgetComponent* CargoSeatInteractWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHealthComponent* HealthComponent;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnTruckHealthChanged OnTruckHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnTruckDestroyed OnTruckDestroyed;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetTruckHealth() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetTruckMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsTruckDestroyed() const { return bTruckDestroyed; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Driver")
	USceneComponent* DriverSeatPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Driver")
	USceneComponent* DriverExitPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Ride")
	USceneComponent* CargoRidePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Ride")
	USceneComponent* CargoExitPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	USceneComponent* TurretSeatPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	USceneComponent* TurretMountPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	USceneComponent* TurretCameraPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	UWidgetComponent* TurretInteractWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Ride")
	UBoxComponent* CargoMoveBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Collision")
	UBoxComponent* CargoFloorCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Collision")
	UBoxComponent* CargoLeftWallCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Collision")
	UBoxComponent* CargoRightWallCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Collision")
	UBoxComponent* CargoFrontWallCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Collision")
	UBoxComponent* CargoBackWallCollision;

	virtual void Interact_Implementation(class AFPSBaseCharacter* Character) override;

	void UpdateEngineSound();
	void UpdateBrakeSound();

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FVector GetCargoRideLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FRotator GetCargoRideRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Driver")
	FVector GetDriverSeatLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Driver")
	FRotator GetDriverSeatRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Driver")
	FVector GetDriverExitLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Driver")
	FRotator GetUprightExitRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FVector GetCargoExitLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Turret")
	FVector GetTurretSeatLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Turret")
	FRotator GetTurretSeatRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Turret")
	FVector GetTurretCameraLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Turret")
	FRotator GetTurretCameraRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	FBox GetCargoWorldBounds() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	UPrimitiveComponent* GetCargoMovementBase() const;

	UFUNCTION(BlueprintCallable, Category = "Cargo|Ride")
	UBoxComponent* GetCargoMoveBoundsComponent() const { return CargoMoveBounds; }

	UFUNCTION(BlueprintCallable, Category = "Turret")
	AMountedMachineGun* GetMountedWeapon() const { return MountedWeapon; }

	void SetDriverCharacter(AFPSBaseCharacter* Character) { DriverCharacter = Character; }
	AFPSBaseCharacter* GetDriverCharacter() const { return DriverCharacter; }
	void SetMountedWeaponUser(AFPSBaseCharacter* Character) { MountedWeaponUser = Character; }
	AFPSBaseCharacter* GetMountedWeaponUser() const { return MountedWeaponUser; }
	void SetLocallyDriven(bool bLocallyDriven);
	bool IsLocallyDriven() const { return bIsLocallyDriven; }
	void ApplyNetworkTransform(const FVector& TargetLocation, const FRotator& TargetRotation, bool bForceCorrection = false);

	UFUNCTION(BlueprintCallable, Category = "GameLogic")
	void SetLoadingPhase(bool bLoadingPhase);

	UFUNCTION(BlueprintPure, Category = "GameLogic")
	bool IsLoadingPhase() const { return bIsLoadingPhase; }

	void ApplyLoadedCargoItem(EItemType ItemType);

	UFUNCTION(BlueprintCallable, Category = "Turret")
	bool TryEnterMountedWeapon(AFPSBaseCharacter* Character);

	void EndMountedWeaponUse(AFPSBaseCharacter* Character);
	void RefreshInteractionWidgetsForCharacter(AFPSBaseCharacter* Character);

	UFUNCTION()
	void OnDriverInteractEnter(AActor* OtherActor);

	UFUNCTION()
	void OnDriverInteractExit(AActor* OtherActor);

	UFUNCTION()
	void OnCargoInteractEnter(AActor* OtherActor);

	UFUNCTION()
	void OnCargoInteractExit(AActor* OtherActor);

	UFUNCTION()
	void OnTurretInteractEnter(AActor* OtherActor);

	UFUNCTION()
	void OnTurretInteractExit(AActor* OtherActor);

	UFUNCTION()
	void ExitDriverSeat();

	UFUNCTION()
	void OnTruckMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Brake(float Value);
	void UseDriverHealPack();
	void SendTruckMovePacket();
	void CheckZombieImpactSweep();
	void ProcessZombieImpact(ABaseZombie* Zombie, const FVector& ImpactPoint, const FVector& ImpactDirection, float ImpactSpeed);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameLogic")
	int32 TotalLoadedItems = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameLogic")
	bool bIsLoadingPhase = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo")
	USceneComponent* CargoOrigin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> AmmoSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> MountedAmmoSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> FuelSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> MedKitSlots;

	int32 CurrentAmmoCount = 0;
	int32 CurrentMountedAmmoCount = 0;
	int32 CurrentFuelCount = 0;
	int32 CurrentMedKitCount = 0;

	void AddCargoVisual(EItemType ItemType);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAudioComponent* EngineAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAIPerceptionStimuliSourceComponent* ZombieStimuliSource;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* EngineSoundCue;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* BrakeSound;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* LoadItemSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AMountedMachineGun> MountedWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret", meta = (AllowPrivateAccess = "true"))
	FTransform MountedWeaponRelativeTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret", meta = (AllowPrivateAccess = "true"))
	float MountedWeaponUseDistance = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactMinSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactFatalSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombiePinnedImpactFatalSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactMinDamage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactMaxDamage = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactKnockback = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactUpwardKnockback = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactImpulse = 260000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactCooldown = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Zombie")
	float ZombieImpactContactTolerance = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "1.0"))
	float TruckMaxHealth = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float BrakeSoundMinSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float ZombieNoiseMinSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float ZombieNoiseMaxSpeed = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float ZombieNoiseRange = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float ZombieNoiseInterval = 0.35f;
private:
	bool bIsLocallyDriven = false;
	bool bIsBrakingSoundPlaying = false;
	bool bBrakePressedLastFrame = false;
	bool bTruckDestroyed = false;
	float TruckMovePacketSendTimer = 0.0f;
	float DebugTransformLogTimer = 0.0f;
	float ZombieNoiseTimer = 0.0f;
	static constexpr float TRUCK_MOVE_PACKET_SEND_DELAY = 0.05f;

	void ReportZombieAwarenessNoise(float DeltaTime);
	bool IsLocalInteractionCharacter(const AFPSBaseCharacter* Character) const;

	UFUNCTION()
	void HandleTruckHealthChanged(float NewHealth, float Damage);

	UPROPERTY()
	AMountedMachineGun* MountedWeapon = nullptr;

	UPROPERTY()
	AFPSBaseCharacter* MountedWeaponUser = nullptr;

	UPROPERTY()
	AFPSBaseCharacter* DriverCharacter = nullptr;

	TMap<TObjectPtr<ABaseZombie>, float> LastZombieImpactTimes;
};
