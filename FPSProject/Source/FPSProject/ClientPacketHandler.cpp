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
				UGameplayStatics::OpenLevel(World, TEXT("Map_RL"));
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

bool Handle_S_ZOMBIE_ATTACK(PacketSessionRef& session, Protocol::S_ZOMBIE_ATTACK& pkt)
{
	Protocol::S_ZOMBIE_ATTACK* pktCopy = new Protocol::S_ZOMBIE_ATTACK(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleZombieAttack(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_ZOMBIE_HP(PacketSessionRef& session, Protocol::S_ZOMBIE_HP& pkt)
{
	Protocol::S_ZOMBIE_HP* pktCopy = new Protocol::S_ZOMBIE_HP(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleZombieHp(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_ZOMBIE_DIE(PacketSessionRef& session, Protocol::S_ZOMBIE_DIE& pkt)
{
	Protocol::S_ZOMBIE_DIE* pktCopy = new Protocol::S_ZOMBIE_DIE(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleZombieDie(*pktCopy);
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

bool Handle_S_ENTER_TRUCK(PacketSessionRef& session, Protocol::S_ENTER_TRUCK& pkt)
{
	Protocol::S_ENTER_TRUCK* pktCopy = new Protocol::S_ENTER_TRUCK(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleEnterTruck(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_EXIT_TRUCK(PacketSessionRef& session, Protocol::S_EXIT_TRUCK& pkt)
{
	Protocol::S_EXIT_TRUCK* pktCopy = new Protocol::S_EXIT_TRUCK(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleExitTruck(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_TRUCK_MOVE(PacketSessionRef& session, Protocol::S_TRUCK_MOVE& pkt)
{
	Protocol::S_TRUCK_MOVE* pktCopy = new Protocol::S_TRUCK_MOVE(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleTruckMove(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}

bool Handle_S_TOGGLE_DOOR(PacketSessionRef& session, Protocol::S_TOGGLE_DOOR& pkt)
{
	Protocol::S_TOGGLE_DOOR* pktCopy = new Protocol::S_TOGGLE_DOOR(pkt);

	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					GameInstance->HandleToggleDoor(*pktCopy);
				}
			}

			delete pktCopy;
		});

	return true;
}
