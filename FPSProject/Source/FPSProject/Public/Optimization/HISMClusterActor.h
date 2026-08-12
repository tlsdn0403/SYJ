#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HISMClusterActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

UCLASS()
class FPSPROJECT_API AHISMClusterActor : public AActor
{
	GENERATED_BODY()

public:
	AHISMClusterActor();

	UPROPERTY(VisibleAnywhere, Category = "Optimization")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Optimization")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMComponents;
};
