// Fill out your copyright notice in the Description page of Project Settings.


#include "Truck/Truck.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ATruck::ATruck()
{
	// 트리거 컴포넌트 생성 및 부착
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(RootComponent);
	InteractTrigger->InitSphereRadius(200.0f); // 범위 설정

	//오디오 컴포넌트 생성 및 부착
	EngineAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudioComponent->SetupAttachment(RootComponent);
	EngineAudioComponent->bAutoActivate = false; // 처음엔 꺼둠 (탑승 시 켤 예정)

	// 짐칸 기준점 생성
	CargoOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("CargoOrigin"));
	CargoOrigin->SetupAttachment(RootComponent);
	// 트럭 뒷부분 짐칸 위치로 설정 (에디터에서 미세 조정 )
	CargoOrigin->SetRelativeLocation(FVector(-120.0f, 0.0f, 80.0f));



	// AmmoSlots 
	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("AmmoSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		AmmoSlots.Add(NewSlot);
	}

	// FuelSlots 
	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("FuelSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		FuelSlots.Add(NewSlot);
	}

	// MedKitSlots
	for (int32 i = 0; i < 3; i++)
	{
		FName SlotName = FName(*FString::Printf(TEXT("MedKitSlot_%d"), i));
		UStaticMeshComponent* NewSlot = CreateDefaultSubobject<UStaticMeshComponent>(SlotName);
		NewSlot->SetupAttachment(CargoOrigin);
		MedKitSlots.Add(NewSlot);
	}
}

void ATruck::BeginPlay()
{
	Super::BeginPlay();

	// 엔진 사운드 설정이 되어있다면 컴포넌트에 할당
	if (EngineSoundCue)
	{
		EngineAudioComponent->SetSound(EngineSoundCue);
	}


	// 게임 시작 시, 미리 배치된 모든 짐을 일단 숨깁니다.
	for (UStaticMeshComponent* Slot : AmmoSlots) { if (Slot) Slot->SetVisibility(false); }
	for (UStaticMeshComponent* Slot : FuelSlots) { if (Slot) Slot->SetVisibility(false); }
	for (UStaticMeshComponent* Slot : MedKitSlots) { if (Slot) Slot->SetVisibility(false); }
}

void ATruck::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsPlayerControlled())
	{
		// 엔진 소리가 꺼져있다면 켬 (시동)
		if (!EngineAudioComponent->IsPlaying())
		{
			EngineAudioComponent->Play();
		}

		UpdateEngineSound();
		UpdateBrakeSound();
	}
	else
	{
		// 내리면 엔진 소리 끔
		if (EngineAudioComponent->IsPlaying())
		{
			EngineAudioComponent->Stop();
		}
	}
}

void ATruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("Throttle", this, &ATruck::MoveForward);   // 가속/후진
	PlayerInputComponent->BindAxis("Steer", this, &ATruck::MoveRight);       // 핸들 좌우
	PlayerInputComponent->BindAxis("Brake", this, &ATruck::Brake);           // 브레이크
}

void ATruck::MoveForward(float Value)
{
	 UE_LOG(LogTemp, Warning, TEXT("Throttle Input: %f"), Value);
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(Value);
	}
}

void ATruck::MoveRight(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetSteeringInput(Value);
	}
}

void ATruck::Brake(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetBrakeInput(Value);

		// 브레이크 소리 로직
		// 속도가 좀 있고(시속 10km 이상), 브레이크를 밟았고, 소리가 안 나는 중이면 재생
		float Speed = GetVelocity().Size();
		if (Value > 0.5f && Speed > 300.0f && !bIsBrakingSoundPlaying)
		{
			if (BrakeSound)
			{
				// 소리 재생
				UGameplayStatics::PlaySoundAtLocation(this, BrakeSound, GetActorLocation());
				bIsBrakingSoundPlaying = true;

				FTimerHandle Handle;
				GetWorld()->GetTimerManager().SetTimer(Handle, [this]() {
					bIsBrakingSoundPlaying = false;
					}, 1.0f, false);
			}
		}
	}
}

// -------------------------------트럭 아이템 적재 시각화 --------------------------------
// 
// 아이템 시각적 적재
void ATruck::AddCargoVisual(EItemType ItemType)
{
	UStaticMeshComponent* TargetSlot = nullptr;

	// 어떤 아이템인지 확인하고, 해당 배열에서 빈 슬롯(숨겨진 메시)을 찾습니다.
	switch (ItemType)
	{
	case EItemType::Ammo:
		if (CurrentAmmoCount < AmmoSlots.Num())
		{
			TargetSlot = AmmoSlots[CurrentAmmoCount];
			CurrentAmmoCount++;
		}
		break;

	case EItemType::Fuel:
		if (CurrentFuelCount < FuelSlots.Num())
		{
			TargetSlot = FuelSlots[CurrentFuelCount];
			CurrentFuelCount++;
		}
		break;

	case EItemType::MedicalKit:
		if (CurrentMedKitCount < MedKitSlots.Num())
		{
			TargetSlot = MedKitSlots[CurrentMedKitCount];
			CurrentMedKitCount++;
		}
		break;
	}

	// 빈 슬롯을 찾았다면 화면에 보이게 켭니다!
	if (TargetSlot)
	{
		TargetSlot->SetVisibility(true);
		UE_LOG(LogTemp, Log, TEXT("Cargo loaded visually at slot. Type: %d"), (int32)ItemType);
	}
	else
	{
		// 준비해둔 슬롯보다 아이템을 많이 넣었을 경우
		UE_LOG(LogTemp, Warning, TEXT("No empty slots left for item type %d!"), (int32)ItemType);
	}
}

// -------------------------------------------------------------------------------------

// -------------------------------인터랙션 구현 --------------------------------

void ATruck::Interact_Implementation(AFPSBaseCharacter* Character)
{
	if (!Character) return;

	//------------------------------------ 1 Stage ------------------------------------
	if (bIsLoadingPhase) {
		if (Character->GetItemCount() > 0)
		{
			// 짐을 받음 (배열을 통째로 받아옴)
			TArray<EItemType> ReceivedItems = Character->OffloadItems();

			// 받은 아이템 분석 (예: 종류별로 점수 계산)
			for (EItemType Item : ReceivedItems)
			{
				TotalLoadedItems++;

				// 시각적 적재
				AddCargoVisual(Item);

				// 아이템별 처리
				switch (Item)
				{
				case EItemType::Ammo:
					UE_LOG(LogTemp, Log, TEXT("Loaded Ammo box"));
					break;
				case EItemType::Fuel:
					UE_LOG(LogTemp, Log, TEXT("Loaded Fuel can"));
					break;
				case EItemType::MedicalKit:
					UE_LOG(LogTemp, Log, TEXT("Loaded Medical Kit"));
					break;
				default:
					break;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("Offloaded %d items to Truck!"), ReceivedItems.Num());
		
			
		}
		// 적재 사운드 재생
		if (LoadItemSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, LoadItemSound, GetActorLocation());
		}


		// 1스테이지 임으로 트럭운전으로 넘어가지 않게
		return;
	}


	// 캐릭터를 조종하던 컨트롤러를 가져옴
	AController* PlayerController = Character->GetController();

	if (PlayerController)
	{
		//	빙의대상을 캐릭터에서 트럭으로 변경
		PlayerController->Possess(this);


		UE_LOG(LogTemp, Log, TEXT("Truck Possessed!"));
	}
}

void ATruck::UpdateEngineSound()
{
	auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (MoveComp)
	{
		// 현재 엔진 회전수(RPM) 가져오기
		float CurrentRPM = MoveComp->GetEngineRotationSpeed();

		UE_LOG(LogTemp, Warning, TEXT("CurrentRPM: %f"), CurrentRPM);
		// Sound Cue에서 만든 "RPM" 파라미터 값 업데이트
		EngineAudioComponent->SetFloatParameter(TEXT("RPM"), CurrentRPM);
	}
}

void ATruck::UpdateBrakeSound()
{
	auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!MoveComp) return;

	// 속도가 어느정도 있을 때
	bool bIsMoving = FMath::Abs(GetVelocity().Size()) > 100.0f;
}
