#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class AFPSProjectile;
class AFPSBaseCharacter;
class USoundBase;
class UAnimMontage;

UCLASS()
class FPSPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	/** Attach weapon to character */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void AttachWeapon(AFPSBaseCharacter* TargetCharacter);

	/** Make the weapon fire a projectile */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire();

protected:
	virtual void BeginPlay() override;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<AFPSProjectile> ProjectileClass;

	/** Skeletal mesh for weapon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* WeaponMesh;

	/** Fire sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	USoundBase* FireSound;

	/** Fire animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* FireAnimation;

	/** Muzzle offset from the character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector MuzzleOffset;

	/** Socket name to attach weapon (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName AttachSocketName = TEXT("GripPoint");

public:
	virtual void Tick(float DeltaTime) override;

protected:
	/** The character holding this weapon */
	UPROPERTY()
	AFPSBaseCharacter* Character;

};