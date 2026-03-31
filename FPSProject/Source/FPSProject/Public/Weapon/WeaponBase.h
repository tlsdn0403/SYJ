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
protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<AFPSProjectile> ProjectileClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector MuzzleOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName MuzzleSocketName = TEXT("b_gun_muzzleflash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName AttachSocketName = TEXT("GripPoint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim")
	float AimTraceDistance = 30000.0f;

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
	virtual void Tick(float DeltaTime) override;

protected:
	float GetCurrentSpreadAngleDegrees() const;
	void ApplyFireRecoil() const;

	/** The character holding this weapon */
	//UPROPERTY()
	UPROPERTY(EditAnywhere, Category = "Network")
	uint64 ObjectId = 0;
	AFPSBaseCharacter* Character;
};
