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

template<typename PacketType, typename HandlerType>
bool DispatchGameInstancePacket(const PacketType& Pkt, HandlerType Handler)
{
	PacketType PktCopy(Pkt);

	AsyncTask(ENamedThreads::GameThread, [PktCopy, Handler]()
		{
			UWorld* World = GetGameWorld();
			if (World)
			{
				if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
				{
					Handler(*GameInstance, PktCopy);
				}
			}
		});

	return true;
}

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
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ENTER_GAME& Pkt)
		{
			GameInstance.HandleSpawn(Pkt);
		});
}

bool Handle_S_LEAVE_GAME(PacketSessionRef& session, Protocol::S_LEAVE_GAME& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_LEAVE_GAME& Pkt)
		{
			GameInstance.HandleLeaveGame(Pkt);
		});
}

bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_SPAWN& Pkt)
		{
			GameInstance.HandleSpawn(Pkt);
		});
}

bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_DESPAWN& Pkt)
		{
			GameInstance.HandleDespawn(Pkt);
		});
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_MOVE& Pkt)
		{
			GameInstance.HandleMove(Pkt);
		});
}

bool Handle_S_ZOMBIE_ATTACK(PacketSessionRef& session, Protocol::S_ZOMBIE_ATTACK& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ZOMBIE_ATTACK& Pkt)
		{
			GameInstance.HandleZombieAttack(Pkt);
		});
}

bool Handle_S_ZOMBIE_HP(PacketSessionRef& session, Protocol::S_ZOMBIE_HP& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ZOMBIE_HP& Pkt)
		{
			GameInstance.HandleZombieHp(Pkt);
		});
}

bool Handle_S_ZOMBIE_DIE(PacketSessionRef& session, Protocol::S_ZOMBIE_DIE& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ZOMBIE_DIE& Pkt)
		{
			GameInstance.HandleZombieDie(Pkt);
		});
}

bool Handle_S_ZOMBIE_DISMEMBER(PacketSessionRef& session, Protocol::S_ZOMBIE_DISMEMBER& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ZOMBIE_DISMEMBER& Pkt)
		{
			GameInstance.HandleZombieDismember(Pkt);
		});
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	auto Msg = pkt.msg();

	return true;
}

bool Handle_S_EQUIP_WEAPON(PacketSessionRef& session, Protocol::S_EQUIP_WEAPON& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_EQUIP_WEAPON& Pkt)
		{
			GameInstance.HandleEquipWeapon(Pkt);
		});
}

bool Handle_S_SPAWN_ITEM(PacketSessionRef& session, Protocol::S_SPAWN_ITEM& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_SPAWN_ITEM& Pkt)
		{
			GameInstance.HandleSpawnItem(Pkt);
		});
}

bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_FIRE& Pkt)
		{
			GameInstance.HandleFire(Pkt);
		});
}

bool Handle_S_ENTER_TRUCK(PacketSessionRef& session, Protocol::S_ENTER_TRUCK& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ENTER_TRUCK& Pkt)
		{
			GameInstance.HandleEnterTruck(Pkt);
		});
}

bool Handle_S_EXIT_TRUCK(PacketSessionRef& session, Protocol::S_EXIT_TRUCK& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_EXIT_TRUCK& Pkt)
		{
			GameInstance.HandleExitTruck(Pkt);
		});
}

bool Handle_S_TRUCK_MOVE(PacketSessionRef& session, Protocol::S_TRUCK_MOVE& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_TRUCK_MOVE& Pkt)
		{
			GameInstance.HandleTruckMove(Pkt);
		});
}

bool Handle_S_LOAD_TRUCK_ITEM(PacketSessionRef& session, Protocol::S_LOAD_TRUCK_ITEM& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_LOAD_TRUCK_ITEM& Pkt)
		{
			GameInstance.HandleLoadTruckItem(Pkt);
		});
}

bool Handle_S_TOGGLE_DOOR(PacketSessionRef& session, Protocol::S_TOGGLE_DOOR& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_TOGGLE_DOOR& Pkt)
		{
			GameInstance.HandleToggleDoor(Pkt);
		});
}

bool Handle_S_ENTER_GAME_READY_COUNT(PacketSessionRef& session, Protocol::S_ENTER_GAME_READY_COUNT& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_ENTER_GAME_READY_COUNT& Pkt)
		{
			GameInstance.HandleEnterGameReadyCount(Pkt);
		});
}

bool Handle_S_STAGE_TIMER(PacketSessionRef& session, Protocol::S_STAGE_TIMER& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_STAGE_TIMER& Pkt)
		{
			GameInstance.HandleStageTimer(Pkt);
		});
}

bool Handle_S_STAGE1_ITEM_SEED(PacketSessionRef& session, Protocol::S_STAGE1_ITEM_SEED& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_STAGE1_ITEM_SEED& Pkt)
		{
			GameInstance.HandleStage1ItemSeed(Pkt);
		});
}

bool Handle_S_RESPAWN_LOOT_ITEM(PacketSessionRef& session, Protocol::S_RESPAWN_LOOT_ITEM& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_RESPAWN_LOOT_ITEM& Pkt)
		{
			GameInstance.HandleRespawnLootItem(Pkt);
		});
}

bool Handle_S_STAGE_TRANSITION(PacketSessionRef& session, Protocol::S_STAGE_TRANSITION& pkt)
{
	return DispatchGameInstancePacket(pkt, [](UFPSProjectGameInstance& GameInstance, const Protocol::S_STAGE_TRANSITION& Pkt)
		{
			GameInstance.HandleStageTransition(Pkt);
		});
}