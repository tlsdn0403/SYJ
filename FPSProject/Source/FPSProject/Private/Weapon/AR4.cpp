// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/AR4.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"

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
    WeaponMesh->SetRelativeLocation(FVector(-7.640821f, 4.648937f, -1.158742f));
    WeaponMesh->SetRelativeRotation(FRotator(-6.316770f, -264.543091f, 2.009403f));

    // 캐릭터의 CurrentWeapon 업데이트
    Character->SetCurrentWeapon(this);


}

void AAR4::Fire()
{
	Super::Fire();

    //각 총기마다 소켓 이름이 달라서 이렇게 처리
    UGameplayStatics::SpawnEmitterAttached(GunParticleEffect, WeaponMesh, TEXT("b_gun_muzzleflash"));
}
