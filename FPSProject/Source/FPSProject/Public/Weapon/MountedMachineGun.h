#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MountedMachineGun.generated.h"

class AFPSBaseCharacter;
class AFPSProjectile;
class UAnimInstance;
class UParticleSystem;
class USceneComponent;
class UChildActorComponent;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mounted Gun")
	UChildActorComponent* MagazineActorComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun")
	FVector CameraSocketOffset = FVector(-20.0f, 0.0f, 6.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Aim")
	float MinYaw = -120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Aim")
	float MaxYaw = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Aim")
	float YawInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Aim")
	float PitchInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Aim")
	float CameraRotationLagSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Aim")
	float CameraLocationLagSpeed = 20.0f;

	// ---------------------- 기관총 애니메이션 설정 -------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	TSubclassOf<UAnimInstance> MountedGunAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float TriggerPressedValue = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float SlideKickDistance = -21.391f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float SlideReturnSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float HorizontalBoneYawMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	float VerticalBonePitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Animation")
	FVector GunTranslationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	TSubclassOf<AActor> MagazineActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineFireEventName = TEXT("Play_Animate_Bullets_Inside_Magazine_TimeL_CE");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineSetBulletCountFunctionName = TEXT("SetBulletCount");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	int32 MagazineCapacity = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineBulletCountPropertyName = TEXT("Number Of Bullets Inside Magazine");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineFirePressedPropertyName = TEXT("Firing Key Is Pressed");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mounted Gun|Magazine")
	FName MagazineSystemWorkingPropertyName = TEXT("Magazine System Is Working");

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
	void UpdateMagazineState(bool bTriggeredByFire);
	void SetAnimFloatValue(FName PropertyName, float Value) const;
	void SetAnimVectorValue(FName PropertyName, const FVector& Value) const;
	void SetAnimRotatorValue(FName PropertyName, const FRotator& Value) const;
	void SetChildActorIntValue(UChildActorComponent* ChildActorComponent, FName PropertyName, int32 Value) const;
	void SetChildActorBoolValue(UChildActorComponent* ChildActorComponent, FName PropertyName, bool Value) const;
	bool CallChildActorFunction(UChildActorComponent* ChildActorComponent, FName FunctionName) const;

	UPROPERTY()
	AFPSBaseCharacter* CurrentUser = nullptr;

	float LastFireTime = -1000.0f;
	float CurrentSlideOffset = 0.0f;
	float CurrentTriggerValue = 0.0f;
	float TargetRelativeYaw = 0.0f;
	float TargetRelativePitch = 0.0f;
	float CurrentRelativeYaw = 0.0f;
	float CurrentRelativePitch = 0.0f;
	int32 CurrentBulletsInMagazine = 50;
	bool bFireInputActive = false;
};

