// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/LoadingUI.h"
#include "Components/Image.h"



void ULoadingUI::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Warning, TEXT("REAL SLOT CREATED %p"), this);
}


void ULoadingUI::connect(int num) {


}


void ULoadingUI:: logout() {

}