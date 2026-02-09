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
}

void ATruck::BeginPlay()
{
	Super::BeginPlay();

	// 엔진 사운드 설정이 되어있다면 컴포넌트에 할당
	if (EngineSoundCue)
	{
		EngineAudioComponent->SetSound(EngineSoundCue);
	}
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
// 적재 위치 계산
FVector ATruck::CalculateCargoPosition(int32 ItemIndex) const
{
	// 층 (Layer), 행 (Row), 열 (Column) 계산
	int32 Layer = ItemIndex / ItemsPerLayer;        // 몇 번째 층인지
	int32 IndexInLayer = ItemIndex % ItemsPerLayer; // 해당 층에서 몇 번째인지
	int32 Row = IndexInLayer / ItemsPerRow;         // 행
	int32 Col = IndexInLayer % ItemsPerRow;         // 열

	// 간격 계산
	float ColSpacing = CargoWidth / FMath::Max(ItemsPerRow - 1, 1);
	float RowSpacing = CargoDepth / FMath::Max((ItemsPerLayer / ItemsPerRow) - 1, 1);

	// 중심 기준으로 좌우, 앞뒤 오프셋
	float X = -Row * RowSpacing + (CargoDepth * 0.5f);    // 앞뒤 (트럭 길이 방향)
	float Y = Col * ColSpacing - (CargoWidth * 0.5f);     // 좌우
	float Z = Layer * ItemHeight;                          // 높이 (쌓기)

	return FVector(X, Y, Z);
}

//  아이템 시각적 적재
void ATruck::AddCargoVisual(EItemType ItemType)
{
	// 아이템 타입에 맞는 메시 선택
	UStaticMesh* ItemMesh = nullptr;
	switch (ItemType)
	{
	case EItemType::Ammo:
		ItemMesh = AmmoBoxMesh;
		break;
	case EItemType::Fuel:
		ItemMesh = FuelCanMesh;
		break;
	case EItemType::MedicalKit:
		ItemMesh = MedKitMesh;
		break;
	default:
		ItemMesh = AmmoBoxMesh; // 기본값
		break;
	}

	if (!ItemMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("No mesh assigned for item type %d"), (int32)ItemType);
		return;
	}

	// 새 StaticMeshComponent 생성
	UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(this);
	if (NewMeshComp)
	{
		NewMeshComp->SetStaticMesh(ItemMesh);
		NewMeshComp->SetupAttachment(CargoOrigin);
		NewMeshComp->RegisterComponent();

		// 위치 계산 (약간의 랜덤 회전으로 자연스럽게)
		FVector Position = CalculateCargoPosition(LoadedItemVisuals.Num());
		NewMeshComp->SetRelativeLocation(Position);

		// 약간의 랜덤 회전 (자연스럽게 쌓인 느낌)
		float RandomYaw = FMath::RandRange(-15.0f, 15.0f);
		float RandomPitch = FMath::RandRange(-3.0f, 3.0f);
		NewMeshComp->SetRelativeRotation(FRotator(RandomPitch, RandomYaw, 0.0f));

		// 충돌 비활성화 (시각적 용도만)
		NewMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 목록에 추가
		FLoadedItemVisual Visual;
		Visual.MeshComponent = NewMeshComp;
		Visual.ItemType = ItemType;
		LoadedItemVisuals.Add(Visual);

		UE_LOG(LogTemp, Log, TEXT("Cargo visual added: Type=%d, Position=(%s), Total=%d"),
			(int32)ItemType, *Position.ToString(), LoadedItemVisuals.Num());
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
