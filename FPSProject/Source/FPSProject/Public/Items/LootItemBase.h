// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InteractTriggerComponent.h"
#include "Interface/InteractInterface.h"
#include "LootItemBase.generated.h"


UENUM(BlueprintType)
enum class EItemType : uint8
{
	None        UMETA(DisplayName = "None"),
	Ammo        UMETA(DisplayName = "Ammo"),      // 총알
	Fuel        UMETA(DisplayName = "Fuel"),      // 연료
	MedicalKit  UMETA(DisplayName = "Medical Kit")  // 구급상자
};

UCLASS()
class FPSPROJECT_API ALootItemBase : public AActor, public IInteractInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALootItemBase();

	// 상호작용 범위 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* InteractTrigger;

	// 에디터에서 이 아이템이 뭔지 설정할 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Properties")
	EItemType ItemType = EItemType::None; // 기본값 None

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "My setting")
	FText ItemName; // 기본값 None
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
