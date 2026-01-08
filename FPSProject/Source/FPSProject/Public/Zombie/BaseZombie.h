// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseZombie.generated.h"

class UHealthComponent;

UCLASS()
class FPSPROJECT_API ABaseZombie : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseZombie();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;


    // 사망 처리
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    void Die();

    // 살아있는지 확인
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    bool IsAlive() const { return bIsAlive; }

    //좀비 매시
    UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
    USkeletalMeshComponent* ZombieMesh;
protected:
    // 체력 컴포넌트 (기존 HealthComponent 재사용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComponent;

    // 사망 여부
    UPROPERTY(BlueprintReadOnly, Category = "Zombie")
    bool bIsAlive = true;

};
