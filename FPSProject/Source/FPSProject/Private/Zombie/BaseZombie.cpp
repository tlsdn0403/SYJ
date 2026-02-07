// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BaseZombie.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h" 
#include "Kismet/GameplayStatics.h"

ABaseZombie::ABaseZombie()
{
    PrimaryActorTick.bCanEverTick = true;

    // 좀비 메시 컴포넌트를 생성.
	ZombieMesh = GetMesh();
    //ZombieMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ZombieMeshMesh"));
    check(ZombieMesh != nullptr);

    //메쉬의 콜리전 설정
    ZombieMesh->SetCollisionProfileName(TEXT("CharacterMesh"));

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void ABaseZombie::BeginPlay()
{
    Super::BeginPlay();

    if (HealthComponent)
    {
		HealthComponent->OnDamaged.AddDynamic(this, &ABaseZombie::OnZombieDamaged); // 데미지 입을 때 OnZombieDamaged를 호출
    }

    InitializeBoneDurability();
}

void ABaseZombie::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}




void ABaseZombie::OnZombieDamaged(float NewHealth, float Damage, const FHitResult& Hit)
{
    UE_LOG(LogTemp, Warning, TEXT("Zombie Damaged: NewHealth=%f, Damage=%f, HitBone=%s"), NewHealth, Damage, *Hit.BoneName.ToString());
    FVector EffectLocation = Hit.ImpactPoint;
    if (BloodImpactEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodImpactEffect, EffectLocation);
    }

	
    // 분해 로직
    
    if (Hit.BoneName != NAME_None)
    {
        // 맞은 뼈를 주요 분해 가능 뼈 이름으로 변환
        FName TargetBone = GetParentBoneForDamage(Hit.BoneName);

        // 2뼈 내구도 깎기 및 분해 시도
        // Hit.ImpactNormal * -1 은 총알이 날아온 방향(충격 방향)을 의미함
        ProcessBoneDamage(TargetBone, Damage, Hit.ImpactPoint, Hit.ImpactNormal * -1.0f);
    }
    if (NewHealth <= 0.f && bIsAlive)
    {
        Die();
    }

}

void ABaseZombie::InitializeBoneDurability()
{
    BoneDurability.Add(FName("head"), 20.0f);

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
    BoneDurability.Add(FName("spine_01"), 30.0f);
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
            return FName("lowerarm_l"); // 손 맞으면 아래팔 데미지로 처리
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

    return HitBoneName; // 매핑 안되면 그대로 반환
}

void ABaseZombie::ProcessBoneDamage(FName BoneName, float Damage, FVector ImpactPoint, FVector ImpactDirection)
{
    // 이미 잘린 뼈라면 무시
    if (BrokenBones.Contains(BoneName)) return;

    // 내구도 리스트에 있는 뼈인지 확인
    if (BoneDurability.Contains(BoneName))
    {
        float CurrentBoneHealth = BoneDurability[BoneName] - Damage;
        BoneDurability[BoneName] = CurrentBoneHealth;

        UE_LOG(LogTemp, Log, TEXT("Bone: %s Health: %f"), *BoneName.ToString(), CurrentBoneHealth);

        // 뼈 체력이 다 되면 분해
        if (CurrentBoneHealth <= 0.0f)
        {
            // 충격량 계산 (총알 방향 * 힘)
            FVector Impulse = ImpactDirection * 300.0f; // 힘 조절 필요
            DismemberLimb(BoneName, Impulse, ImpactPoint);

            // 하체가 분리되었으면 크롤링 상태로 전환
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

    // 실제 메쉬 컴포넌트 가져오기 (GetMesh() 사용 권장)
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) MeshComp = ZombieMesh;

    if (MeshComp)
    {
        // 1. 제약 조건 파괴 (뼈를 물리적으로 분리)
        MeshComp->BreakConstraint(Impulse, HitLocation, BoneName);

        MeshComp->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
        // 2. 잘린 부위가 물리 시뮬레이션을 하도록 설정
        // 이 설정이 없으면 잘린 팔이 공중에 둥둥 떠다니며 애니메이션을 계속 따라합니다.
        // SetAllBodiesBelowSimulatePhysics: 해당 뼈 아래쪽 모든 뼈를 물리 시뮬레이션으로 전환
        MeshComp->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
		

        // 3. 물리 충격 가하기 (잘려나갈 때 튕겨나가도록)
        MeshComp->AddImpulse(Impulse, BoneName, true);

        UE_LOG(LogTemp, Warning, TEXT("Dismembered: %s"), *BoneName.ToString());
    }
}

void ABaseZombie::StartCrawling()
{
    
    if (MovementState == EZombieMovementState::Crawling) return;

    // 좀비의 상태 기어다니는 상태로 변경
    MovementState = EZombieMovementState::Crawling;

    UE_LOG(LogTemp, Warning, TEXT("Zombie %s is now CRAWLING"), *GetName());

    //  좀비가 바닥에 눕도록 캡슐 크기 줄이기
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (Capsule)
    {
        Capsule->SetCapsuleHalfHeight(CrawlingCapsuleHalfHeight);
        Capsule->SetCapsuleRadius(CrawlingCapsuleRadius);
    }

    //  이동 속도 줄이기
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (MoveComp)
    {
        MoveComp->MaxWalkSpeed = CrawlingMaxSpeed;

        // 바닥에서 움직일 수 있도록
        MoveComp->SetMovementMode(MOVE_Walking);

        // NavMesh 기반 이동이라면 높이 오프셋 조정
        MoveComp->bOrientRotationToMovement = true;
    }

    // 캡슐이 줄었으니 매쉬를 아래로
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
    if (!bIsAlive) return;

    bIsAlive = false;

    UE_LOG(LogTemp, Warning, TEXT("Zombie %s Died!"), *GetName());

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetSimulatePhysics(true);
    // 죽을 때는 전체 래그돌
    GetMesh()->SetSimulatePhysics(true);
    // 모든 뼈가 물리 영향을 받도록
    GetMesh()->SetAllBodiesBelowSimulatePhysics(FName("pelvis"), true, true);
    SetLifeSpan(5.f);
}

