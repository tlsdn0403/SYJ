// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EffectUI.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "HUD/BloodEfWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Math/UnrealMathUtility.h"


void UEffectUI::NativeConstruct()
{
	Super::NativeConstruct();

    if (BloodWidgetClass)
    {
        BloodW = CreateWidget<UBloodEfWidget>(this, BloodWidgetClass);
    }
}

void UEffectUI::PlayAni_Effect(bool re)
{
	replay = re;
	if (!B_EdgeAni)
	{
		return;
	}

	if (replay) {
       // BloodEdge->SetRenderOpacity(1.0f);
		PlayAnimation(B_EdgeAni, 0.0f, 0);
	}
	else {
       // SetAnimationCurrentTime(B_EdgeAni, 0.0f);
        StopAnimation(B_EdgeAni);
        BloodEdge->SetRenderOpacity(0.0f);
	}
}

void UEffectUI::SpawnBloodEffects(float Intensity)
{
    if (!BloodWidgetClass || !BaseCanvas || !GEngine || !GEngine->GameViewport) return;

    const float SafeIntensity = FMath::Clamp(Intensity, 0.05f, 1.0f);
    const int32 MaxBloodCount = FMath::Max(1, FMath::CeilToInt(5.0f * SafeIntensity));
    int32 BloodCount = FMath::RandRange(1, MaxBloodCount);

    for (int32 i = 0; i < BloodCount; ++i)
    {
        UBloodEfWidget* BloodWidget = CreateWidget<UBloodEfWidget>(GetWorld(), BloodWidgetClass);
        if (!BloodWidget) continue;

        BaseCanvas->AddChild(BloodWidget);

        UCanvasPanelSlot* bloodSlot = Cast<UCanvasPanelSlot>(BloodWidget->Slot);
        if (bloodSlot)
        {
            FVector2D ViewportSize;
            GEngine->GameViewport->GetViewportSize(ViewportSize);

            const float RandomX = FMath::RandRange(0.0f, static_cast<float>(ViewportSize.X));
            const float RandomY = FMath::RandRange(0.0f, static_cast<float>(ViewportSize.Y));

            bloodSlot->SetPosition(FVector2D(RandomX, RandomY));
            bloodSlot->SetAutoSize(true);
        }

        float RandomAngle = FMath::RandRange(0.0f, 360.0f);
        float RandomScale = FMath::RandRange(0.7f, 1.3f);

        FWidgetTransform Transform;
        Transform.Angle = RandomAngle;
        Transform.Scale = FVector2D(RandomScale, RandomScale);

        BloodWidget->SetRenderTransform(Transform);
        BloodWidget->SetRenderOpacity(SafeIntensity);
        BloodWidget->PlayAni_Ef();
    }
}
