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
class UNiagaraComponent;
class USceneComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USoundBase;
class UWidgetComponent;
class AStage2TileManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTruckHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTruckFuelChanged, float, CurrentFuel, float, MaxFuel);
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

	virtual void OnConstruction(const FTransform& Transform) override;
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

	UPROPERTY(BlueprintAssignable, Category = "Fuel")
	FOnTruckFuelChanged OnTruckFuelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnTruckDestroyed OnTruckDestroyed;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetTruckHealth() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetTruckMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RepairTruck(float RepairAmount);

	void ApplyNetworkHealth(float CurrentHealth, float MaxHealth);
	void ResetVehiclePhysicsState(bool bReleaseBrake);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsTruckDestroyed() const { return bTruckDestroyed; }

	UFUNCTION(BlueprintPure, Category = "Fuel")
	float GetTruckFuel() const { return CurrentTruckFuel; }

	UFUNCTION(BlueprintPure, Category = "Fuel")
	float GetTruckMaxFuel() const { return TruckMaxFuel; }

	UFUNCTION(BlueprintPure, Category = "Fuel")
	bool HasTruckFuel() const { return !bUseFuel || CurrentTruckFuel > KINDA_SMALL_NUMBER; }

	UFUNCTION(BlueprintCallable, Category = "Fuel")
	void SetTruckFuel(float NewFuel);

	UFUNCTION(BlueprintCallable, Category = "Fuel")
	void RefuelTruck(float FuelAmount);

	void SyncTruckStateToServer(bool bAllowHealthIncrease = false);

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

	// Query-only blocker used instead of the simulated vehicle mesh for characters and zombies.
	// It prevents pawn contacts from feeding non-deterministic impulses back into Chaos physics.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
	UBoxComponent* VehiclePawnCollision;

	UFUNCTION(BlueprintPure, Category = "Collision")
	UBoxComponent* GetVehiclePawnCollision() const { return VehiclePawnCollision; }

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

	void SetDriverCharacter(AFPSBaseCharacter* Character);
	AFPSBaseCharacter* GetDriverCharacter() const { return DriverCharacter; }
	void SetMountedWeaponUser(AFPSBaseCharacter* Character);
	AFPSBaseCharacter* GetMountedWeaponUser() const { return MountedWeaponUser; }
	void SetLocallyDriven(bool bLocallyDriven);
	bool IsLocallyDriven() const { return bIsLocallyDriven; }
	void SetCinematicControlLocked(bool bLocked);
	bool IsCinematicControlLocked() const { return bCinematicControlLocked; }
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
	void SendTruckMovePacket(bool bAllowHealthIncrease = false);
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
	TArray<UStaticMeshComponent*> RepairKitSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Slots")
	TArray<UStaticMeshComponent*> MedKitSlots;

	int32 CurrentAmmoCount = 0;
	int32 CurrentMountedAmmoCount = 0;
	int32 CurrentFuelCount = 0;
	int32 CurrentRepairKitCount = 0;
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

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* ZombieCrashSound;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bAutoFitVehiclePawnCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (EditCondition = "bAutoFitVehiclePawnCollision"))
	FVector VehiclePawnCollisionPadding = FVector(8.0f, 8.0f, 4.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (ClampMin = "1.0"))
	float TruckMaxHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel")
	bool bUseFuel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel", meta = (ClampMin = "1.0", EditCondition = "bUseFuel"))
	float TruckMaxFuel = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel", meta = (ClampMin = "0.0", EditCondition = "bUseFuel"))
	float TruckStartingFuel = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel", meta = (ClampMin = "0.0", EditCondition = "bUseFuel"))
	float FuelConsumptionPerSecond = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fuel")
	float CurrentTruckFuel = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Stage2", meta = (ClampMin = "1.0"))
	float Stage2EngineTorqueMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float BrakeSoundMinSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke")
	TObjectPtr<UNiagaraComponent> WhiteSmokeComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke")
	TObjectPtr<UNiagaraComponent> BlackSmokeComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke")
	FName WhiteSmokeComponentName = TEXT("Niagara");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke")
	FName BlackSmokeComponentName = TEXT("Niagara1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke", meta = (ClampMin = "0.0"))
	float WhiteSmokeMinSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlackSmokeHealthRatioThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Smoke", meta = (ClampMin = "0.0"))
	float NetworkSmokeSpeedTimeout = 0.25f;

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
	bool bCinematicControlLocked = false;
	bool bApplyingNetworkHealth = false;
	bool bIsBrakingSoundPlaying = false;
	bool bBrakePressedLastFrame = false;
	bool bTruckDestroyed = false;
	float TruckMovePacketSendTimer = 0.0f;
	float DebugTransformLogTimer = 0.0f;
	float ZombieNoiseTimer = 0.0f;
	float CurrentThrottleInput = 0.0f;
	float OriginalEngineMaxTorque = 0.0f;
	FVector LastImpactSweepLocation = FVector::ZeroVector;
	float LastImpactSweepTime = 0.0f;
	FVector LastNetworkSmokeLocation = FVector::ZeroVector;
	float LastNetworkSmokeSampleTime = 0.0f;
	float LastNetworkSmokeUpdateTime = 0.0f;
	float NetworkSmokeSpeed = 0.0f;
	bool bHasOriginalEngineMaxTorque = false;
	bool bHasImpactSweepSample = false;
	bool bHasNetworkSmokeSample = false;
	bool bWhiteSmokeActive = false;
	bool bBlackSmokeActive = false;
	static constexpr float TRUCK_MOVE_PACKET_SEND_DELAY = 0.05f;

	void ApplyStageVehicleTuning();
	void ResolveTruckSmokeComponents();
	void RefreshTruckSmokeEffects();
	void SetSmokeComponentActive(UNiagaraComponent* SmokeComponent, bool bShouldBeActive);
	void UpdateNetworkSmokeSpeedFromTransform(const FVector& TargetLocation);
	float GetTruckSmokeEvaluationSpeed() const;
	void ReportZombieAwarenessNoise(float DeltaTime);
	void UpdateFuelConsumption(float DeltaTime);
	void ClearDrivingInput(bool bHoldBrake);
	void ConfigureVehiclePawnCollision();
	bool IsLocalInteractionCharacter(const AFPSBaseCharacter* Character) const;
	void SetInteractionWidgetsHidden(bool bShouldHide);
	void RefreshLocalInteractionWidgets();

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
