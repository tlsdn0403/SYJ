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
#include "Truck/Truck.h"
#include "Zombie/ZombieFallZone.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
void SetAnimFloatIfPresent(UAnimInstance* AnimInstance, const TCHAR* PropertyName, float Value)
{
	if (AnimInstance == nullptr)
	{
		return;
	}

	if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(AnimInstance->GetClass(), PropertyName))
	{
		FloatProperty->SetPropertyValue_InContainer(AnimInstance, Value);
	}
}

void SetAnimBoolIfPresent(UAnimInstance* AnimInstance, const TCHAR* PropertyName, bool bValue)
{
	if (AnimInstance == nullptr)
	{
		return;
	}

	if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(AnimInstance->GetClass(), PropertyName))
	{
		BoolProperty->SetPropertyValue_InContainer(AnimInstance, bValue);
	}
}
}

ABaseZombie::ABaseZombie()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(15.0f);

	// 기본 좀비 메시 컴포넌트 가져오기
	ZombieMesh = GetMesh();
	//ZombieMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ZombieMeshMesh"));
	check(ZombieMesh != nullptr);

	// 메시 콜리전 설정
	ZombieMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
	ZombieMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ZombieMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultHitEffect(
		TEXT("/Script/Niagara.NiagaraSystem'/Game/Niagara/NS_BloodEffect.NS_BloodEffect'"));
	if (DefaultHitEffect.Succeeded())
	{
		HitEffect = DefaultHitEffect.Object;
	}

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, TurnRateYaw, 0.0f);
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
		StandingMeshRelativeLocation = ZombieMesh->GetRelativeLocation();
		bHasStandingMeshRelativeLocation = true;

		ZombieMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		ZombieMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

		if (!ZombieMesh->GetAnimClass() && ZombieAnimClass)
		{
			ZombieMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			ZombieMesh->SetAnimInstanceClass(ZombieAnimClass);
		}
	}

	if (HealthComponent)
	{
		HealthComponent->OnDamaged.AddDynamic(this, &ABaseZombie::OnZombieDamaged); // 대미지를 입을 때 OnZombieDamaged 호출
	}

	ApplyAnimationDesync();
	ApplyMovementTuning();
	InitializeBoneDurability();
}

float ABaseZombie::GetDirectAttackAnimationDuration()
{
	return 0.0f;
}

float ABaseZombie::PlayDeathAnimationBeforeRagdoll()
{
	return 0.0f;
}

FVector ABaseZombie::GetCrawlingMeshRelativeLocation(const FVector& CurrentStandingMeshRelativeLocation) const
{
	return CurrentStandingMeshRelativeLocation;
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

	StartAttack(TargetActor, true);
}

void ABaseZombie::HandleNetworkAttack(AActor* TargetActor, bool bShouldApplyDamage)
{
	if (bIsAttacking || MovementState == EZombieMovementState::Crawling)
	{
		return;
	}

	bShouldApplyCurrentNetworkAttackDamage = bShouldApplyDamage;
	StartAttack(TargetActor, false);
}

void ABaseZombie::HandleNetworkHit(float NewHealth, float MaxHealth)
{
	if (!bIsAlive || HealthComponent == nullptr)
	{
		return;
	}

	const float CurrentHealth = HealthComponent->GetHealth();
	const float ClampedNewHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth > 0.0f ? MaxHealth : CurrentHealth);
	const float Damage = FMath::Max(CurrentHealth - ClampedNewHealth, 0.0f);
	if (Damage > 0.0f)
	{
		HealthComponent->ApplyDamage(Damage);
	}

	if (HitEffect)
	{
		const FVector EffectLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		const FRotator Rotation = (-GetActorForwardVector()).Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitEffect, EffectLocation, Rotation);
	}
}

void ABaseZombie::HandleNetworkDeath()
{
	if (!bIsAlive)
	{
		return;
	}

	bIsAlive = false;
	bIsAttacking = false;
	bAttackDamageApplied = false;
	bShouldApplyCurrentNetworkAttackDamage = true;
	CurrentAttackTarget = nullptr;
	MovementState = EZombieMovementState::Dead;
	GetWorldTimerManager().ClearTimer(AttackDamageTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	StartDeathVisual();
}

void ABaseZombie::HandleNetworkDismember(FName BoneName, const FVector& Impulse, const FVector& HitLocation)
{
	if (BoneName == NAME_None)
	{
		return;
	}

	DismemberLimb(BoneName, Impulse, HitLocation);
	if (IsLegBone(BoneName) && MovementState == EZombieMovementState::Normal)
	{
		StartCrawling();
	}
}

void ABaseZombie::SetNetworkMoveTarget(const FVector& TargetLocation, const FRotator& TargetRotation, bool bInIsMoving)
{
	if (!bIsAlive || IsActorBeingDestroyed() || TargetLocation.ContainsNaN() || TargetRotation.ContainsNaN())
	{
		return;
	}

	const FVector PreviousLocation = GetActorLocation();

	NetworkTargetLocation = TargetLocation;
	NetworkTargetRotation = TargetRotation;
	bNetworkTargetIsMoving = bInIsMoving;
	bHasNetworkMoveTarget = false;

	if (!SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!IsValid(MoveComp))
	{
		return;
	}

	float AnimSpeed = 0.0f;

	if (!bInIsMoving)
	{
		MoveComp->Velocity = FVector::ZeroVector;
	}
	else
	{
		constexpr float ZombieServerTickSeconds = 0.1f;
		constexpr float NetworkZombieFallbackMaxSpeed = 180.0f;
		FVector PacketVelocity = (TargetLocation - PreviousLocation) / ZombieServerTickSeconds;
		const float MaxAnimSpeed = MoveComp->GetMaxSpeed() > 0.0f
			? MoveComp->GetMaxSpeed()
			: NetworkZombieFallbackMaxSpeed;
		PacketVelocity = PacketVelocity.GetClampedToMaxSize(MaxAnimSpeed);
		MoveComp->Velocity = PacketVelocity;
		AnimSpeed = PacketVelocity.Size2D();
	}

	MoveComp->UpdateComponentVelocity();

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!IsValid(MeshComp) || MeshComp->IsSimulatingPhysics())
	{
		return;
	}

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return;
	}

	SetAnimFloatIfPresent(AnimInstance, TEXT("Speed"), AnimSpeed);
	SetAnimFloatIfPresent(AnimInstance, TEXT("GroundSpeed"), AnimSpeed);
	SetAnimBoolIfPresent(AnimInstance, TEXT("IsMoving"), bInIsMoving);
	SetAnimBoolIfPresent(AnimInstance, TEXT("bIsMoving"), bInIsMoving);
	SetAnimBoolIfPresent(AnimInstance, TEXT("HasAcceleration"), bInIsMoving);
	SetAnimBoolIfPresent(AnimInstance, TEXT("bHasAcceleration"), bInIsMoving);
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

void ABaseZombie::FaceAttackTarget(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	FVector Direction = GetAttackPointForTarget(TargetActor) - GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FRotator FacingRotation(0.0f, Direction.Rotation().Yaw, 0.0f);
	SetActorRotation(FacingRotation, ETeleportType::None);

	if (AController* ZombieController = GetController())
	{
		ZombieController->SetControlRotation(FacingRotation);
	}
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

bool ABaseZombie::TryGetFallZonePursuitLocation(AActor* TargetActor, FVector& OutApproachLocation, FVector& OutCommitLocation)
{
	if (!bIsAlive || !TargetActor)
	{
		return false;
	}

	// 규모가 큰 프로젝트가 아니라면 좀비마다 활성 Zone을 캐시하지 않고
	// 등록된 Zone 전체를 훑어 가장 적합한 경로를 고르는 편이 더 읽기 쉽다.
	TArray<AZombieFallZone*> RegisteredZones;
	AZombieFallZone::GetRegisteredFallZones(GetWorld(), RegisteredZones);

	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundZone = false;
	for (AZombieFallZone* FallZone : RegisteredZones)
	{
		if (!FallZone)
		{
			continue;
		}

		// 점수가 가장 높은 Zone 하나만 선택해 추격 경로로 사용한다.
		FVector CandidateApproachLocation = FVector::ZeroVector;
		FVector CandidateCommitLocation = FVector::ZeroVector;
		float CandidateScore = 0.0f;
		if (FallZone->CanGuideZombieTowardTarget(this, TargetActor, CandidateApproachLocation, CandidateCommitLocation, CandidateScore) &&
			CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			OutApproachLocation = CandidateApproachLocation;
			OutCommitLocation = CandidateCommitLocation;
			bFoundZone = true;
		}
	}

	return bFoundZone;
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

void ABaseZombie::ApplyMovementTuning()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, TurnRateYaw, 0.0f);
	MoveComp->MaxAcceleration = MaxAcceleration;
	MoveComp->BrakingDecelerationWalking = BrakingDecelerationWalking;
	MoveComp->bRequestedMoveUseAcceleration = true;
	ApplyAvoidanceTuning();
}

void ABaseZombie::ApplyAvoidanceTuning()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	MoveComp->bUseRVOAvoidance = bUseRVOAvoidance;
	const float DesiredAvoidanceRadius = MovementState == EZombieMovementState::Crawling
		? CrawlingAvoidanceConsiderationRadius
		: AvoidanceConsiderationRadius;
	const float MaxUsefulAvoidanceRadius = MovementState == EZombieMovementState::Crawling ? 70.0f : 120.0f;
	MoveComp->AvoidanceConsiderationRadius = FMath::Min(DesiredAvoidanceRadius, MaxUsefulAvoidanceRadius);
	MoveComp->AvoidanceWeight = AvoidanceWeight;
}

AActor* ABaseZombie::ResolveAttackDamageTarget() const
{
	AActor* TargetActor = IsValid(CurrentAttackTarget)
		? CurrentAttackTarget
		: (NetworkObjectId == 0 ? UGameplayStatics::GetPlayerPawn(GetWorld(), 0) : nullptr);

	if (NetworkObjectId != 0)
	{
		if (!bShouldApplyCurrentNetworkAttackDamage)
		{
			return nullptr;
		}

		if (TargetActor && TargetActor->IsA<ATruck>())
		{
			return TargetActor;
		}

		AFPSBaseCharacter* TargetPlayer = Cast<AFPSBaseCharacter>(TargetActor);
		if (TargetPlayer == nullptr || !TargetPlayer->IsLocallyControlled())
		{
			TargetActor = nullptr;
		}
	}

	return TargetActor;
}

UAnimMontage* ABaseZombie::GetAttackMontageForCurrentState() const
{
	if (MovementState == EZombieMovementState::Crawling)
	{
		return nullptr;
	}

	return AttackMontage;
}

void ABaseZombie::StartAttack(AActor* TargetActor, bool bAllowFallbackTarget)
{
	if (!bIsAlive || bIsAttacking || MovementState == EZombieMovementState::Crawling)
	{
		return;
	}

	CurrentAttackTarget = TargetActor;
	if (!CurrentAttackTarget && bAllowFallbackTarget)
	{
		CurrentAttackTarget = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	}

	if (!CurrentAttackTarget)
	{
		return;
	}

	bIsAttacking = true;
	bAttackDamageApplied = false;
	GetWorldTimerManager().ClearTimer(AttackDamageTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->bUseRVOAvoidance = false;
		MoveComp->bOrientRotationToMovement = false;
	}

	FaceAttackTarget(CurrentAttackTarget);

	UE_LOG(LogTemp, Verbose, TEXT("Zombie %s Attack! State=%d"), *GetName(), static_cast<int32>(MovementState));

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* SelectedAttackMontage = GetAttackMontageForCurrentState();
	if (AnimInstance && SelectedAttackMontage)
	{
		const float AttackPlayRate = FMath::Max(
			0.1f,
			AnimationRateScale * FMath::FRandRange(1.0f - AttackMontagePlayRateVariance, 1.0f + AttackMontagePlayRateVariance));

		const float MontageDuration = AnimInstance->Montage_Play(SelectedAttackMontage, AttackPlayRate, EMontagePlayReturnType::Duration);
		if (MontageDuration > 0.0f)
		{
			UE_LOG(LogTemp, Verbose, TEXT("Zombie %s Attack Montage: %s"), *GetName(), *GetNameSafe(SelectedAttackMontage));
			ScheduleAttackDamage(MontageDuration);

			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &ABaseZombie::OnAttackMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, SelectedAttackMontage);
			return;
		}
	}

	const float DirectAnimationDuration = GetDirectAttackAnimationDuration();
	const float FallbackDuration = DirectAnimationDuration > KINDA_SMALL_NUMBER
		? DirectAnimationDuration
		: FMath::Max(0.0f, FallbackAttackDuration);
	ScheduleAttackDamage(FallbackDuration);
	if (FallbackDuration <= KINDA_SMALL_NUMBER)
	{
		FinishAttack();
		return;
	}

	GetWorldTimerManager().SetTimer(
		AttackFinishTimerHandle,
		this,
		&ABaseZombie::FinishAttack,
		FallbackDuration,
		false);
}

void ABaseZombie::ScheduleAttackDamage(float MontageDuration)
{
	bAttackDamageApplied = false;
	GetWorldTimerManager().ClearTimer(AttackDamageTimerHandle);

	// 얼마나 딜레이 될건지 구함 AttackDamageTimeRatio는 현재 0.23으로 설정
	const float DamageDelay = FMath::Max(0.0f, MontageDuration) * FMath::Clamp(AttackDamageTimeRatio, 0.0f, 0.95f);
	if (DamageDelay <= KINDA_SMALL_NUMBER)
	{
		TriggerAttackDamage();
		return;
	}

	GetWorldTimerManager().SetTimer(
		AttackDamageTimerHandle,
		this,
		&ABaseZombie::TriggerAttackDamage,
		DamageDelay,
		false);
}

void ABaseZombie::TriggerAttackDamage()
{
	if (bAttackDamageApplied || !bIsAlive || !bIsAttacking)
	{
		return;
	}

	bAttackDamageApplied = true;
	ApplyAttackDamage(ResolveAttackDamageTarget());
}

void ABaseZombie::FinishAttack()
{
	GetWorldTimerManager().ClearTimer(AttackDamageTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);
	CurrentAttackTarget = nullptr;
	bIsAttacking = false;
	bAttackDamageApplied = false;
	bShouldApplyCurrentNetworkAttackDamage = true;
	ApplyAvoidanceTuning();
	UE_LOG(LogTemp, Verbose, TEXT("Attack Finished"));
}

void ABaseZombie::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(AttackDamageTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(0.1f);
	}

	CurrentAttackTarget = nullptr;
	bIsAttacking = false;
	bAttackDamageApplied = false;
	bShouldApplyCurrentNetworkAttackDamage = true;
}

void ABaseZombie::ApplyAttackDamage(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	const FVector AttackPoint = GetAttackPointForTarget(TargetActor);
	const float Distance = FVector::Dist(GetActorLocation(), AttackPoint);
	const bool bServerConfirmedTruckAttack = NetworkObjectId != 0 && TargetActor->IsA<ATruck>();
	if (Distance > AttackRange && !bServerConfirmedTruckAttack)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Attack missed - target moved away"));
		return;
	}

	UHealthComponent* TargetHealth = TargetActor->FindComponentByClass<UHealthComponent>();
	if (!TargetHealth)
	{
		UE_LOG(LogTemp, Warning, TEXT("Zombie attacked %s, but it has no HealthComponent"), *GetNameSafe(TargetActor));
		return;
	}

	AFPSBaseCharacter* PlayerPawn = Cast<AFPSBaseCharacter>(TargetActor);
	if (PlayerPawn)
	{
		TargetHealth->ApplyDamageSilently(AttackDamage);
	}
	else
	{
		TargetHealth->ApplyDamage(AttackDamage);
	}
	UE_LOG(LogTemp, Verbose, TEXT("Zombie dealt %f damage to %s!"), AttackDamage, *GetNameSafe(TargetActor));

	if (PlayerPawn)
	{
		PlayerPawn->SetHealth(TargetHealth->GetHealth(), TargetHealth->MaxGetHealth());
		if (TargetHealth->GetHealth() <= 0.0f)
		{
			PlayerPawn->Die();
		}
	}
}


void ABaseZombie::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	FinishAttack();
}


void ABaseZombie::OnZombieDamaged(float NewHealth, float Damage, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("Zombie Damaged: NewHealth=%f, Damage=%f, HitBone=%s"), NewHealth, Damage, *Hit.BoneName.ToString());
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
	ResetDismemberBones();

	RegisterDismemberBone(FName("head"), 10.0f);

	// 팔
	RegisterDismemberBone(FName("upperarm_l"), 15.0f);
	RegisterDismemberBone(FName("lowerarm_l"), 10.0f);
	RegisterDismemberBone(FName("upperarm_r"), 15.0f);
	RegisterDismemberBone(FName("lowerarm_r"), 10.0f);

	// 다리
	RegisterDismemberBone(FName("thigh_l"), 20.0f);
	RegisterDismemberBone(FName("calf_l"), 15.0f);
	RegisterDismemberBone(FName("thigh_r"), 20.0f);
	RegisterDismemberBone(FName("calf_r"), 15.0f);

	// 척추 (옵션: 허리가 끊어지게 할 것인지)
	RegisterDismemberBone(FName("spine_01"), 50.0f);
}

void ABaseZombie::ResetDismemberBones()
{
	BoneDurability.Reset();
	BrokenBones.Reset();
}

void ABaseZombie::RegisterDismemberBone(FName BoneName, float Durability)
{
	if (BoneName != NAME_None && Durability > 0.0f)
	{
		BoneDurability.Add(BoneName, Durability);
	}
}

FName ABaseZombie::GetParentBoneForDamage(FName HitBoneName) const
{
	FString BoneString = HitBoneName.ToString();

	UE_LOG(LogTemp, Verbose, TEXT("Hit Bone: %s"), *BoneString);
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

FName ABaseZombie::GetPhysicsRootBoneName() const
{
	return FName("pelvis");
}

bool ABaseZombie::IsFatalDismemberBone(FName BoneName) const
{
	return BoneName == FName("head") || BoneName == FName("spine_01");
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

		UE_LOG(LogTemp, Verbose, TEXT("Bone: %s Health: %f"), *BoneName.ToString(), CurrentBoneHealth);

		// 뼈 체력이 0 이하라면 분해
		if (CurrentBoneHealth <= 0.0f)
		{
			// 충격량 계산 (총알 방향 * 세기)
			FVector Impulse = ImpactDirection * 300.0f; // 힘 조절 필요
			DismemberLimb(BoneName, Impulse, ImpactPoint);

			if (IsFatalDismemberBone(BoneName) && bIsAlive)
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
		SpawnDismemberChunk(BoneName, Impulse, HitLocation);

		// 살아있는 Character의 메인 메시에서는 절단 부위 물리 바디를 제거한다.
		// 물리 시뮬레이션은 별도 조각 액터에서만 처리해야 캡슐/CharacterMovement와 충돌하지 않는다.
		MeshComp->HideBoneByName(BoneName, EPhysBodyOp::PBO_Term);

		UE_LOG(LogTemp, Verbose, TEXT("Dismembered: %s"), *BoneName.ToString());
	}
}

void ABaseZombie::SpawnDismemberChunk(FName BoneName, const FVector& Impulse, const FVector& HitLocation)
{
	UWorld* World = GetWorld();
	if (World == nullptr || BoneName == NAME_None)
	{
		return;
	}

	TSubclassOf<AActor> ChunkClass = DefaultDismemberChunkClass;
	if (const TSubclassOf<AActor>* FoundClass = DismemberChunkClasses.Find(BoneName))
	{
		ChunkClass = *FoundClass;
	}

	if (!ChunkClass)
	{
		return;
	}

	FVector SpawnLocation = HitLocation;
	FRotator SpawnRotation = GetActorRotation();
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->GetBoneIndex(BoneName) != INDEX_NONE)
		{
			SpawnLocation = MeshComp->GetBoneLocation(BoneName);
			SpawnRotation = MeshComp->GetBoneQuaternion(BoneName).Rotator();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* ChunkActor = World->SpawnActor<AActor>(ChunkClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (ChunkActor == nullptr)
	{
		return;
	}
	if (DismemberChunkLifeSpan > 0.0f)
	{
		ChunkActor->SetLifeSpan(DismemberChunkLifeSpan);
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	ChunkActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent == nullptr)
		{
			continue;
		}

		PrimitiveComponent->SetSimulatePhysics(true);
		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PrimitiveComponent->IgnoreActorWhenMoving(this, true);
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			PrimitiveComponent->IgnoreComponentWhenMoving(Capsule, true);
		}
		PrimitiveComponent->AddImpulse(Impulse, NAME_None, true);
	}
}

void ABaseZombie::StartCrawling()
{
	
	if (MovementState == EZombieMovementState::Crawling) return;

	CancelAttack();

	// 좀비의 상태를 기어 다니는 상태로 변경
	MovementState = EZombieMovementState::Crawling;

	UE_LOG(LogTemp, Verbose, TEXT("Zombie %s is now CRAWLING"), *GetName());

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
		ApplyAvoidanceTuning();
	}

	// Preserve the blueprint-authored mesh offset. Forcing this to the new capsule half height
	// lifts the zombie mesh after leg dismemberment, especially in crawling animations.
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp)
	{
		if (bHasStandingMeshRelativeLocation)
		{
			MeshComp->SetRelativeLocation(GetCrawlingMeshRelativeLocation(StandingMeshRelativeLocation));
		}
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
	bIsAttacking = false;
	bAttackDamageApplied = false;
	MovementState = EZombieMovementState::Dead;
	CurrentAttackTarget = nullptr;
	GetWorldTimerManager().ClearTimer(AttackDamageTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);

	UE_LOG(LogTemp, Verbose, TEXT("Zombie %s Died!"), *GetName());

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

	StartDeathVisual();
}

void ABaseZombie::StartDeathVisual()
{
	GetWorldTimerManager().ClearTimer(DeathRagdollTimerHandle);
	const float DeathAnimationDuration = FMath::Max(0.0f, PlayDeathAnimationBeforeRagdoll());
	if (DeathAnimationDuration > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(
			DeathRagdollTimerHandle,
			this,
			&ABaseZombie::EnableDeathRagdoll,
			DeathAnimationDuration,
			false);
	}
	else
	{
		EnableDeathRagdoll();
	}

	SetLifeSpan(DeathAnimationDuration + 5.0f);
}

void ABaseZombie::EnableDeathRagdoll()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(true);
		MeshComp->SetAllBodiesBelowSimulatePhysics(GetPhysicsRootBoneName(), true, true);
	}
}
