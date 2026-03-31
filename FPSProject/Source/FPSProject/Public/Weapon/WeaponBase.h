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
	FName AttachSocketName = TEXT("GripPoint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UParticleSystem* GunParticleEffect;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void AttachWeapon(AFPSBaseCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void DetachWeapon();


public:
	virtual void Tick(float DeltaTime) override;

protected:
	/** The character holding this weapon */
	//UPROPERTY()
	UPROPERTY(EditAnywhere, Category = "Network")
	uint64 ObjectId = 0;
	AFPSBaseCharacter* Character;
};
