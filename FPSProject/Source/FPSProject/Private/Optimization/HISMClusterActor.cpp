#include "Optimization/HISMClusterActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"

AHISMClusterActor::AHISMClusterActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	SceneRoot->SetMobility(EComponentMobility::Static);

	constexpr int32 ComponentCount = 10;
	HISMComponents.Reserve(ComponentCount);
	for (int32 Index = 0; Index < ComponentCount; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("HISM_%02d"), Index));
		UHierarchicalInstancedStaticMeshComponent* Component =
			CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(ComponentName);
		Component->SetupAttachment(SceneRoot);
		Component->SetMobility(EComponentMobility::Static);
		HISMComponents.Add(Component);
	}
}
