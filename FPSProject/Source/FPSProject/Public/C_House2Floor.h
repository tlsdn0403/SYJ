// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "C_HouseBase.h"
#include "C_House2Floor.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AC_House2Floor : public AC_HouseBase
{
	GENERATED_BODY()
protected:
	virtual void OnConstruction(const FTransform& Transform) override;
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySettings")
	int32 HoleX = 3;	//¹Ù´Ú °¡·ÎÄ­¼ö
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySettings")
	int32 HoleY = 1;	//¹Ù´Ú ¼¼·ÎÄ­¼ö
};
