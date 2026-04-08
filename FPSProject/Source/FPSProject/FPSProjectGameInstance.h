// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Tickable.h"
#include "FPSProject.h"
#include "Enum.pb.h"
#include "FPSProjectGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API UFPSProjectGameInstance : public UGameInstance, public FTickableGameObject
{
	GENERATED_BODY()

	struct FPendingEquippedWeapon
	{
		uint64 ItemId = 0;
		int32 WeaponType = Protocol::WEAPON_TYPE_NONE;
	};

public:
	UFUNCTION(BlueprintCallable, Category = "Network")
	void ConnectToGameServer(const FString& IPAddress);

	UFUNCTION(BlueprintCallable)
	void DisconnectFromGameServer();
	// 서버에서 남이 나갔다고 알려줬을 때 실행할 함수
	void HandleLeaveGame(const Protocol::S_LEAVE_GAME& pkt);

	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();

	void SendPacket(SendBufferRef SendBuffer);
	static void SendPacketStatic(SendBufferRef SendBuffer);

public:
	void HandleSpawn(const Protocol::ObjectInfo& PlayerInfo, bool IsMine);
	void HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt);
	void HandleSpawn(const Protocol::S_SPAWN& SpawnPkt);

	void HandleDespawn(uint64 ObjectId);
	void HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt);

	void HandleMove(const Protocol::S_MOVE& MovePkt);

	void HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt);
	void HandleSpawnItem(const Protocol::S_SPAWN_ITEM& pkt);
	void HandleFire(const Protocol::S_FIRE& pkt);
	void ApplyEquippedWeapon(uint64 PlayerId, uint64 ItemId, int32 WeaponType);
	void RetryPendingWeapon(uint64 PlayerId);
	TSubclassOf<class AWeaponBase> ResolveWeaponClass(int32 WeaponType) const;

public:
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }

public:
	// GameServer
	class FSocket* Socket;
	//FString IpAddress = TEXT("127.0.0.1");
	int16 Port = 7777;
	TSharedPtr<class PacketSession> GameServerSession;

public:
	UPROPERTY(EditAnywhere, Category = "Network")
	TSubclassOf<class AFPSBaseCharacter> OtherPlayerClass;
	class AFPSBaseCharacter* MyPlayer;
	TMap<uint64, class AFPSBaseCharacter*> Players;

	// [추가] 바닥에 떨어진 아이템(총기 등)들을 ID로 관리하기 위한 맵
	UPROPERTY()
	TMap<uint64, AActor*> FieldItems;

	TMap<uint64, FPendingEquippedWeapon> PendingWeaponsByPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Class")
	TSubclassOf<class AWeaponBase> DefaultWeaponClass;	// 바닥에 떨어진 무기 스폰할 때 사용할 기본 무기 클래스

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Class")
	TSubclassOf<class AWeaponBase> DefaultEquippedWeaponClass;	// 손에 장착된 무기 스폰할 때 사용할 기본 무기 클래스
};