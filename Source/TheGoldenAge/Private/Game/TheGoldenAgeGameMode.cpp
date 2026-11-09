// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/TheGoldenAgeGameMode.h"

#include "Player/TheGoldenAgePlayerController.h"

#include "GameFramework/Controller.h"

ATheGoldenAgeGameMode::ATheGoldenAgeGameMode()
{
	PlayerControllerClass = ATheGoldenAgePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void ATheGoldenAgeGameMode::RestartPlayer(AController* NewPlayer)
{
	// UTheGoldenAgePlayerSync possesses the account pawn after login.
	(void)NewPlayer;
}
