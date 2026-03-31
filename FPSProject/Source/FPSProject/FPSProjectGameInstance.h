// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Tickable.h"
#include "FPSProject.h"
#include "FPSProjectGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API UFPSProjectGameInstance : public UGameInstance, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Network")
	void ConnectToGameServer(const FString& IPAddress);

	UFUNCTION(BlueprintCallable)
	void DisconnectFromGameServer();

	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();

	void SendPacket(SendBufferRef SendBuffer);

public:
	void HandleSpawn(const Protocol::ObjectInfo& PlayerInfo, bool IsMine);
	void HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt);
	void HandleSpawn(const Protocol::S_SPAWN& SpawnPkt);

	void HandleDespawn(uint64 ObjectId);
	void HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt);

	void HandleMove(const Protocol::S_MOVE& MovePkt);

	void HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt);

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
};