// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ObjectPoolSubSystem.generated.h"

/**
 * 
 */
 // 풀링 가능한 Actor를 위한 인터페이스
UINTERFACE(MinimalAPI, Blueprintable)
class UFPSPoolableInterface : public UInterface
{
    GENERATED_BODY()
};

class FPSPROJECT_API IFPSPoolableInterface
{
    GENERATED_BODY()

public:
    //풀링을 쓰고싶다면 아래 3가지 함수가 있어야 한다
  
    // 풀에서 꺼낼 때 호출
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pool")
    void OnPoolActivate();

    // 풀에 반환될 때 호출
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pool")
    void OnPoolDeactivate();

    // 스폰 위치 설정
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pool")
    void OnPoolSpawn(const FVector& Location, const FRotator& Rotation);
};
UCLASS()
class FPSPROJECT_API UObjectPoolSubSystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
    // Subsystem 생명주기
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // 풀 초기화
    UFUNCTION(BlueprintCallable, Category = "Pool")
    void InitializePool(TSubclassOf<AActor> ActorClass, int32 PoolSize = 20);

    // 풀에서 Actor 가져오기
    UFUNCTION(BlueprintCallable, Category = "Pool")
    AActor* SpawnFromPool(TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation);

    // 풀에 반환
    UFUNCTION(BlueprintCallable, Category = "Pool")
    void ReturnToPool(AActor* Actor);

    // 모든 풀 정리
    UFUNCTION(BlueprintCallable, Category = "Pool")
    void ClearAllPools();

private:
    AActor* CreateNewActor(TSubclassOf<AActor> ActorClass);
    void DeactivateActor(AActor* Actor);
    void ActivateActor(AActor* Actor);

    // 클래스별 사용 가능한 Actor 풀
    TMap<UClass*, TArray<AActor*>> AvailablePools;

    // 클래스별 사용 중인 Actor 풀
    TMap<UClass*, TArray<AActor*>> ActivePools;
};
