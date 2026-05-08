// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BaseZombie.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h" 
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"

ABaseZombie::ABaseZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
	NetUpdateFrequency = 30.0f;
	MinNetUpdateFrequency = 15.0f;

	// 기본 좀비 메시 컴포넌트 가져오기
	ZombieMesh = GetMesh();
	//ZombieMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ZombieMeshMesh"));
	check(ZombieMesh != nullptr);

	// 메시 콜리전 설정
	ZombieMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
	ZombieMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, TurnRateYaw, 0.0f);
		MoveComp->MaxAcceleration = MaxAcceleration;
		MoveComp->BrakingDecelerationWalking = BrakingDecelerationWalking;
		MoveComp->bCanWalkOffLedges = true;
		MoveComp->LedgeCheckThreshold = 0.0f;
	}
}

void ABaseZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseZombie, MovementState);
	DOREPLIFETIME(ABaseZombie, bIsAlive);
	DOREPLIFETIME(ABaseZombie, bIsAttacking);
}

void ABaseZombie::BeginPlay()
{
	Super::BeginPlay();

	if (ZombieMesh)
	{
		ZombieMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDamaged.AddDynamic(this, &ABaseZombie::OnZombieDamaged); // 대미지를 입을 때 OnZombieDamaged 호출
	}

	ApplyAnimationDesync();
	InitializeBoneDurability();
}

void ABaseZombie::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABaseZombie::Attack()
{
	Attack(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

void ABaseZombie::Attack(AActor* TargetActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsAlive || bIsAttacking) return;

	CurrentAttackTarget = TargetActor ? TargetActor : UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!CurrentAttackTarget)
	{
		return;
	}

	bIsAttacking = true;

	UE_LOG(LogTemp, Warning, TEXT("Zombie %s Attack!"), *GetName());

	// --- 1. 공격 애니메이션 재생 ---
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && AttackMontage)
	{
		// 1.0 배속으로 공격 몽타주 재생
		const float AttackPlayRate = FMath::Max(
			0.1f,
			AnimationRateScale * FMath::FRandRange(1.0f - AttackMontagePlayRateVariance, 1.0f + AttackMontagePlayRateVariance));
		AnimInstance->Montage_Play(AttackMontage, AttackPlayRate);

		UE_LOG(LogTemp, Warning, TEXT("Zombie %s Montage!"), *GetName());
		// 몽타주가 끝나면 OnAttackMontageEnded 호출
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABaseZombie::OnAttackMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
	}
	else
	{
		// 몽타주가 없으면 바로 대미지를 준다.
		ApplyAttackDamage(CurrentAttackTarget);
		CurrentAttackTarget = nullptr;
		bIsAttacking = false;
	}
}

FVector ABaseZombie::GetAttackPointForTarget(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return GetActorLocation();
	}

	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
	{
		FVector ClosestPoint = TargetActor->GetActorLocation();
		if (PrimitiveComponent->GetClosestPointOnCollision(GetActorLocation(), ClosestPoint) >= 0.0f)
		{
			return ClosestPoint;
		}
	}

	return TargetActor->GetActorLocation();
}

bool ABaseZombie::IsTargetInAttackRange(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return false;
	}

	const FVector AttackPoint = GetAttackPointForTarget(TargetActor);
	return FVector::Dist(GetActorLocation(), AttackPoint) <= AttackRange;
}

void ABaseZombie::ApplyDirectPursuitInput(const FVector& TargetLocation)
{
	if (!bIsAlive)
	{
		return;
	}

	const FVector Direction2D = FVector(
		TargetLocation.X - GetActorLocation().X,
		TargetLocation.Y - GetActorLocation().Y,
		0.0f).GetSafeNormal();

	if (!Direction2D.IsNearlyZero())
	{
		AddMovementInput(Direction2D, 1.0f);
	}
}

void ABaseZombie::ApplyAnimationDesync()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// 최대, 최소 애니메이션 재생 속도
	const float MinRate = FMath::Min(MinAnimationRateScale, MaxAnimationRateScale);
	const float MaxRate = FMath::Max(MinAnimationRateScale, MaxAnimationRateScale);

	// 랜덤 재생속도
	AnimationRateScale = FMath::FRandRange(MinRate, MaxRate);
	// 메쉬 전체 애니메이션 재생 속도에 적용
	MeshComp->GlobalAnimRateScale = AnimationRateScale;

	// 시작 오프셋 설정
	const float StartOffset = FMath::FRandRange(0.0f, FMath::Max(0.0f, MaxAnimationStartOffset));
	if (StartOffset > KINDA_SMALL_NUMBER && MeshComp->GetAnimInstance())
	{
		MeshComp->TickAnimation(StartOffset, false);
		MeshComp->RefreshBoneTransforms();
	}
}

void ABaseZombie::ApplyAttackDamage(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	const FVector AttackPoint = GetAttackPointForTarget(TargetActor);
	const float Distance = FVector::Dist(GetActorLocation(), AttackPoint);
	if (Distance > AttackRange)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack missed - target moved away"));
		return;
	}

	UHealthComponent* TargetHealth = TargetActor->FindComponentByClass<UHealthComponent>();
	if (!TargetHealth)
	{
		UE_LOG(LogTemp, Warning, TEXT("Zombie attacked %s, but it has no HealthComponent"), *GetNameSafe(TargetActor));
		return;
	}

	TargetHealth->ApplyDamage(AttackDamage);
	UE_LOG(LogTemp, Warning, TEXT("Zombie dealt %f damage to %s!"), AttackDamage, *GetNameSafe(TargetActor));

	if (AFPSBaseCharacter* PlayerPawn = Cast<AFPSBaseCharacter>(TargetActor))
	{
		PlayerPawn->SetHealth(TargetHealth->GetHealth(), TargetHealth->MaxGetHealth());
	}
}


void ABaseZombie::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 몽타주가 끝나는 시점에 대미지 적용
	if (!bInterrupted) // 중단되지 않았다면
	{
		AActor* TargetActor = IsValid(CurrentAttackTarget)
			? CurrentAttackTarget
			: UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		ApplyAttackDamage(TargetActor);
	}

	CurrentAttackTarget = nullptr;
	bIsAttacking = false;
	UE_LOG(LogTemp, Log, TEXT("Attack Montage Ended"));
}


void ABaseZombie::OnZombieDamaged(float NewHealth, float Damage, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Zombie Damaged: NewHealth=%f, Damage=%f, HitBone=%s"), NewHealth, Damage, *Hit.BoneName.ToString());
	FVector EffectLocation =  Hit.ImpactPoint;
 /*   if (BloodImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodImpactEffect, EffectLocation);
	}*/

	if (HitEffect)
	{
		FVector Direction = -Hit.ImpactNormal;
		FRotator Rotation = Direction.Rotation();


		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			HitEffect,
			EffectLocation,
			Rotation
		);
	}
	// 분해 로직
	
	if (Hit.BoneName != NAME_None)
	{
		// 맞은 뼈를 주요 분해 가능한 뼈 이름으로 변환
		FName TargetBone = GetParentBoneForDamage(Hit.BoneName);

		// 뼈 내구도 깎기 및 분해 시도
		// Hit.ImpactNormal * -1은 총알이 날아온 방향(충격 방향)을 의미
		ProcessBoneDamage(TargetBone, Damage, Hit.ImpactPoint, Hit.ImpactNormal * -1.0f);
	}
	if (NewHealth <= 0.f && bIsAlive)
	{
		Die();
	}

}

void ABaseZombie::InitializeBoneDurability()
{
	BoneDurability.Add(FName("head"), 10.0f);

	// 팔
	BoneDurability.Add(FName("upperarm_l"), 15.0f);
	BoneDurability.Add(FName("lowerarm_l"), 10.0f);
	BoneDurability.Add(FName("upperarm_r"), 15.0f);
	BoneDurability.Add(FName("lowerarm_r"), 10.0f);

	// 다리
	BoneDurability.Add(FName("thigh_l"), 20.0f);
	BoneDurability.Add(FName("calf_l"), 15.0f);
	BoneDurability.Add(FName("thigh_r"), 20.0f);
	BoneDurability.Add(FName("calf_r"), 15.0f);

	// 척추 (옵션: 허리가 끊어지게 할 것인지)
	BoneDurability.Add(FName("spine_01"), 50.0f);
}

FName ABaseZombie::GetParentBoneForDamage(FName HitBoneName)
{
	FString BoneString = HitBoneName.ToString();

	UE_LOG(LogTemp, Log, TEXT("Hit Bone: %s"), *BoneString);
	// 머리/목
	if (BoneString.Contains("neck") || BoneString.Contains("head")) return FName("head");

	// 왼쪽 팔 계열
	if (BoneString.Contains("_l"))
	{
		if (BoneString.Contains("hand") || BoneString.Contains("finger") || BoneString.Contains("thumb") ||
			BoneString.Contains("index") || BoneString.Contains("middle") || BoneString.Contains("pinky") || BoneString.Contains("ring"))
		{
			return FName("lowerarm_l"); // 손을 맞으면 아래팔 대미지로 처리
		}
		if (BoneString.Contains("lowerarm") || BoneString.Contains("twist")) return FName("lowerarm_l");
		if (BoneString.Contains("upperarm") || BoneString.Contains("clavicle")) return FName("upperarm_l");

		// 왼쪽 다리
		if (BoneString.Contains("foot") || BoneString.Contains("ball") || BoneString.Contains("calf")) return FName("calf_l");
		if (BoneString.Contains("thigh")) return FName("thigh_l");
	}

	// 오른쪽 팔 계열
	if (BoneString.Contains("_r"))
	{
		if (BoneString.Contains("hand") || BoneString.Contains("finger") || BoneString.Contains("thumb") ||
			BoneString.Contains("index") || BoneString.Contains("middle") || BoneString.Contains("pinky") || BoneString.Contains("ring"))
		{
			return FName("lowerarm_r");
		}
		if (BoneString.Contains("lowerarm") || BoneString.Contains("twist")) return FName("lowerarm_r");
		if (BoneString.Contains("upperarm") || BoneString.Contains("clavicle")) return FName("upperarm_r");

		// 오른쪽 다리
		if (BoneString.Contains("foot") || BoneString.Contains("ball") || BoneString.Contains("calf")) return FName("calf_r");
		if (BoneString.Contains("thigh")) return FName("thigh_r");
	}

	// 척추/골반
	if (BoneString.Contains("spine") || BoneString.Contains("pelvis")) return FName("spine_01");

	return HitBoneName; // 매핑되지 않으면 그대로 반환
}

void ABaseZombie::ProcessBoneDamage(FName BoneName, float Damage, FVector ImpactPoint, FVector ImpactDirection)
{
	// 이미 분리된 뼈라면 무시
	if (BrokenBones.Contains(BoneName)) return;

	// 내구도 리스트에 있는 뼈인지 확인
	if (BoneDurability.Contains(BoneName))
	{
		float CurrentBoneHealth = BoneDurability[BoneName] - Damage;
		BoneDurability[BoneName] = CurrentBoneHealth;

		UE_LOG(LogTemp, Log, TEXT("Bone: %s Health: %f"), *BoneName.ToString(), CurrentBoneHealth);

		// 뼈 체력이 0 이하라면 분해
		if (CurrentBoneHealth <= 0.0f)
		{
			// 충격량 계산 (총알 방향 * 세기)
			FVector Impulse = ImpactDirection * 300.0f; // 힘 조절 필요
			DismemberLimb(BoneName, Impulse, ImpactPoint);

			const bool bShouldDieImmediately =
				BoneName == FName("head") ||
				BoneName == FName("spine_01");

			if (bShouldDieImmediately && bIsAlive)
			{
				Die();
				return;
			}

			// 하체가 분리되었다면 기어가는 상태로 전환
			if (IsLegBone(BoneName) && MovementState == EZombieMovementState::Normal)
			{
				StartCrawling();
			}
		}
	   
	}
}

void ABaseZombie::DismemberLimb(FName BoneName, FVector Impulse, FVector HitLocation)
{
	if (BrokenBones.Contains(BoneName)) return;

	// 분해 처리 기록
	BrokenBones.Add(BoneName);

	// 실제 메시 컴포넌트 가져오기(GetMesh() 사용 권장)
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) MeshComp = ZombieMesh;

	if (MeshComp)
	{
		// 1. 제약 조건 파괴 (뼈를 물리적으로 분리)
		MeshComp->BreakConstraint(Impulse, HitLocation, BoneName);

		MeshComp->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
		// 2. 분리된 부위가 물리 시뮬레이션을 하도록 설정
		// 설정하지 않으면 분리된 부위가 공중에 떠서 애니메이션을 계속 따라갑니다.
		// SetAllBodiesBelowSimulatePhysics: 해당 뼈 아래쪽 모든 뼈를 물리 시뮬레이션으로 전환
		MeshComp->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
		

		// 3. 물리 충격 가하기 (잘려나간 부위가 튀어나가도록)
		MeshComp->AddImpulse(Impulse, BoneName, true);

		UE_LOG(LogTemp, Warning, TEXT("Dismembered: %s"), *BoneName.ToString());
	}
}

void ABaseZombie::StartCrawling()
{
	
	if (MovementState == EZombieMovementState::Crawling) return;

	// 좀비의 상태를 기어 다니는 상태로 변경
	MovementState = EZombieMovementState::Crawling;

	UE_LOG(LogTemp, Warning, TEXT("Zombie %s is now CRAWLING"), *GetName());

	// 좀비가 바닥에 붙도록 캡슐 크기 줄이기
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (Capsule)
	{
		Capsule->SetCapsuleHalfHeight(CrawlingCapsuleHalfHeight);
		Capsule->SetCapsuleRadius(CrawlingCapsuleRadius);
	}

	// 이동 속도 줄이기
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->MaxWalkSpeed = CrawlingMaxSpeed;

		// 바닥에서 움직일 수 있도록
		MoveComp->SetMovementMode(MOVE_Walking);

		// NavMesh 기반 이동이면 높이 오프셋 조정
		MoveComp->bOrientRotationToMovement = true;
	}

	// 캡슐을 줄였으니 메시를 아래로 내림
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp)
	{
		// 기존 메시 위치에서 아래로 내리기
		FVector CurrentOffset = MeshComp->GetRelativeLocation();
		MeshComp->SetRelativeLocation(FVector(CurrentOffset.X, CurrentOffset.Y, -CrawlingCapsuleHalfHeight));
	}
}

bool ABaseZombie::IsLegBone(FName BoneName) const
{
	static TArray<FName> LegBones = {
	   FName("thigh_l"), FName("thigh_r"),
	   FName("calf_l"), FName("calf_r")
	};
	return LegBones.Contains(BoneName);
}


void ABaseZombie::Die()
{
	if (!HasAuthority() || !bIsAlive)
	{
		return;
	}

	if (!bIsAlive) return;

	bIsAlive = false;
	MovementState = EZombieMovementState::Dead;

	UE_LOG(LogTemp, Warning, TEXT("Zombie %s Died!"), *GetName());

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);

		if (UBrainComponent* BrainComponent = AIController->GetBrainComponent())
		{
			BrainComponent->StopLogic(TEXT("Zombie died"));
		}
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(true);
		MeshComp->SetAllBodiesBelowSimulatePhysics(FName("pelvis"), true, true);
	}

	SetLifeSpan(5.f);
}


