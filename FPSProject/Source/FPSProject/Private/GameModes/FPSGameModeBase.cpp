// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/FPSGameModeBase.h"

void AFPSGameModeBase::StartPlay()
{
    Super::StartPlay();

    check(GEngine != nullptr);

    // ����� �޽����� 5�ʰ� ǥ���մϴ�. 
    // -1 'Ű' �� �������ڰ� �޽����� ������Ʈ�ǰų� ���ΰ�ħ���� �ʵ��� �����մϴ�.
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Hello World, this is FPSGameMode!"));
}