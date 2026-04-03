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
		UGameplayStatics::OpenLevel(World, TEXT("Map_helpme1"));
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

	/*if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GWorld->GetGameInstance()))
	{
		GameInstance->HandleSpawn(pkt);
	}*/

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

	//if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GWorld->GetGameInstance()))
	//{
	//	// TODO : 게임 종료? 로비로?
	//}

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

	/*if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GWorld->GetGameInstance()))
	{
		GameInstance->HandleSpawn(pkt);
	}*/

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

	/*if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GWorld->GetGameInstance()))
	{
		GameInstance->HandleDespawn(pkt);
	}*/

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

	/*if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GWorld->GetGameInstance()))
	{
		GameInstance->HandleMove(pkt);
	}*/

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
	//if (auto* GameInstance = Cast<UFPSProjectGameInstance>(GWorld->GetGameInstance()))
	//{
	//	//일단 서버를 돌아서 내 클라까지 패킷이 잘 도착했는지 로그로 확인!
	//	//UE_LOG(LogTemp, Warning, TEXT("======== [네트워크] S_EQUIP_WEAPON 수신 ========"));
	//	//UE_LOG(LogTemp, Warning, TEXT("누가 주웠는가(PlayerID) : %llu"), pkt.playerid());
	//	//UE_LOG(LogTemp, Warning, TEXT("무슨 아이템(ItemID) : %llu"), pkt.itemobjectid());
	//	//UE_LOG(LogTemp, Warning, TEXT("무기 타입(WeaponType) : %d"), pkt.weapontype());

	//	 // GameInstance에 함수를 만들어서 실제 모델링을 손에 붙이기
	//	 GameInstance->HandleEquipWeapon(pkt); 
	//}

	return true;
}
