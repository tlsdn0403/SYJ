#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class AFPSProjectile;
class AFPSBaseCharacter;
class USoundBase;
class UAnimMontage;
class UParticleSystem;

UCLASS()
class FPSPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();


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

	// 총기 발사시 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UParticleSystem* GunParticleEffect;



	// 무기를 캐릭터에 부착
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void AttachWeapon(AFPSBaseCharacter* TargetCharacter);

	// 무기를 캐릭터에서 해제
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void DetachWeapon();
public:
	virtual void Tick(float DeltaTime) override;

protected:
	/** The character holding this weapon */
	UPROPERTY()
	AFPSBaseCharacter* Character;

};