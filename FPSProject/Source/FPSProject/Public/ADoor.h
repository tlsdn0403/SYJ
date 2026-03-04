// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractInterface.h"
#include "ADoor.generated.h"


//상호작용이 있을때만 활동할 것이라 그 외 이벤트가 필요없

class UWidgetComponent;
class UInteractTriggerComponent;
class AFPSBaseCharacter;

UCLASS()
class FPSPROJECT_API AADoor : public AActor, public IInteractInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AADoor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot;	//여러 컴포넌트를 묶어주는 역할, 위젯도 추가될 것이기에 추가.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMeshComp;

	// 상호작용 범위 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractTriggerComponent* InteractTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UWidgetComponent* WidgetComp;

	// 인터페이스 함수 오버라이드 (F키 눌렀을 때 실행될 내용)
	virtual void Interact_Implementation(AFPSBaseCharacter* Character) override;

	bool bOpen = false;
	FRotator OriginalRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "setting")
	FRotator Target = FRotator(0.f, 90.f, 0.f);	//Pitch - Y  ,Yaw - Z , Roll - X

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "setting")
	FRotator MoveDir = FRotator(0.f, -90.f, 0.f);	//Pitch - Y  ,Yaw - Z , Roll - X

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "setting")
	float MoveTime = 3.f;

	//델리게이트 함수 (트리거 컴포넌트의 이벤트에 바인딩 될 함수)
	UFUNCTION()
	void WidgetStart(AActor* OtherActor);

	UFUNCTION()
	void WidgetEnd(AActor* OtherActor);
};
