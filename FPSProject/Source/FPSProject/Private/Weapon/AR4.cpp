// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/AR4.h"

void AAR4::ApplyFirstPersonZoomTransform()
{
	FirstPersonWeaponRelativeLocation = FVector(0.114707f, 0.0f, -17.005057f);
	FirstPersonWeaponRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	FirstPersonWeaponRelativeScale = FVector(1.0f, 1.0f, 1.0f);
	bAutoAlignFirstPersonAimPoint = false;
}

AAR4::AAR4()
{
	ApplyFirstPersonZoomTransform();
}

void AAR4::BeginPlay()
{
	Super::BeginPlay();
	ApplyFirstPersonZoomTransform();
}

void AAR4::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
	Super::AttachWeapon(TargetCharacter);
}

void AAR4::Fire()
{
	Super::Fire();
}
