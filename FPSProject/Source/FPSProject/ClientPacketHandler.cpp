#include "ClientPacketHandler.h"
#include "BufferReader.h"
#include "FPSProject.h"
#include "FPSProjectGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Async/Async.h"

UWorld* GetGameWorld()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
	}
	return GWorld;
}

PacketHandlerFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
	AsyncTask(ENamedThreads::GameThread, []()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				UGameplayStatics::OpenLevel(World, TEXT("sinwoo_test"));
			}
		});

	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	Protocol::S_ENTER_GAME* pktCopy = new Protocol::S_ENTER_GAME(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleSpawn(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_LEAVE_GAME(PacketSessionRef& session, Protocol::S_LEAVE_GAME& pkt)
{
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			// TODO : 게임 종료 혹은 로비 이동 로직
			// 예: UGameplayStatics::OpenLevel(World, TEXT("StartMap"));
		}
	}

	return true;
}

bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt)
{
	Protocol::S_SPAWN* pktCopy = new Protocol::S_SPAWN(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleSpawn(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
	Protocol::S_DESPAWN* pktCopy = new Protocol::S_DESPAWN(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleDespawn(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	Protocol::S_MOVE* pktCopy = new Protocol::S_MOVE(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleMove(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	auto Msg = pkt.msg();

	return true;
}

bool Handle_S_EQUIP_WEAPON(PacketSessionRef& session, Protocol::S_EQUIP_WEAPON& pkt)
{
	Protocol::S_EQUIP_WEAPON* pktCopy = new Protocol::S_EQUIP_WEAPON(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleEquipWeapon(*pktCopy);
				}
			}

			delete pktCopy;
		});
	
	return true;
}

bool Handle_S_SPAWN_ITEM(PacketSessionRef& session, Protocol::S_SPAWN_ITEM& pkt)
{
	Protocol::S_SPAWN_ITEM* pktCopy = new Protocol::S_SPAWN_ITEM(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleSpawnItem(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt)
{
	Protocol::S_FIRE* pktCopy = new Protocol::S_FIRE(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleFire(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}