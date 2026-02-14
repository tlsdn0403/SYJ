// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FPSBaseCharacter.generated.h"

class UCameraComponent;
class AWeaponBase;
class USpringArmComponent;
class UHealthComponent;
class UInventoryWidget;

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


    // 스폰할 발사체 클래스입니다.
    UPROPERTY(EditDefaultsOnly, Category = Projectile)
    TSubclassOf<class AFPSProjectile> ProjectileClass;

    // 체력관리 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComponent;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//// Called to bind functionality to input
	//virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
 //   
    
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

 /*   UFUNCTION()
    void OnPress1();*/

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

    ////위젯
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    //TSubclassOf<UInventoryWidget> InventoryClass; //어떤 위젯 사용할것인지에 대한 변수

    //UPROPERTY()
    //UInventoryWidget* InventoryWidget;
};
