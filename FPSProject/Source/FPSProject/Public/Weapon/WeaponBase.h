#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class AFPSProjectile;
class AFPSBaseCharacter;
class USoundBase;
class UAnimMontage;
class UParticleSystem;
class USkeletalMeshComponent;
class USkeletalMesh;
class UCameraComponent;

UCLASS()
class FPSPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void SetWeaponUser(AFPSBaseCharacter* NewCharacter);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponCollisionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponHidden(bool Hidden);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Aim")
	bool GetAimCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	UFUNCTION(BlueprintCallable, Category = "Weapon|FirstPerson")
	void SetFirstPersonViewEnabled(bool bEnabled, UCameraComponent* AttachCamera);

	UFUNCTION(BlueprintPure, Category = "Weapon|FirstPerson")
	bool IsFirstPersonViewEnabled() const { return bFirstPersonViewEnabled; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<AFPSProjectile> ProjectileClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|FirstPerson")
	USkeletalMeshComponent* FirstPersonWeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector MuzzleOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName AttachSocketName = TEXT("GripPoint");

	// Original FPSBaseCharacter mesh used as the canonical hand/socket alignment.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ThirdPerson|Alignment")
	TObjectPtr<USkeletalMesh> ThirdPersonAlignmentReferenceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|ThirdPerson|Alignment")
	FVector ThirdPersonWeaponRelativeLocation = FVector(-8.883712f, 5.298776f, -0.142411f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|ThirdPerson|Alignment")
	FRotator ThirdPersonWeaponRelativeRotation = FRotator(-0.023171f, 82.465882f, 13.423545f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|ThirdPerson|Alignment")
	FVector ThirdPersonWeaponRelativeScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	float AimTraceDistance = 30000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FirstPerson")
	FVector FirstPersonWeaponRelativeLocation = FVector(0.0f, 0.0f, -15.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FirstPerson")
	FRotator FirstPersonWeaponRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FirstPerson")
	FVector FirstPersonWeaponRelativeScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FirstPerson|Alignment")
	bool bAutoAlignFirstPersonAimPoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FirstPerson|Alignment")
	FName FirstPersonAimPointSocketName = TEXT("ADS_Socket");

	// Camera-local fine tuning in centimeters: X moves right, Y moves up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FirstPerson|Alignment")
	FVector2D FirstPersonAimPointCameraOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	FName AimCameraSocketName = TEXT("ADS_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	FVector AimCameraFallbackOffset = FVector(-35.0f, 0.0f, 4.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	bool bUseAimSocketForIronSightTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UParticleSystem* GunParticleEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	float HipFireSpreadAngleDegrees = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	float AimSpreadAngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	FVector2D HipFireRecoilPitchRange = FVector2D(0.9f, 1.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float HipFireRecoilYawMagnitude = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	FVector2D AimRecoilPitchRange = FVector2D(0.35f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float AimRecoilYawMagnitude = 0.18f;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void AttachWeapon(AFPSBaseCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void DetachWeapon();

public:
	// 서버가 발급해준 아이템 고유 ID (이걸 알아야 줍기 요청을 할 수 있음!)
	uint64 ItemObjectId = 0;

	void RemoteFire();

protected:
	bool GetAimSocketViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	float GetCurrentSpreadAngleDegrees() const;
	void ApplyFireRecoil() const;
	void PlayMuzzleFlash() const;

	void SyncFirstPersonWeaponMesh();
	void AlignFirstPersonAimPoint(UCameraComponent* AttachCamera);
	void AlignThirdPersonWeaponToReference(AFPSBaseCharacter* TargetCharacter);

	bool bFirstPersonViewEnabled = false;

	AFPSBaseCharacter* Character;
};
