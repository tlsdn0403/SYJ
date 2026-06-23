#pragma once

#include "CoreMinimal.h"
#include "Zombie/BaseZombie.h"
#include "MixamoZombie.generated.h"

class UAnimSequenceBase;

/**
 * Zombie that drives Mixamo animation sequences directly from C++.
 * No Animation Blueprint is required. All sequences assigned to an instance
 * must use the same skeleton as its skeletal mesh.
 */
UCLASS(Blueprintable)
class FPSPROJECT_API AMixamoZombie : public ABaseZombie
{
	GENERATED_BODY()

public:
	AMixamoZombie();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual float GetDirectAttackAnimationDuration() override;
	virtual float PlayDeathAnimationBeforeRagdoll() override;
	virtual void InitializeBoneDurability() override;
	virtual FName GetParentBoneForDamage(FName HitBoneName) const override;
	virtual FName GetPhysicsRootBoneName() const override;
	virtual bool IsFatalDismemberBone(FName BoneName) const override;
	virtual bool IsLegBone(FName BoneName) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	bool bUseDirectAnimation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	TObjectPtr<UAnimSequenceBase> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	TObjectPtr<UAnimSequenceBase> CrawlAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	TArray<TObjectPtr<UAnimSequenceBase>> AttackAnimations;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	TObjectPtr<UAnimSequenceBase> CrawlingAttackAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Direct Animation", meta = (ClampMin = "0.0"))
	float MovingSpeedThreshold = 3.0f;

private:
	enum class EDirectAnimationState : uint8
	{
		None,
		Idle,
		Walking,
		Crawling,
		Attacking,
		CrawlingAttacking,
		Dead
	};

	void UpdateDirectAnimation();
	void PlayDirectAnimation(EDirectAnimationState NewState);
	UAnimSequenceBase* GetAnimationForState(EDirectAnimationState State) const;
	UAnimSequenceBase* ChooseAttackAnimation();
	bool IsAnimationCompatible(const UAnimSequenceBase* Animation) const;
	float GetAnimationDuration(const UAnimSequenceBase* Animation) const;

	EDirectAnimationState CurrentDirectState = EDirectAnimationState::None;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentAttackAnimation;
};
