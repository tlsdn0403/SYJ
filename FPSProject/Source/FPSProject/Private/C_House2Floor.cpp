// Fill out your copyright notice in the Description page of Project Settings.


#include "C_House2Floor.h"

void AC_House2Floor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TArray<FTransform> Instances;
	Instances.Reserve(Fwidth * Flength);


	for (int i = 0; i < Flength; ++i)
	{
		for (int j = 0; j < Fwidth; ++j)
		{
			if (i == HoleX && j == HoleY)
				continue; // 계단 구멍

			Instances.Add(FTransform(FVector(i * 400.f, j * 400.f, 302.f)));
		}
	}

	HISM_Floor->AddInstances(Instances, false);
}