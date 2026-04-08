// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BaseZombie.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h" 
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"

ABaseZombie::ABaseZombie()
{
    PrimaryActorTick.bCanEverTick = true;

    // 醫鍮?硫붿떆 而댄룷?뚰듃瑜??앹꽦.
	ZombieMesh = GetMesh();
    //ZombieMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ZombieMeshMesh"));
    check(ZombieMesh != nullptr);

    //硫붿돩??肄쒕━???ㅼ젙
    ZombieMesh->SetCollisionProfileName(TEXT("CharacterMesh"));

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void ABaseZombie::BeginPlay()
{
    Super::BeginPlay();

    if (HealthComponent)
    {
		HealthComponent->OnDamaged.AddDynamic(this, &ABaseZombie::OnZombieDamaged); // ?곕?吏 ?낆쓣 ??OnZombieDamaged瑜??몄텧
    }

    InitializeBoneDurability();
}

void ABaseZombie::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void ABaseZombie::Attack()
{
    if (!bIsAlive || bIsAttacking) return;

    bIsAttacking = true;

    UE_LOG(LogTemp, Warning, TEXT("Zombie %s Attack!"), *GetName());

    // --- 1. 怨듦꺽 ?좊땲硫붿씠???ъ깮 ---
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && AttackMontage)
    {
        //1.0諛곗냽?쇰줈 ?좊땲硫붿씠??紐쏀?二??ъ깮
        AnimInstance->Montage_Play(AttackMontage, 1.0f);

        UE_LOG(LogTemp, Warning, TEXT("Zombie %s Montage!"), *GetName());
        // 紐쏀?二??앸굹硫?OnAttackMontageEnded ?몄텧
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &ABaseZombie::OnAttackMontageEnded);
        AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
    }
    else
    {
        // 紐쏀?二??놁쑝硫?諛붾줈 ?곕?吏 二쇨퀬 ??
        // ---  ?뚮젅?댁뼱?먭쾶 ?곕?吏 ---
        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
        if (PlayerPawn)
        {
            float Distance = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
            if (Distance <= AttackRange)
            {
                UHealthComponent* PlayerHealth = PlayerPawn->FindComponentByClass<UHealthComponent>();
                if (PlayerHealth)
                {
                    PlayerHealth->ApplyDamage(AttackDamage);
                    UE_LOG(LogTemp, Warning, TEXT("Zombie dealt %f damage!"), AttackDamage);
                }
            }
        }
        bIsAttacking = false;
    }
}


void ABaseZombie::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 紐쏀?二쇨? ?앸굹???쒖젏???곕?吏 ?곸슜
    if (!bInterrupted) // 以묐떒?섏? ?딆븯?쇰㈃
    {
        AFPSBaseCharacter* PlayerPawn = Cast<AFPSBaseCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
        if (PlayerPawn)
        {
            float Distance = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
            if (Distance <= AttackRange)
            {
                UHealthComponent* PlayerHealth = PlayerPawn->FindComponentByClass<UHealthComponent>();
                if (PlayerHealth)
                {
                    PlayerHealth->ApplyDamage(AttackDamage);
                    UE_LOG(LogTemp, Warning, TEXT("Zombie dealt %f damage!"), AttackDamage);

                    PlayerPawn->SetHealth(PlayerHealth->GetHealth(), PlayerHealth->MaxGetHealth());
                }
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("Attack missed - player moved away"));
            }
        }
    }

    bIsAttacking = false;
    UE_LOG(LogTemp, Log, TEXT("Attack Montage Ended"));
}


void ABaseZombie::OnZombieDamaged(float NewHealth, float Damage, const FHitResult& Hit)
{
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
    // 遺꾪빐 濡쒖쭅
    
    if (Hit.BoneName != NAME_None)
    {
        // 留욎? 堉덈? 二쇱슂 遺꾪빐 媛??堉??대쫫?쇰줈 蹂??
        FName TargetBone = GetParentBoneForDamage(Hit.BoneName);

        // 2堉??닿뎄??源롪린 諛?遺꾪빐 ?쒕룄
        // Hit.ImpactNormal * -1 ? 珥앹븣???좎븘??諛⑺뼢(異⑷꺽 諛⑺뼢)???섎???
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

    // ??
    BoneDurability.Add(FName("upperarm_l"), 15.0f);
    BoneDurability.Add(FName("lowerarm_l"), 10.0f);
    BoneDurability.Add(FName("upperarm_r"), 15.0f);
    BoneDurability.Add(FName("lowerarm_r"), 10.0f);

    // ?ㅻ━
    BoneDurability.Add(FName("thigh_l"), 20.0f);
    BoneDurability.Add(FName("calf_l"), 15.0f);
    BoneDurability.Add(FName("thigh_r"), 20.0f);
    BoneDurability.Add(FName("calf_r"), 15.0f);

    // 泥숈텛 (?듭뀡: ?덈━媛 ?딆뼱吏寃???寃껋씤吏)
    BoneDurability.Add(FName("spine_01"), 50.0f);
}

FName ABaseZombie::GetParentBoneForDamage(FName HitBoneName)
{
    FString BoneString = HitBoneName.ToString();

	UE_LOG(LogTemp, Log, TEXT("Hit Bone: %s"), *BoneString);
    // 癒몃━/紐?
    if (BoneString.Contains("neck") || BoneString.Contains("head")) return FName("head");

    // ?쇱そ ??怨꾩뿴
    if (BoneString.Contains("_l"))
    {
        if (BoneString.Contains("hand") || BoneString.Contains("finger") || BoneString.Contains("thumb") ||
            BoneString.Contains("index") || BoneString.Contains("middle") || BoneString.Contains("pinky") || BoneString.Contains("ring"))
        {
            return FName("lowerarm_l"); // ??留욎쑝硫??꾨옒???곕?吏濡?泥섎━
        }
        if (BoneString.Contains("lowerarm") || BoneString.Contains("twist")) return FName("lowerarm_l");
        if (BoneString.Contains("upperarm") || BoneString.Contains("clavicle")) return FName("upperarm_l");

        // ?쇱そ ?ㅻ━
        if (BoneString.Contains("foot") || BoneString.Contains("ball") || BoneString.Contains("calf")) return FName("calf_l");
        if (BoneString.Contains("thigh")) return FName("thigh_l");
    }

    // ?ㅻⅨ履???怨꾩뿴
    if (BoneString.Contains("_r"))
    {
        if (BoneString.Contains("hand") || BoneString.Contains("finger") || BoneString.Contains("thumb") ||
            BoneString.Contains("index") || BoneString.Contains("middle") || BoneString.Contains("pinky") || BoneString.Contains("ring"))
        {
            return FName("lowerarm_r");
        }
        if (BoneString.Contains("lowerarm") || BoneString.Contains("twist")) return FName("lowerarm_r");
        if (BoneString.Contains("upperarm") || BoneString.Contains("clavicle")) return FName("upperarm_r");

        // ?ㅻⅨ履??ㅻ━
        if (BoneString.Contains("foot") || BoneString.Contains("ball") || BoneString.Contains("calf")) return FName("calf_r");
        if (BoneString.Contains("thigh")) return FName("thigh_r");
    }

    // 泥숈텛/怨⑤컲
    if (BoneString.Contains("spine") || BoneString.Contains("pelvis")) return FName("spine_01");

    return HitBoneName; // 留ㅽ븨 ?덈릺硫?洹몃?濡?諛섑솚
}

void ABaseZombie::ProcessBoneDamage(FName BoneName, float Damage, FVector ImpactPoint, FVector ImpactDirection)
{
    // ?대? ?섎┛ 堉덈씪硫?臾댁떆
    if (BrokenBones.Contains(BoneName)) return;

    // ?닿뎄??由ъ뒪?몄뿉 ?덈뒗 堉덉씤吏 ?뺤씤
    if (BoneDurability.Contains(BoneName))
    {
        float CurrentBoneHealth = BoneDurability[BoneName] - Damage;
        BoneDurability[BoneName] = CurrentBoneHealth;

        UE_LOG(LogTemp, Log, TEXT("Bone: %s Health: %f"), *BoneName.ToString(), CurrentBoneHealth);

        // 堉?泥대젰?????섎㈃ 遺꾪빐
        if (CurrentBoneHealth <= 0.0f)
        {
            // 異⑷꺽??怨꾩궛 (珥앹븣 諛⑺뼢 * ??
            FVector Impulse = ImpactDirection * 300.0f; // ??議곗젅 ?꾩슂
            DismemberLimb(BoneName, Impulse, ImpactPoint);

            const bool bShouldDieImmediately =
                BoneName == FName("head") ||
                BoneName == FName("spine_01");

            if (bShouldDieImmediately && bIsAlive)
            {
                Die();
                return;
            }

            // ?섏껜媛 遺꾨━?섏뿀?쇰㈃ ?щ·留??곹깭濡??꾪솚
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

    // 遺꾪빐 泥섎━ 湲곕줉
    BrokenBones.Add(BoneName);

    // ?ㅼ젣 硫붿돩 而댄룷?뚰듃 媛?몄삤湲?(GetMesh() ?ъ슜 沅뚯옣)
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) MeshComp = ZombieMesh;

    if (MeshComp)
    {
        // 1. ?쒖빟 議곌굔 ?뚭눼 (堉덈? 臾쇰━?곸쑝濡?遺꾨━)
        MeshComp->BreakConstraint(Impulse, HitLocation, BoneName);

        MeshComp->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
        // 2. ?섎┛ 遺?꾧? 臾쇰━ ?쒕??덉씠?섏쓣 ?섎룄濡??ㅼ젙
        // ???ㅼ젙???놁쑝硫??섎┛ ?붿씠 怨듭쨷???λ뫁 ?좊떎?덈ŉ ?좊땲硫붿씠?섏쓣 怨꾩냽 ?곕씪?⑸땲??
        // SetAllBodiesBelowSimulatePhysics: ?대떦 堉??꾨옒履?紐⑤뱺 堉덈? 臾쇰━ ?쒕??덉씠?섏쑝濡??꾪솚
        MeshComp->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
		

        // 3. 臾쇰━ 異⑷꺽 媛?섍린 (?섎젮?섍컝 ???뺢꺼?섍??꾨줉)
        MeshComp->AddImpulse(Impulse, BoneName, true);

        UE_LOG(LogTemp, Warning, TEXT("Dismembered: %s"), *BoneName.ToString());
    }
}

void ABaseZombie::StartCrawling()
{
    
    if (MovementState == EZombieMovementState::Crawling) return;

    // 醫鍮꾩쓽 ?곹깭 湲곗뼱?ㅻ땲???곹깭濡?蹂寃?
    MovementState = EZombieMovementState::Crawling;

    UE_LOG(LogTemp, Warning, TEXT("Zombie %s is now CRAWLING"), *GetName());

    //  醫鍮꾧? 諛붾떏???뺣룄濡?罹≪뒓 ?ш린 以꾩씠湲?
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (Capsule)
    {
        Capsule->SetCapsuleHalfHeight(CrawlingCapsuleHalfHeight);
        Capsule->SetCapsuleRadius(CrawlingCapsuleRadius);
    }

    //  ?대룞 ?띾룄 以꾩씠湲?
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (MoveComp)
    {
        MoveComp->MaxWalkSpeed = CrawlingMaxSpeed;

        // 諛붾떏?먯꽌 ?吏곸씪 ???덈룄濡?
        MoveComp->SetMovementMode(MOVE_Walking);

        // NavMesh 湲곕컲 ?대룞?대씪硫??믪씠 ?ㅽ봽??議곗젙
        MoveComp->bOrientRotationToMovement = true;
    }

    // 罹≪뒓??以꾩뿀?쇰땲 留ㅼ돩瑜??꾨옒濡?
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        // 湲곗〈 硫붿떆 ?꾩튂?먯꽌 ?꾨옒濡??대━湲?
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
    // 二쎌쓣 ?뚮뒗 ?꾩껜 ?섍렇??    GetMesh()->SetSimulatePhysics(true);
    // 紐⑤뱺 堉덇? 臾쇰━ ?곹뼢??諛쏅룄濡?    GetMesh()->SetAllBodiesBelowSimulatePhysics(FName("pelvis"), true, true);
    SetLifeSpan(5.f);
}


