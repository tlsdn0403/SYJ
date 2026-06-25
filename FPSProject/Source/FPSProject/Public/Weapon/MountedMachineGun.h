#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MountedMachineGun.generated.h"

class AFPSBaseCharacter;
class AFPSProjectile;
class AActor;
class UParticleSystem;
class USceneComponent;
class USkeletalMeshComponent;
class UChildActorComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class USpringArmComponent;
class UAnimInstance;

UCLASS()
class FPSPROJECT_API AMountedMachineGun : public AActor
{
	GENERATED_BODY()

public:
	AMountedMachineGun();

	virtual void Tick(float DeltaTime) override;

	void SetWeaponUser(AFPSBaseCharacter* NewUser);
	void Fire();
	void UpdateAim(const FRotator& ControlRotation);
	void ApplyNetworkAim(const FRotator& NetworkAimRotation);
	FRotator ClampAimRotation(const FRotator& DesiredRotation) const;
	FVector GetCameraLocation() const;
	FRotator GetCameraRotation() const;
	float GetCameraFOV() const { return CameraFOV; }
	void ConfigureOperatorSeat(const FTransform& SeatWorldTransform);
	void AttachUserToOperatorSeat(AFPSBaseCharacter* User);
	float GetFireInterval() const { return FireInterval; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* YawPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* PitchPivot;

	// Follows yaw (chair/base rotation) but not gun pitch.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* OperatorSeatPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USkeletalMeshComponent* GunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* MuzzlePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* CameraPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun|Animation")
	UStaticMeshComponent* FeedBulletComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun|Magazine")
	UChildActorComponent* MagazineActorComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	FName MuzzleSocketName = TEXT("Muzzle_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	FVector CameraSocketOffset = FVector(-20.0f, 0.0f, 6.0f);

	// Camera origin relative to the operator seat. Keeping the position outside the
	// gun assembly prevents the view from clipping through the modified turret mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Camera")
	FVector OperatorCameraOffset = FVector(25.0f, 0.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Camera", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float CameraFOV = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Camera", meta = (ClampMin = "1000.0"))
	float IronSightAimDistance = 100000.0f;

	// Negative values move the impact point below the exact screen center so the
	// physical front-sight tip can be used as the point of aim.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Camera", meta = (ClampMin = "-5.0", ClampMax = "5.0"))
	float IronSightAimPitchOffset = -0.35f;

	// Blueprint-added chair/platform meshes that should follow yaw, never pitch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Assembly")
	TArray<FName> YawOnlyComponentNames = {
		TEXT("Office_Chair"),
		TEXT("SM_MERGED_StaticMeshActor_96")
	};

	UPROPERTY(EditDefaultsOnly, Category = "Mounted Gun")
	TSubclassOf<AFPSProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	UParticleSystem* GunParticleEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	float FireInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	float MaxPitch = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	float MinPitch = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxYaw = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Network", meta = (ClampMin = "1.0"))
	float NetworkAimInterpolationSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Network", meta = (ClampMin = "0.01"))
	float NetworkAimSendInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Recoil")
	FVector2D RecoilPitchRange = FVector2D(0.2f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Recoil")
	float RecoilYawMagnitude = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mounted Gun|Animation")
	TSubclassOf<UAnimInstance> GunAnimationBlueprintClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float TriggerAnimationReturnSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float RecoilAnimationReturnSpeed = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mounted Gun|Shell")
	TSubclassOf<AActor> EmptyShellClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Shell")
	FName EmptyShellSocketName = TEXT("EmptyShell_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Shell")
	FVector EmptyShellEjectImpulse = FVector(20.0f, 140.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Shell")
	float EmptyShellLifetime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Shell")
	int32 EmptyShellPoolSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FVector GunRecoilTranslation = FVector(-12.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FName AmmoFeedStartSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FName AmmoFeedEndSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FVector AmmoFeedStartOffset = FVector(-24.0f, -12.0f, 4.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FVector AmmoFeedEndOffset = FVector(6.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float AmmoFeedAnimationDuration = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FVector AmmoFeedBulletScale = FVector(0.45f, 0.45f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	TSubclassOf<AActor> MagazineActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	int32 MagazineCapacity = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineFireEventName = TEXT("Play_Animate_Bullets_Inside_Magazine_TimeL_CE");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineBulletCountPropertyName = TEXT("Number Of Bullets Inside Magazine");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineFirePressedPropertyName = TEXT("Firing Key Is Pressed");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineSystemWorkingPropertyName = TEXT("Magazine System Is Working");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineAnimationPlayingTimePropertyName = TEXT("Animation Playing Time");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineFiringSpeedPropertyName = TEXT("Firing Speed");

private:
	UPROPERTY()
	AFPSBaseCharacter* CurrentUser = nullptr;

	UPROPERTY()
	UStaticMesh* FeedBulletMesh = nullptr;

	float LastFireTime = -1000.0f;
	float TriggerAnimationAlpha = 0.0f;
	float RecoilAnimationAlpha = 0.0f;
	float AmmoFeedAnimationAlpha = 0.0f;
	float MagazineAnimationPlayingTime = 0.0f;
	int32 CurrentBulletsInMagazine = 0;
	bool bMagazineFirePressed = false;
	bool bHasNetworkAimTarget = false;
	float LastNetworkAimSendTime = -1000.0f;
	FRotator NetworkAimTarget = FRotator::ZeroRotator;

	void ApplyMountedRecoil() const;
	void ApplyAimVisuals(const FRotator& AimRotation);
	void SendAimToServer(const FRotator& AimRotation);
	void ConfigureYawOnlyVisuals();
	FVector GetIronSightAimTarget(const FVector& MuzzleLocation) const;
	void ApplyFireAnimation();
	void UpdateFireAnimation(float DeltaTime);
	void StartAmmoFeedAnimation();
	void UpdateAmmoFeedAnimation(float DeltaTime);
	void UpdateMagazineAnimationState(bool bTriggeredByFire);
	void SetAnimFloatProperty(UAnimInstance* AnimInstance, const TCHAR* PropertyName, float Value) const;
	void SetAnimVectorProperty(UAnimInstance* AnimInstance, const TCHAR* PropertyName, const FVector& Value) const;
	void SetChildActorIntProperty(UChildActorComponent* ChildActorComponent, FName PropertyName, int32 Value) const;
	void SetChildActorFloatProperty(UChildActorComponent* ChildActorComponent, FName PropertyName, float Value) const;
	void SetChildActorBoolProperty(UChildActorComponent* ChildActorComponent, FName PropertyName, bool Value) const;
	bool CallChildActorFunction(UChildActorComponent* ChildActorComponent, FName FunctionName) const;
	void SpawnEmptyShell();
	void ResetEmptyShellPhysics(AActor* ShellActor, bool bEnablePhysics) const;
	void ReturnEmptyShellToPool(AActor* ShellActor) const;
};

