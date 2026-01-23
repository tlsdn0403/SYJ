// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InteractTriggerComponent.h"
#include "Interface/InteractInterface.h"
#include "LootItemBase.generated.h"

UCLASS()
class FPSPROJECT_API ALootItemBase : public AActor, public IInteractInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALootItemBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 아이템 mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 인터페이스 함수 오버라이드 (F키 눌렀을 때 실행될 내용)
	virtual void Interact_Implementation(AFPSBaseCharacter* Character) override;
};
