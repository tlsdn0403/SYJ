// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/AR4.h"
#include "Characters/FPSBaseCharacter.h"

void AAR4::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
    Character = TargetCharacter;
    if (!Character) return;

    // 부착할 메쉬 선택
    USkeletalMeshComponent* AttachMesh = Character->GetMesh();

    // 부착 트랜스폼 설정
    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);

    // 부착 후 무기  위치, 회전 조정
    WeaponMesh->SetRelativeLocation(FVector(-8.883712f, 5.298776f, -0.142411f));
    WeaponMesh->SetRelativeRotation(FRotator(-0.023171f, 82.465882f, 13.423545f));

    // 캐릭터의 CurrentWeapon 업데이트
    Character->SetCurrentWeapon(this);


}

void AAR4::Fire()
{
	Super::Fire();
}
