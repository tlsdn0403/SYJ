#include "ClientPacketHandler.h"
#include "BufferReader.h"
#include "FPSProject.h"
#include "FPSProjectGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

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
	UWorld* World = GetGameWorld();
	if (World)
	{
		UGameplayStatics::OpenLevel(World, TEXT("sinwoo_test"));
	}

	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			// 입장 성공 시 스폰 처리
			GameInstance->HandleSpawn(pkt);
		}
	}

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
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			GameInstance->HandleSpawn(pkt);
		}
	}

	return true;
}

bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			GameInstance->HandleDespawn(pkt);
		}
	}

	return true;
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			GameInstance->HandleMove(pkt);
		}
	}

	return true;
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	auto Msg = pkt.msg();

	return true;
}

bool Handle_S_EQUIP_WEAPON(PacketSessionRef& session, Protocol::S_EQUIP_WEAPON& pkt)
{
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			GameInstance->HandleEquipWeapon(pkt);
		}
	}
	
	return true;
}

bool Handle_S_SPAWN_ITEM(PacketSessionRef& session, Protocol::S_SPAWN_ITEM& pkt)
{
	UWorld* World = GetGameWorld();
	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			// GameInstance에게 스폰 처리를 맡김
			GameInstance->HandleSpawnItem(pkt);
		}
	}
	return true;
}

bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt)
{
	// 1. 메인 스레드에서 안전하게 쓰기 위해 패킷 복사본 만들기
	Protocol::S_FIRE* pktCopy = new Protocol::S_FIRE(pkt);

	// 2. 메인 스레드(GameThread)에게 작업 넘기기
	AsyncTask(ENamedThreads::GameThread, [pktCopy]()
		{
			// 3. 현재 켜져 있는 월드와 게임 인스턴스 찾기
			if (GEngine && GEngine->GetWorldContexts().Num() > 0)
			{
				UWorld* World = GEngine->GetWorldContexts()[0].World();
				if (World)
				{
					UFPSProjectGameInstance* GI = Cast<UFPSProjectGameInstance>(World->GetGameInstance());
					if (GI)
					{
						// 4. 이전에 우리가 GameInstance에 만들어둔 진짜 실행 함수 호출!
						GI->HandleFire(*pktCopy);
					}
				}
			}

			// 5. 다 썼으면 메모리 누수 안 나게 삭제!
			delete pktCopy;
		});

	return true;
}