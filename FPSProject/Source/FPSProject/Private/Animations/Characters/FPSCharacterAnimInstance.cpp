// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Characters/FPSCharacterAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/FPSBaseCharacter.h"

void UFPSCharacterAnimInstance::NativeInitializeAnimation()
{
	//부모함수 비어있어서 SUPER호출 할 필요없다
	OwningCharacter = Cast<AFPSBaseCharacter>(TryGetPawnOwner());  //애니메이션 인스턴스가 소유한 폰을 가져옴

	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement(); //캐릭터 무브먼트 컴포넌트를 가져옴
	}
}

void UFPSCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	//부모함수 비어있어서 SUPER호출 할 필요없다
	if(! OwningCharacter || ! OwningMovementComponent)
	{
		return;
	}
	GroundSpeed = OwningCharacter->GetVelocity().Size2D();

	bHasAcceleration = OwningMovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;
}
