#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/LootItemBase.h"
#include "Components/InteractTriggerComponent.h"
#include "Protocol.pb.h"
#include "FPSBaseCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UAIPerceptionStimuliSourceComponent;
class AWeaponBase;
class AActor;
class USpringArmComponent;
class UHealthComponent;
class AFPSProjectile;
class UInventoryWidget;
class UAnimInstance;
class UAnimationAsset;
class ATruck;
class AMountedMachineGun;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, const TArray<EItemType>&, CurrentInventory);

UCLASS()
class FPSPROJECT_API AFPSBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFPSBaseCharacter();
	virtual ~AFPSBaseCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Stage")
	void TravelToStage2Map();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetCurrentWeapon(AWeaponBase* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void ClearCurrentWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool IsIronSightAiming() const { return bIsIronSightAiming; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void GetWeaponAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void ApplyWeaponRecoil(float PitchKick, float YawKick);

	void Interact();
	void SetInteractableActor(AActor* NewActor);

	UFUNCTION(BlueprintCallable, Category = "FPS|Inventory")
	bool AddItem(EItemType NewItemType);

	UFUNCTION(BlueprintCallable, Category = "FPS|Inventory")
	bool UseHealPack();

	UFUNCTION(BlueprintCallable, Category = "FPS|Inventory")
	TArray<EItemType> OffloadItems();

	UFUNCTION(BlueprintPure, Category = "FPS|Inventory")
	int32 GetItemCount() const { return Inventory.Num(); }

	UPROPERTY(BlueprintAssignable, Category = "FPS|Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }
	AActor* GetCurrentInteractableActor() const { return CurrentInteractableActor; }

	void SetCurrentTruckInteractType(ETruckInteractType NewType) { CurrentTruckInteractType = NewType; }
	ETruckInteractType GetCurrentTruckInteractType() const { return CurrentTruckInteractType; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
	bool bIsOnTruckCargo = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
	ATruck* CurrentTruck = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
	bool bIsDrivingTruck = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
	bool bIsUsingMountedWeapon = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
	AMountedMachineGun* CurrentMountedWeapon = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Truck")
	void EnterTruckDriverSeat(ATruck* Truck);

	UFUNCTION(BlueprintCallable, Category = "Truck")
	void ExitTruckDriverSeat();

	UFUNCTION(BlueprintCallable, Category = "Truck")
	void EnterTruckCargo(ATruck* Truck);

	UFUNCTION(BlueprintCallable, Category = "Truck")
	void ExitTruckCargo();

	UFUNCTION(BlueprintCallable, Category = "Truck")
	void EnterMountedWeapon(ATruck* Truck, AMountedMachineGun* MountedWeapon);

	UFUNCTION(BlueprintCallable, Category = "Truck")
	void ExitMountedWeapon(bool bReturnToCargo = true);

	UFUNCTION(BlueprintCallable, Category = "Truck")
	bool CanInteractWithMountedWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "Truck")
	bool IsOnTruckCargo() const { return bIsOnTruckCargo; }

	UFUNCTION(BlueprintCallable, Category = "Truck")
	bool IsDrivingTruck() const { return bIsDrivingTruck; }

	UFUNCTION(BlueprintCallable, Category = "Truck")
	bool IsUsingMountedWeapon() const { return bIsUsingMountedWeapon; }

	void SyncMovementToServer();

	void SetHealth(float currentHp, float maxHp);   //체력 수정 함수

protected:
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	FRotator GetMovementViewRotation() const;
	void StartJump();
	void StopJump();
	void Fire();
	void StopFire();
	void StartAim();
	void StopAim();
	void HandleUseHealPackInput();
	void LeaveGame();
	void SendEnterGamePacket();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	UCameraComponent* FPSCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	UCameraComponent* ThirdPersonCameraComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "FPS|Mesh")
	USkeletalMeshComponent* FPSMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Components")
	UHealthComponent* HealthComponent;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAIPerceptionStimuliSourceComponent* ZombieStimuliSource;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TSubclassOf<class AWeaponBase> WeaponClass;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = "true"))
	AWeaponBase* CurrentWeapon = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Interaction", meta = (AllowPrivateAccess = "true"))
	AActor* CurrentInteractableActor = nullptr;

	UPROPERTY()
	ETruckInteractType CurrentTruckInteractType = ETruckInteractType::None;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TSubclassOf<AWeaponBase> WeaponBPclass;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Projectile")
	TSubclassOf<AFPSProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Animation")
	TSubclassOf<UAnimInstance> UnarmedAnimClass;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Animation")
	TSubclassOf<UAnimInstance> ArmedAnimClass;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Animation")
	UAnimationAsset* DrivingAnimationAsset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Aim", meta = (AllowPrivateAccess = "true"))
	bool bIsHoldAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Aim", meta = (AllowPrivateAccess = "true"))
	bool bIsIronSightAiming = false;

	bool bAimInputHeld = false;
	float AimPressedTime = 0.0f;

	// 기본 카메라 FOV
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float DefaultThirdPersonFOV = 90.0f;

	// 줌 했을 때 FOV
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float AimingThirdPersonFOV = 55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float DefaultBoomLength = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float AimingBoomLength = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	FVector DefaultCameraBoomSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	FVector AimingCameraBoomSocketOffset = FVector(0.0f, 45.0f, 20.0f);

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float AimInterpSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float AimTapToggleThreshold = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float IronSightFOV = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float IronSightInterpSpeed = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	float RecoilRecoverySpeed = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Aim")
	bool bUseFirstPersonWeaponIronSight = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Aim", meta = (AllowPrivateAccess = "true"))
	FRotator RecoilRecoveryRemaining = FRotator::ZeroRotator;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<EItemType> Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Gameplay", meta = (AllowPrivateAccess = "true"))
	FVector FirePosition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true"))
	int32 MaxItemCount = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HealPackHealAmount = 50.0f;

	void RefreshStage2ItemUI();

public:
	void SetPlayerInfo(const Protocol::PosInfo& Info);
	void SetDestInfo(const Protocol::PosInfo& Info);
	void EquipWeapon(AWeaponBase* Weapon);
	void DestroyEquippedWeapon();

	void Heal(float h);

	Protocol::PosInfo* GetPlayerInfo() { return PlayerInfo; }

protected:
	Protocol::PosInfo* PlayerInfo;
	Protocol::PosInfo* DestInfo;
	Protocol::MoveState RemoteLastState = Protocol::MOVE_STATE_IDLE;

	const float MOVE_PACKET_SEND_DELAY = 0.05f;
	float MovePacketSendTimer = 0.f;

	void ApplyDefaultAnimationClass();
	void PlayDrivingAnimation();
	void HandleMountedWeaponAutoFire();
	void BeginTruckCargoWalk(ATruck* Truck);
	void EndTruckCargoWalk();
	void ConstrainToTruckCargoBounds();
	void SetTruckMeshMovementIgnored(ATruck* Truck, bool bShouldIgnore);
	void SetHeldWeaponVehicleVisibility(bool bShouldHide);
	void UpdateIronSightFirstPersonView(bool bEnable);
	void ClearTruckInteractionState();
	void RefreshTruckInteractionState(ATruck* Truck);

	void SendMovePacket();

	FTimerHandle MountedWeaponAutoFireTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Truck")
	float TruckCargoBoundsPadding = 20.0f;

	UPROPERTY(Transient)
	FVector SavedTruckCargoLocalLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasSavedTruckCargoLocalLocation = false;

	UPROPERTY(Transient)
	FVector ReplicatedTruckCargoLocalLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasReplicatedTruckCargoLocalLocation = false;

	UPROPERTY(Transient)
	FVector LastTruckCargoLocalLocationForMoveState = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastTruckCargoLocalLocationForMoveState = false;

	UPROPERTY(Transient)
	bool bEnterGamePacketSent = false;

	UPROPERTY(Transient)
	bool bFirstPersonIronSightViewActive = false;
};
