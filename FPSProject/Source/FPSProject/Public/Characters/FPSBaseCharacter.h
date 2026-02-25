#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/LootItemBase.h"
#include "FPSBaseCharacter.generated.h"

// 전방 선언 
class UCameraComponent;
class AWeaponBase;
class USpringArmComponent;
class UHealthComponent;
class AFPSProjectile;
class UInventoryWidget;

/** 인벤토리 업데이트 알림을 위한 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, const TArray<EItemType>&, CurrentInventory);

UCLASS()
class FPSPROJECT_API AFPSBaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AFPSBaseCharacter();

    // --- 인터페이스 섹션 (Public) ---
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


    // 스폰할 발사체 클래스.
    UPROPERTY(EditDefaultsOnly, Category = Projectile)
    TSubclassOf<class AFPSProjectile> ProjectileClass;

    // 체력관리 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComponent;


	//--------------------인벤토리 ----------------------------------------------------------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<EItemType> Inventory;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
    int32 MaxItemCount = 5;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


    /** 무기 및 상호작용 */
    UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
    void SetCurrentWeapon(AWeaponBase* NewWeapon) { CurrentWeapon = NewWeapon; }

    void Interact();
    void SetInteractableActor(AActor* NewActor);

    /** 인벤토리 시스템 */
    UFUNCTION(BlueprintCallable, Category = "FPS|Inventory")
    bool AddItem(EItemType NewItemType);

    UFUNCTION(BlueprintCallable, Category = "FPS|Inventory")
    TArray<EItemType> OffloadItems();

    UFUNCTION(BlueprintPure, Category = "FPS|Inventory")
    int32 GetItemCount() const { return Inventory.Num(); }

    /** UI 바인딩용 델리게이트 */
    UPROPERTY(BlueprintAssignable, Category = "FPS|Inventory")
    FOnInventoryUpdated OnInventoryUpdated;

	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }
	AActor* GetCurrentInteractableActor() const { return CurrentInteractableActor; }

    
protected:
    virtual void BeginPlay() override;

    /* 입력 처리 함수 */
    void MoveForward(float Value);
    void MoveRight(float Value);
    void StartJump();
    void StopJump();
    void Fire();

    // --- 컴포넌트 섹션 (Protected) ---
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

private:

    // 8바이트, 포인터 및 컨테이너
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = "true"))
    AWeaponBase* CurrentWeapon = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Interaction", meta = (AllowPrivateAccess = "true"))
    AActor* CurrentInteractableActor = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
    TSubclassOf<AWeaponBase> WeaponBPclass;

    UPROPERTY(EditDefaultsOnly, Category = "FPS|Projectile")
    TSubclassOf<AFPSProjectile> ProjectileClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true"))
    TArray<EItemType> Inventory;

    // 12바이트 float*3 구조체
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Gameplay", meta = (AllowPrivateAccess = "true"))
    FVector FirePosition;

    // 4바이트 영역 int32, float
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true"))
    int32 MaxItemCount = 5;

};