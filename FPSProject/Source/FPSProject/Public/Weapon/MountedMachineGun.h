#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MountedMachineGun.generated.h"

class AFPSBaseCharacter;
class AFPSProjectile;
class UParticleSystem;
class USceneComponent;
class USkeletalMeshComponent;
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
	FVector GetCameraLocation() const;
	FRotator GetCameraRotation() const;
	float GetFireInterval() const { return FireInterval; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* YawPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* PitchPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USkeletalMeshComponent* GunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* MuzzlePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	USceneComponent* CameraPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	FVector CameraSocketOffset = FVector(-20.0f, 0.0f, 6.0f);

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

private:
	UPROPERTY()
	AFPSBaseCharacter* CurrentUser = nullptr;

	float LastFireTime = -1000.0f;
	float TriggerAnimationAlpha = 0.0f;
	float RecoilAnimationAlpha = 0.0f;

	void ApplyMountedRecoil() const;
	void ApplyFireAnimation();
	void UpdateFireAnimation(float DeltaTime);
	void SetAnimFloatProperty(UAnimInstance* AnimInstance, const TCHAR* PropertyName, float Value) const;
};
