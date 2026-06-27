#include "FPSProjectGameInstance.h"
#include "Engine/Engine.h"
#include "FPSNetworkManager.h"
#include "Kismet/KismetSystemLibrary.h"

void UFPSProjectGameInstance::ConnectToGameServer(const FString& IPAddress)
{
	if (NetworkManager)
	{
		NetworkManager->ConnectToGameServer(IPAddress, Port);
	}
}

void UFPSProjectGameInstance::DisconnectFromGameServer()
{
	if (NetworkManager)
	{
		NetworkManager->DisconnectFromGameServer(true);
	}
}

void UFPSProjectGameInstance::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UFPSProjectGameInstance::HandleRecvPackets()
{
	if (NetworkManager)
	{
		NetworkManager->HandleRecvPackets();
	}
}

void UFPSProjectGameInstance::SendPacket(SendBufferRef SendBuffer)
{
	if (NetworkManager)
	{
		NetworkManager->SendPacket(SendBuffer);
	}
}

void UFPSProjectGameInstance::SetPlayerNickname(const FString& Nickname)
{
	PlayerNickname = Nickname;
}

void UFPSProjectGameInstance::SendPacketStatic(SendBufferRef SendBuffer)
{
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
	if (World == nullptr)
	{
		World = GWorld;
	}

	if (World)
	{
		if (auto* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			GameInstance->SendPacket(SendBuffer);
		}
	}
}

bool UFPSProjectGameInstance::IsConnectedToGameServer() const
{
	return NetworkManager && NetworkManager->IsConnected();
}

bool UFPSProjectGameInstance::ShouldUseLocalInteractionFallback() const
{
	return !IsConnectedToGameServer();
}
