#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MountedMachineGun.generated.h"

class AFPSBaseCharacter;
class AFPSProjectile;
class UAnimInstance;
class UParticleSystem;
class USceneComponent;
class USkeletalMeshComponent;
class USoundBase;
class USpringArmComponent;

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

	// ---------------------- 기관총 애니메이션 설정 -------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	TSubclassOf<UAnimInstance> MountedGunAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float TriggerPressedValue = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float SlideKickDistance = -21.391f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float SlideReturnSpeed = 18.0f;

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

private:
	void UpdateAnimationState(float DeltaTime);
	void SetAnimFloatValue(FName PropertyName, float Value) const;
	void SetAnimVectorValue(FName PropertyName, const FVector& Value) const;
	void SetAnimRotatorValue(FName PropertyName, const FRotator& Value) const;

	UPROPERTY()
	AFPSBaseCharacter* CurrentUser = nullptr;

	float LastFireTime = -1000.0f;
	float CurrentSlideOffset = 0.0f;
	float CurrentTriggerValue = 0.0f;
};
