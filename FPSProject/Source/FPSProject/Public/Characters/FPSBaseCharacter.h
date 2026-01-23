// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FPSBaseCharacter.generated.h"

class UCameraComponent;
class AWeaponBase;
class USpringArmComponent;
class UHealthComponent;

// 손 모양 UI를 위해서 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, int32, CurrentCount); 

UCLASS()
class FPSPROJECT_API AFPSBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AFPSBaseCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


    // 스폰할 발사체 클래스.
    UPROPERTY(EditDefaultsOnly, Category = Projectile)
    TSubclassOf<class AFPSProjectile> ProjectileClass;

    // 체력관리 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComponent;

	// 인벤토리 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 CurrentItemCount = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
    int32 MaxItemCount = 5;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    // 현재 장착한 무기
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetCurrentWeapon(AWeaponBase* NewWeapon) { CurrentWeapon = NewWeapon; }

    // 앞으로 이동 및 뒤로 이동 입력을 처리
    UFUNCTION()
    void MoveForward(float Value);

    // 오른쪽 이동 및 왼쪽 이동 입력을 처리
    UFUNCTION()
    void MoveRight(float Value);

    // 키가 눌릴 경우 점프 플래그를 설정
    UFUNCTION()
    void StartJump();

    // 키가 떼어질 경우 점프 플래그를 지움
    UFUNCTION()
    void StopJump();

    UFUNCTION()
    void Fire();

    // FPS 카메라
    UPROPERTY(VisibleAnywhere)
    UCameraComponent* FPSCameraComponent;

    // 3인칭용 카메라
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UCameraComponent* ThirdPersonCameraComponent;

    //메시 
    UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
    USkeletalMeshComponent* FPSMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
    FVector FirePosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Weapon")
    AWeaponBase* CurrentWeapon = nullptr;

    // 무기   
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> WeaponBPclass;


    // 현재 상호작용 가능한 액터 저장 변수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    AActor* CurrentInteractableActor;

    // 상호작용 시도 함수
    void Interact();

    // TriggerComponent에서 호출하여 캐릭터에게 대상 설정
    void SetInteractableActor(AActor* NewActor);

    //--------------- 인벤토리 관련 함수들--------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(); // 아이템 획득 시도 (꽉 찼으면 false)

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 OffloadItems(); // 가진 아이템을 모두 반환(트럭에 넣을 때)

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetItemCount() const { return CurrentItemCount; }

    // UI 업데이트를 위한 델리게이트 (손모양 UI 갱신용)
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryUpdated OnInventoryUpdated;

    //--------------------------------------------------------------------------------
};
