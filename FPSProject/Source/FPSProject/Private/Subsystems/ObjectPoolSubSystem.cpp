// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/ObjectPoolSubSystem.h"

void UObjectPoolSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("ObjectPoolSubSystem Initialized"));
}

void UObjectPoolSubSystem::Deinitialize()
{
    ClearAllPools();
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("ObjectPoolSubSystem Deinitialized"));
}

bool UObjectPoolSubSystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // 게임 월드에서만 생성
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        return World->IsGameWorld();
    }
    return false;
}

void UObjectPoolSubSystem::InitializePool(TSubclassOf<AActor> ActorClass, int32 PoolSize)
{
    if (!ActorClass) return;

    UClass* ClassPtr = ActorClass.Get();

    // 이미 풀이 있으면 스킵
    if (AvailablePools.Contains(ClassPtr) && AvailablePools[ClassPtr].Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pool for %s already exists"), *ActorClass->GetName());
        return;
    }

    // 풀 배열 초기화
    AvailablePools.Add(ClassPtr, TArray<AActor*>());
    ActivePools.Add(ClassPtr, TArray<AActor*>());

    // Actor 미리 생성
    for (int32 i = 0; i < PoolSize; i++)
    {
        AActor* Actor = CreateNewActor(ActorClass);
        if (Actor)
        {
            DeactivateActor(Actor);
            AvailablePools[ClassPtr].Add(Actor);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Initialized pool for %s with %d actors"),
        *ActorClass->GetName(), PoolSize);
}

AActor* UObjectPoolSubSystem::SpawnFromPool(TSubclassOf<AActor> ActorClass,
    const FVector& Location,
    const FRotator& Rotation)
{
    if (!ActorClass) return nullptr;

    UClass* ClassPtr = ActorClass.Get();
    AActor* Actor = nullptr;

    // 풀에 사용 가능한 Actor가 있는 경우
    if (AvailablePools.Contains(ClassPtr) && AvailablePools[ClassPtr].Num() > 0)
    {
        Actor = AvailablePools[ClassPtr].Pop();
    }
    else
    {
        // 풀이 비어있으면 새로 생성
        Actor = CreateNewActor(ActorClass);

        if (!ActivePools.Contains(ClassPtr))
        {
            ActivePools.Add(ClassPtr, TArray<AActor*>());
        }
    }

    if (Actor)
    {
        // 위치 설정
        Actor->SetActorLocation(Location);
        Actor->SetActorRotation(Rotation);

        // 활성화
        ActivateActor(Actor);

        // 인터페이스 OnPoolSpawn 호출
        if (Actor->Implements<UFPSPoolableInterface>())
        {
            IFPSPoolableInterface::Execute_OnPoolSpawn(Actor, Location, Rotation);
        }

        ActivePools[ClassPtr].Add(Actor);
    }

    return Actor;
}

void UObjectPoolSubSystem::ReturnToPool(AActor* Actor)
{
    if (!Actor) return;

    UClass* ClassPtr = Actor->GetClass();

    // 비활성화
    DeactivateActor(Actor);

    // Active에서 제거
    if (ActivePools.Contains(ClassPtr))
    {
        ActivePools[ClassPtr].Remove(Actor);
    }

    // Available에 추가
    if (!AvailablePools.Contains(ClassPtr))
    {
        AvailablePools.Add(ClassPtr, TArray<AActor*>());
    }
    AvailablePools[ClassPtr].Add(Actor);
}

void UObjectPoolSubSystem::ClearAllPools()
{
    for (auto& Pair : AvailablePools)
    {
        for (AActor* Actor : Pair.Value)
        {
            if (Actor && IsValid(Actor))
            {
                Actor->Destroy();
            }
        }
    }

    for (auto& Pair : ActivePools)
    {
        for (AActor* Actor : Pair.Value)
        {
            if (Actor && IsValid(Actor))
            {
                Actor->Destroy();
            }
        }
    }

    AvailablePools.Empty();
    ActivePools.Empty();
}

AActor* UObjectPoolSubSystem::CreateNewActor(TSubclassOf<AActor> ActorClass)
{
    UWorld* World = GetWorld();
    if (!World || !ActorClass) return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    return World->SpawnActor<AActor>(
        ActorClass,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams
    );
}

void UObjectPoolSubSystem::DeactivateActor(AActor* Actor)
{
    if (!Actor) return;

    // 인터페이스 OnPoolDeactivate 호출
    if (Actor->Implements<UFPSPoolableInterface>())
    {
        IFPSPoolableInterface::Execute_OnPoolDeactivate(Actor);
    }

    // 기본 비활성화
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->SetActorTickEnabled(false);
    Actor->SetActorLocation(FVector(0, 0, -10000.f));
}

void UObjectPoolSubSystem::ActivateActor(AActor* Actor)
{
    if (!Actor) return;

    // 기본 활성화
    Actor->SetActorHiddenInGame(false);
    Actor->SetActorEnableCollision(true);
    Actor->SetActorTickEnabled(true);

    // 인터페이스 OnPoolActivate 호출
    if (Actor->Implements<UFPSPoolableInterface>())
    {
        IFPSPoolableInterface::Execute_OnPoolActivate(Actor);
    }
}