#include "GameModes/FPSGameModeBase.h"
#include "Characters/FPSPlayerController.h"

void AFPSGameModeBase::StartPlay()
{
	Super::StartPlay();

	PlayerControllerClass = AFPSPlayerController::StaticClass();
}
