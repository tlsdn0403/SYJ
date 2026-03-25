#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/LootItemBase.h"
#include "Components/InteractTriggerComponent.h"
#include "Protocol.pb.h"
#include "FPSBaseCharacter.generated.h"

// 전방 선언 
class UCameraComponent;
class AWeaponBase;
class USpringArmComponent;
class UHealthComponent;
class AFPSProjectile;
class UInventoryWidget;
class UAnimInstance;
class ATruck;

/** 인벤토리 업데이트 알림을 위한 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, const TArray<EItemType>&, CurrentInventory);

UCLASS()
class FPSPROJECT_API AFPSBaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AFPSBaseCharacter();
    virtual ~AFPSBaseCharacter();

    // --- 인터페이스 섹션 (Public) ---
    virtual void Tick(float DeltaTime) override;                                                   
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;   

    /** 무기 및 상호작용 */
    UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
    void SetCurrentWeapon(AWeaponBase* NewWeapon);

    UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
    void ClearCurrentWeapon();

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


    // 인터렉트 정보
    void SetCurrentTruckInteractType(ETruckInteractType NewType) { CurrentTruckInteractType = NewType; }
    ETruckInteractType GetCurrentTruckInteractType() const { return CurrentTruckInteractType; }



    // ------------------------- 캐릭터 트럭 관련 ----------------------------------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
    bool bIsOnTruckCargo = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truck")
    ATruck* CurrentTruck = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Truck")
    void EnterTruckCargo(ATruck* Truck);

    UFUNCTION(BlueprintCallable, Category = "Truck")
    void ExitTruckCargo();

    UFUNCTION(BlueprintCallable, Category = "Truck")
    bool IsOnTruckCargo() const { return bIsOnTruckCargo; }
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

    // 
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Components")
    UHealthComponent* HealthComponent;

    // 에디터에서 설정할 무기 블루프린트 클래스 (BP_Weapon)
    UPROPERTY(EditAnywhere, Category = "Weapon")
    TSubclassOf<class AWeaponBase> WeaponClass;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = "true"))
    AWeaponBase* CurrentWeapon = nullptr;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Interaction", meta = (AllowPrivateAccess = "true"))
    AActor* CurrentInteractableActor = nullptr;

    UPROPERTY()
    ETruckInteractType CurrentTruckInteractType = ETruckInteractType::None;

   

    UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
    TSubclassOf<AWeaponBase> WeaponBPclass;

    // 
    UPROPERTY(EditDefaultsOnly, Category = "FPS|Projectile")
    TSubclassOf<AFPSProjectile> ProjectileClass;


    // 애니메이션
    UPROPERTY(EditDefaultsOnly, Category = "FPS|Animation")
    TSubclassOf<UAnimInstance> UnarmedAnimClass;

    UPROPERTY(EditDefaultsOnly, Category = "FPS|Animation")
    TSubclassOf<UAnimInstance> ArmedAnimClass;


    // 인벤토리
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true"))
    TArray<EItemType> Inventory;

    // 총알 발사 포지션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Gameplay", meta = (AllowPrivateAccess = "true"))
    FVector FirePosition;

    // 
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Inventory", meta = (AllowPrivateAccess = "true"))
    int32 MaxItemCount = 5;

public:
    // 플레이어 이동 관련 함수
    void SetPlayerInfo(const Protocol::PosInfo& Info);
    void SetDestInfo(const Protocol::PosInfo& Info);

    // 서버로부터 무기 장착 명령을 받았을 때 호출될 함수
    void EquipWeaponFromField(AWeaponBase* Weapon);

    Protocol::PosInfo* GetPlayerInfo() { return PlayerInfo; }

protected:
    Protocol::PosInfo* PlayerInfo; // 현재 위치
    Protocol::PosInfo* DestInfo; // 목적지
    // 상대방 캐릭터의 이전 상태를 기억하기 위한 변수(점프가 두번되는 문제를 막기 위함)
    Protocol::MoveState RemoteLastState = Protocol::MOVE_STATE_IDLE;

    const float MOVE_PACKET_SEND_DELAY = 0.05f; // (초당 20번 전송으로 약간 줄여서 부드럽게)
    float MovePacketSendTimer = 0.f;

    void SendMovePacket();
};